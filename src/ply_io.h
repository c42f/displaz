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

#ifndef DISPLAZ_PLY_IO_INCLUDED
#define DISPLAZ_PLY_IO_INCLUDED

#include <vector>

#include <rply/rply.h>

#include <QRegExp>

#include "pointfield.h"
#include "util.h"
#include "logger.h"


/// Find and return "vertex" element from ply file.
///
/// Return NULL when no vertex element is present.
p_ply_element findVertexElement(p_ply ply, size_t& npoints);


/// Load properties of ply "vertex" element
///
/// Properties of the vertex element are mapped into displaz point fields as
/// follows:
///
/// The following standard properties are recognized:
///
///   (x,y,z)          -> position
///   (nx,ny,nz)       -> normal
///   (red,green,blue) -> color
///   (r,g,b)          -> color
///
/// Other attributes are processed as follows.
///
///   prop[0], ..., prop[N]   -> N-element array per point with name "prop"
///   propx, propy, propz     -> 3-element vector per point with name "prop"
///   prop_x, prop_y, prop_z  -> 3-element vector per point with name "prop"
///
bool loadPlyVertexProperties(QString fileName, p_ply ply, p_ply_element vertexElement,
                             std::vector<PointFieldData>& fields, V3d& offset,
                             size_t npoints);


#endif // DISPLAZ_PLY_IO_INCLUDED
