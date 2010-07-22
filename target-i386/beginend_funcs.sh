#! /bin/bash
# Copyright 2008 (C) Intel Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISiNG FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
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
