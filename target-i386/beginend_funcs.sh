#!/bin/bash
# Copyright 2008 (C) Intel Corporation
#
# NOTE: This REQUIRES bash, not sh.
#
# echo names of functions that are legal between a glBegin and glEnd pair.
echo -e MAGIC_MACRO\(glVertex{2,3,4}{s,i,f,d}{,v}\)\\n
echo -e MAGIC_MACRO\(glTexCoord{1,2,3,4}{s,i,f,d}{,v}\)\\n
echo -e MAGIC_MACRO\(glMultiTexCoord{1,2,3,4}{s,i,f,d}{,v}\)\\n
echo -e MAGIC_MACRO\(glNormal3{b,s,i,f,d}{,v}\)\\n
echo -e MAGIC_MACRO\(glFogCoord{f,d}{,v}\)\\n
echo -e MAGIC_MACRO\(glColor{3,4}{b,s,i,f,d,ub,us,ui}{,v}\)\\n
echo -e MAGIC_MACRO\(glSecondaryColor3{b,s,i,f,d,ub,us,ui}{,v}\)\\n
echo -e MAGIC_MACRO\(glIndex{s,i,f,d,ub}{,v}\)\\n
echo -e MAGIC_MACRO\(glVertexAttrib{1,2,3,4}{s,f,d}{,v}\)\\n
echo -e MAGIC_MACRO\(glVertexAttrib4{b,i,ub,us,ui}v\)\\n
echo -e MAGIC_MACRO\(glVertexAttrib4Nub\)\\n
echo -e MAGIC_MACRO\(glVertexAttrib4N{b,s,i,ub,us,ui}v\)\\n
echo -e MAGIC_MACRO\(glArrayElement\)\\n
echo -e MAGIC_MACRO\(glEvalCoord{1,2}{f,d}{,v}\)\\n
echo -e MAGIC_MACRO\(glEvalPoint{1,2}\)\\n
echo -e MAGIC_MACRO\(glMaterial{i,f}{,v}\)\\n
echo -e MAGIC_MACRO\(glCallList\)\\n
echo -e MAGIC_MACRO\(glCallLists\)\\n
echo -e MAGIC_MACRO\(glEdgeFlag{,v}\)\\n
