// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#pragma once

#include <algorithm>

#include "glutil.h"

//------------------------------------------------------------------------------
/// Class to determine whether geometry can be culled by being entirely outside
/// the OpenGL frustum volume.
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
            const M44d mvp = transState.modelViewMatrix * transState.projMatrix;
            const V3f c1 = V3f(mvp[0][0], mvp[1][0], mvp[2][0]); const float d1 = mvp[3][0];
            const V3f c2 = V3f(mvp[0][1], mvp[1][1], mvp[2][1]); const float d2 = mvp[3][1];
            const V3f c3 = V3f(mvp[0][2], mvp[1][2], mvp[2][2]); const float d3 = mvp[3][2];
            const V3f c4 = V3f(mvp[0][3], mvp[1][3], mvp[2][3]); const float d4 = mvp[3][3];
            normal[0] = c4 + c1;  distance[0] = d4 + d1;
            normal[1] = c4 - c1;  distance[1] = d4 - d1;
            normal[2] = c4 + c2;  distance[2] = d4 + d2;
            normal[3] = c4 - c2;  distance[3] = d4 - d2;
            normal[4] = c4 + c3;  distance[4] = d4 + d3;
            normal[5] = c4 - c3;  distance[5] = d4 - d3;
        }

        /// Determine whether `box` lies entirely outside the clipping volume
        /// and can therefore be discarded
        bool canCull(const Imath::Box3f& box) const
        {
            // Box corners
            const V3f points[8] = {
                V3f(box.min.x, box.min.y, box.min.z),
                V3f(box.min.x, box.max.y, box.min.z),
                V3f(box.max.x, box.max.y, box.min.z),
                V3f(box.max.x, box.min.y, box.min.z),
                V3f(box.min.x, box.min.y, box.max.z),
                V3f(box.min.x, box.max.y, box.max.z),
                V3f(box.max.x, box.max.y, box.max.z),
                V3f(box.max.x, box.min.y, box.max.z),
            };

            // We can cull if all box vertices are outside any clipping plane.
            // This underestimates possible culling for some corner cases,
            // but is far simpler than the alternatives.
            for (int j = 0; j < 6; ++j)
            {
                if (std::all_of(std::begin(points), std::end(points), [&](const auto& p)
                    { return normal[j].dot(p) + distance[j] < 0.0; }))
                {
                    return true;
                }
            }
            return false;
        }

    private:
        // Plane equation coeffs:  normal[j].dot(v) + distance[j] == 0
        Imath::V3f normal[6];
        float distance[6];
};
