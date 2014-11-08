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

//------------------------------------------------------------------------------
/// Class to determine whether geometry can be culled by being entirely outside
/// the OpenGL frustum clipping volume.
///
/// Clipping happens in "clip coordinates" - the coordinate system after
/// multiplication by the 4x4 model view projection matrix, but before doing
/// the perspective divide.  A vector (xc yc zc wc) in clip coordinates is
/// inside the non-clipped area when the following are satisfied:
///
///   -wc <= xc <= wc
///   -wc <= yc <= wc
///   -wc <= zc <= wc
///
class ClipBox
{
    public:
        ClipBox(const TransformState& transState)
        {
            // Extract plane equations for frustum clipping.  The clipping
            // plane equations in model space coordinates can be represented as
            // linear combinations of the columns of the model view projection
            // matrix.  [Normally these would be rows of the MVP matrix in
            // OpenGL, but Imath multiplies vectors on the left :-( ]
            M44d mvp = transState.modelViewMatrix * transState.projMatrix;
            V3f c1 = V3f(mvp[0][0], mvp[1][0], mvp[2][0]); float d1 = mvp[3][0];
            V3f c2 = V3f(mvp[0][1], mvp[1][1], mvp[2][1]); float d2 = mvp[3][1];
            V3f c3 = V3f(mvp[0][2], mvp[1][2], mvp[2][2]); float d3 = mvp[3][2];
            V3f c4 = V3f(mvp[0][3], mvp[1][3], mvp[2][3]); float d4 = mvp[3][3];
            normal[0] = c4+c1;   distance[0] = d4+d1;
            normal[1] = c4-c1;   distance[1] = d4-d1;
            normal[2] = c4+c2;   distance[2] = d4+d2;
            normal[3] = c4-c2;   distance[3] = d4-d2;
            normal[4] = c4+c3;   distance[4] = d4+d3;
            normal[5] = c4-c3;   distance[5] = d4-d3;
        }

        /// Determine whether `box` lies entirely outside the clipping volume
        /// and can therefore be discarded
        bool canCull(const Imath::Box3f& box) const
        {
            // Corners of bounding box
            V3f points[8] = {
                V3f(box.min.x, box.min.y, box.min.z),
                V3f(box.min.x, box.max.y, box.min.z),
                V3f(box.max.x, box.max.y, box.min.z),
                V3f(box.max.x, box.min.y, box.min.z),
                V3f(box.min.x, box.min.y, box.max.z),
                V3f(box.min.x, box.max.y, box.max.z),
                V3f(box.max.x, box.max.y, box.max.z),
                V3f(box.max.x, box.min.y, box.max.z),
            };
            for (int j = 0; j < 6; ++j)
            {
                // We can clip if all bounding box vertices are outside of jth
                // clipping plane.  This underestimates possible clipping for
                // some corner cases, but is far simpler than the alternatives.
                bool doClip = true;
                for (int i = 0; i < 8; ++i)
                    doClip &= normal[j].dot(points[i]) + distance[j] < 0;
                if (doClip)
                    return true;
            }
            return false;
        }

    private:
        // Plane equation coeffs:  normal[j].dot(v) + distance[j] == 0
        Imath::V3f normal[6];
        float distance[6];
};


