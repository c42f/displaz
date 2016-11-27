// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#ifndef DISPLAZ_PLY_IO_INCLUDED
#define DISPLAZ_PLY_IO_INCLUDED

#include <vector>

#include <rply/rply.h>

#include "typespec.h"
#include "geomfield.h"
#include "util.h"


/// Callback handler for reading ply data
class PlyLoader
{
    public:
        PlyLoader(const std::string& fileName);

        /// Get a list of all comments in the ply header
        std::vector<std::string> comments();

        /// Create a set of `GeomField`s containing all the per-vertex
        /// information contained in the ply file.
        ///
        /// This attempts to extract vertex information in both supported
        /// formats using a combination of hookPlyVertexPropertyNames() and
        /// hookPrefixedDisplazFields()
        void hookVertexFields(std::vector<GeomField>& fields);

        /// Connect standard ply vertex properties of a given element to
        /// displaz `GeomField`s for loading.
        ///
        /// Having all fields as a jumbled mix of properties on the "vertex"
        /// element seems to be a common(?) way to store point clouds in ply
        /// format.  Properties of the vertex element are mapped into displaz
        /// point fields as follows:
        ///
        /// The following standard properties are recognized:
        ///
        ///   (x,y,z)          -> position
        ///   (nx,ny,nz)       -> normal
        ///   (red,green,blue) -> color
        ///   (r,g,b)          -> color
        ///   (u,v)            -> texcoord
        ///
        /// Other attributes are processed as follows.
        ///
        ///   prop[0], ..., prop[N]   -> N-element array per point with name "prop"
        ///   propx, propy, propz     -> 3-element vector per point with name "prop"
        ///   prop_x, prop_y, prop_z  -> 3-element vector per point with name "prop"
        ///
        void hookPlyVertexPropertyNames(const std::string& elementName,
                                        std::vector<GeomField>& fields);

        /// Connect elements prefixed with `elementPrefix` to displaz
        /// `GeomFields`s for loading.
        ///
        /// The displaz native field format has a fairly rigid and specific
        /// layout which is designed to be unambiguous, and to be simple and
        /// efficient for binary serialization.  Each ply element should be
        /// named as "prefix_<field_name>"; the "prefix_" is stripped off by
        /// displaz during parsing.  The semantics of the field are determined
        /// by the names of the first property: "x" implies a vector, "r" a
        /// color, and "0" an array.
        ///
        /// Example header elements for a "vertex_" prefix:
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
        void hookPrefixedDisplazFields(const std::string& elementPrefix,
                                       GeomFields& fields);

        /// Read the ply file, filling all previously hooked fields with data.
        bool read();

    private:
        p_ply ply;

        class PlyFieldLoader;

        std::vector<std::unique_ptr<PlyFieldLoader>> m_loaders
};


#endif // DISPLAZ_PLY_IO_INCLUDED
