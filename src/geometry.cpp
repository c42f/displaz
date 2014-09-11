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

#include "geometry.h"
#include "mesh.h"
#include "pointarray.h"

#include <rply/rply.h>

/// Determine whether a ply file has mesh or line segment elements
///
/// (If not, assume it's a point cloud.)
static bool plyHasMesh(QString fileName)
{
    std::unique_ptr<t_ply_, int(*)(p_ply)> ply(
            ply_open(fileName.toUtf8().constData(), NULL, 0, NULL), ply_close);
    if (!ply || !ply_read_header(ply.get()))
        return false;
    for (p_ply_element elem = ply_get_next_element(ply.get(), NULL);
         elem != NULL; elem = ply_get_next_element(ply.get(), elem))
    {
        const char* name = 0;
        long ninstances = 0;
        if (!ply_get_element_info(elem, &name, &ninstances))
            continue;
        if (strcmp(name, "face") == 0 || strcmp(name, "triangle") == 0 ||
            strcmp(name, "edge") == 0 || strcmp(name, "hullxy") == 0)
        {
            return true;
        }
    }
    return false;
}


//------------------------------------------------------------------------------
Geometry::Geometry()
    : m_fileName(),
    m_offset(0,0,0),
    m_centroid(0,0,0),
    m_bbox()
{ }


std::shared_ptr<Geometry> Geometry::create(QString fileName)
{
    if (fileName.endsWith(".ply") && plyHasMesh(fileName))
        return std::shared_ptr<Geometry>(new TriMesh());
    else
        return std::shared_ptr<Geometry>(new PointArray());
}


bool Geometry::reloadFile(size_t maxVertexCount)
{
    return loadFile(m_fileName, maxVertexCount);
}

