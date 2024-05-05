// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#ifndef DISPLAZ_PLY_IO_INCLUDED
#define DISPLAZ_PLY_IO_INCLUDED

#include <vector>

#include <rply/rply.h>

#include <QRegExp>

#include "typespec.h"
#include "GeomField.h"
#include "util.h"


/// Find and return "vertex" element from ply file.
///
/// Return NULL when no vertex element is present.
p_ply_element findVertexElement(p_ply ply, size_t& npoints);


/// Load properties of ply "vertex" element
///
/// Having all fields as a jumbled mix of properties on the "vertex" element is
/// a common way to store point clouds in ply format.  Properties of the vertex
/// element are mapped into displaz point fields as follows:
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
                             std::vector<GeomField>& fields, V3d& offset,
                             size_t npoints);

/// Load native displaz ply format
///
/// This has a fairly rigid and specific layout which is designed to be
/// unambiguous, and to be simple and efficient for binary serialization.
/// Each ply element should be named as "vertex_<field_name>"; the vertex_
/// prefix is stripped off by displaz during parsing.  The semantics of the
/// field are determined by the names of the first property: "x" implies a
/// vector, "r" a color, and "0" an array.
///
/// Example header elements:
///
///   element vertex_position 20
///   property float x
///   property float y
///   property float z
///   element vertex_mycolor 20
///   property uint8 r
///   property uint8 g
///   property uint8 b
///   element vertex_myarray 20
///   property float 0
///   property float 1
///
/// will be parsed as
///
///   vector float[3] position
///   color uint8_t[3] mycolor
///   array float[2] myarray
///
///
/// Parameters:
///   fileName - ply file name
///   ply - open ply file
///   fields - returned point fields.
///   offset - offset to be applied to position field
///   npoints - total number of points
bool loadDisplazNativePly(QString fileName, p_ply ply,
                          std::vector<GeomField>& fields, V3d& offset,
                          size_t& npoints);


/// Logging callback, logging all rply errors to g_logger
void logRplyError(p_ply ply, const char* message);


#endif // DISPLAZ_PLY_IO_INCLUDED
