/* This is a modified and simplified version of original ghash.c */

/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/.
 */


#include <stdlib.h>

#include "ghash.h"

#define HASH_TABLE_MIN_SIZE 11
#define HASH_TABLE_MAX_SIZE 13845163

#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))


typedef struct _SimpleHashNode      SimpleHashNode;

struct _SimpleHashNode
{
  int        key;
  void*      value;
  SimpleHashNode *next;
};

struct _SimpleHashTable
{
  int             size;
  int             nnodes;
  SimpleHashNode      **nodes;
  SimpleDestroyNotify   value_destroy_func;
};

static const unsigned int simple_primes[] =
{
  11,
  19,
  37,
  73,
  109,
  163,
  251,
  367,
  557,
  823,
  1237,
  1861,
  2777,
  4177,
  6247,
  9371,
  14057,
  21089,
  31627,
  47431,
  71143,
  106721,
  160073,
  240101,
  360163,
  540217,
  810343,
  1215497,
  1823231,
  2734867,
  4102283,
  6153409,
  9230113,
  13845163,
};

static const unsigned int simple_nprimes = sizeof (simple_primes) / sizeof (simple_primes[0]);

unsigned int simple_spaced_primes_closest (unsigned int num)
{
  int i;

  for (i = 0; i < simple_nprimes; i++)
    if (simple_primes[i] > num)
      return simple_primes[i];

  return simple_primes[simple_nprimes - 1];
}

#define HASH_TABLE_RESIZE(hash_table)				\
   do {						\
     if ((hash_table->size >= 3 * hash_table->nnodes &&	        \
	  hash_table->size > HASH_TABLE_MIN_SIZE) ||		\
	 (3 * hash_table->size <= hash_table->nnodes &&	        \
	  hash_table->size < HASH_TABLE_MAX_SIZE))		\
	   simple_hash_table_resize (hash_table);			\
    } while(0)

static void		simple_hash_table_resize	  (SimpleHashTable	  *hash_table);
static SimpleHashNode**	simple_hash_table_lookup_node  (SimpleHashTable     *hash_table,
                                                   int   key);
static SimpleHashNode*	simple_hash_node_new		  (int	   key,
                                           void*        value);
static void		simple_hash_nodes_destroy	  (SimpleHashNode	  *hash_node,
                                           SimpleDestroyNotify   value_destroy_func);


#define alloc0(type, n) (type*)calloc(n, sizeof(type))

SimpleHashTable*
simple_hash_table_new (SimpleDestroyNotify  value_destroy_func)
{
  SimpleHashTable *hash_table;

  hash_table                     = alloc0(SimpleHashTable, 1);
  hash_table->size               = HASH_TABLE_MIN_SIZE;
  hash_table->nnodes             = 0;
  hash_table->value_destroy_func = value_destroy_func;
  hash_table->nodes              = alloc0 (SimpleHashNode*, hash_table->size);

  return hash_table;
}

SimpleHashTable* simple_hash_table_clone(SimpleHashTable *hash_table,
                                       SimpleCloneValue clone_value_func)
{
  SimpleHashTable *hash_table_new;
  SimpleHashNode *new_node;
  SimpleHashNode *node;
  int i;

  hash_table_new                     = alloc0 (SimpleHashTable, 1);
  hash_table_new->size               = hash_table->size;
  hash_table_new->nnodes             = hash_table->nnodes;
  hash_table_new->value_destroy_func = hash_table->value_destroy_func;
  hash_table_new->nodes              = alloc0 (SimpleHashNode*, hash_table_new->size);
  for (i = 0; i < hash_table->size; i++)
  {
    node = hash_table->nodes[i];
    while(node)
    {
      SimpleHashNode *next = hash_table_new->nodes[i];
      new_node = simple_hash_node_new(node->key,
                                     (clone_value_func)? clone_value_func(node->value) : node->value);
      new_node->next = next;
      hash_table_new->nodes[i] = new_node;
      node = node->next;
    }
  }
  return hash_table_new;
}

void
simple_hash_table_destroy (SimpleHashTable *hash_table)
{
  int i;

  for (i = 0; i < hash_table->size; i++)
  {
    simple_hash_nodes_destroy (hash_table->nodes[i],
                              hash_table->value_destroy_func);
    hash_table->nodes[i] = NULL;
  }
  free (hash_table->nodes);
  free (hash_table);
}

