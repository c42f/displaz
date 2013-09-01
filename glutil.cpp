// Copyright (C) 2012, Chris J. Foster and the other authors and contributors
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name of the software's owners nor the names of its
//   contributors may be used to endorse or promote products derived from this
//   software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// (This is the BSD 3-clause license)

#include "glutil.h"


void drawBoundingBox(const Imath::Box3f& bbox,
                     const Imath::C3f& col)
{
    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor3f(col.x, col.y, col.z);
    glLineWidth(1);
    GLfloat verts[] = {
        bbox.min.x, bbox.min.y, bbox.min.z,
        bbox.min.x, bbox.max.y, bbox.min.z,
        bbox.max.x, bbox.max.y, bbox.min.z,
        bbox.max.x, bbox.min.y, bbox.min.z,
        bbox.min.x, bbox.min.y, bbox.max.z,
        bbox.min.x, bbox.max.y, bbox.max.z,
        bbox.max.x, bbox.max.y, bbox.max.z,
        bbox.max.x, bbox.min.y, bbox.max.z
    };
    unsigned char inds[] = {
        // rows: bottom, sides, top
        0,1, 1,2, 2,3, 3,0,
        0,4, 1,5, 2,6, 3,7,
        4,5, 5,6, 6,7, 7,4
    };
    // TODO: Use shaders here
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, verts);
    glDrawElements(GL_LINES, sizeof(inds)/sizeof(inds[0]),
                   GL_UNSIGNED_BYTE, inds);
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisable(GL_BLEND);
    glDisable(GL_LINE_SMOOTH);
}