static inline SimpleHashNode**
simple_hash_table_lookup_node (SimpleHashTable	*hash_table,
                              int key)
{
  SimpleHashNode **node;

  node = &hash_table->nodes[(unsigned int)key % hash_table->size];
  while (*node && (*node)->key != key)
    node = &(*node)->next;

  return node;
}

void*
simple_hash_table_lookup (SimpleHashTable	  *hash_table, int key)
{
  SimpleHashNode *node;

  node = *simple_hash_table_lookup_node (hash_table, key);

  return node ? node->value : NULL;
}

void**
simple_hash_table_lookup_pointer (SimpleHashTable	  *hash_table, int key)
{
  SimpleHashNode *node;

  node = *simple_hash_table_lookup_node (hash_table, key);

  return node ? &node->value : NULL;
}


void
simple_hash_table_insert (SimpleHashTable *hash_table,
                         int	 key,
                         void*	 value)
{
  SimpleHashNode **node;

  node = simple_hash_table_lookup_node (hash_table, key);

  if (*node)
    {
      /* do not reset node->key in this place, keeping
       * the old key is the intended behaviour.
       * simple_hash_table_replace() can be used instead.
       */
      if (hash_table->value_destroy_func)
        hash_table->value_destroy_func ((*node)->value);

      (*node)->value = value;
    }
  else
    {
      *node = simple_hash_node_new (key, value);
      hash_table->nnodes++;
      HASH_TABLE_RESIZE (hash_table);
    }
}
int
simple_hash_table_remove (SimpleHashTable	   *hash_table,
                         int  key)
{
  SimpleHashNode **node, *dest;

  node = simple_hash_table_lookup_node (hash_table, key);
  if (*node)
  {
    dest = *node;
    (*node) = dest->next;
    if (hash_table->value_destroy_func)
      hash_table->value_destroy_func (dest->value);
    free (dest);
    hash_table->nnodes--;

    HASH_TABLE_RESIZE (hash_table);

    return 1;
  }

  return 0;
}


void
simple_hash_table_foreach (SimpleHashTable *hash_table,
                          SimpleHFunc	  func,
                          void*	  user_data)
{
  SimpleHashNode *node;
  int i;

  for (i = 0; i < hash_table->size; i++)
    for (node = hash_table->nodes[i]; node; node = node->next)
      (* func) (node->key, node->value, user_data);
}

unsigned int
simple_hash_table_size (SimpleHashTable *hash_table)
{
  return hash_table->nnodes;
}

static void
simple_hash_table_resize (SimpleHashTable *hash_table)
{
  SimpleHashNode **new_nodes;
  SimpleHashNode *node;
  SimpleHashNode *next;
  unsigned int hash_val;
  int new_size;
  int i;

  new_size = simple_spaced_primes_closest (hash_table->nnodes);
  new_size = CLAMP (new_size, HASH_TABLE_MIN_SIZE, HASH_TABLE_MAX_SIZE);

  new_nodes = alloc0 (SimpleHashNode*, new_size);

  for (i = 0; i < hash_table->size; i++)
    for (node = hash_table->nodes[i]; node; node = next)
    {
      next = node->next;

      hash_val = (unsigned int)(node->key) % new_size;

      node->next = new_nodes[hash_val];
      new_nodes[hash_val] = node;
    }

  free (hash_table->nodes);
  hash_table->nodes = new_nodes;
  hash_table->size = new_size;
}

static SimpleHashNode*
simple_hash_node_new (int key,
                     void* value)
{
  SimpleHashNode *hash_node = alloc0 (SimpleHashNode, 1);

  hash_node->key = key;
  hash_node->value = value;
  hash_node->next = NULL;

  return hash_node;
}

static void
simple_hash_nodes_destroy (SimpleHashNode *hash_node,
                          SimpleDestroyNotify  value_destroy_func)
{
  while (hash_node)
  {
    SimpleHashNode *next = hash_node->next;
    if (value_destroy_func)
      value_destroy_func (hash_node->value);
    free (hash_node);
    hash_node = next;
  }
}
