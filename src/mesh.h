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

#ifndef DISPLAZ_MESH_H_INCLUDED
#define DISPLAZ_MESH_H_INCLUDED

#include <vector>
#include <memory>

#include <QtCore/QString>
#include <QtCore/QMetaType>

#include "geometry.h"

class QGLShaderProgram;


/// Mesh of triangles or line segment edges, in indexed format for OpenGL
class TriMesh : public Geometry
{
    public:
        virtual bool loadFile(QString fileName, size_t maxVertexCount);

        virtual void drawFaces(QGLShaderProgram& prog) const;
        virtual void drawEdges(QGLShaderProgram& prog) const;

        virtual size_t pointCount() const { return 0; }

        virtual size_t simplifiedPointCount(const V3d& cameraPos,
                                            bool incrementalDraw) const { return 0; }

        virtual V3d pickVertex(const V3d& cameraPos,
                               const V3d& rayOrigin, const V3d& rayDirection,
                               double longitudinalScale, double* distance = 0) const;

    private:

        static void makeSmoothNormals(std::vector<float>& normals,
                                      const std::vector<float>& verts,
                                      const std::vector<unsigned int>& faces);

        static void makeEdges(std::vector<unsigned int>& edges,
                              const std::vector<unsigned int>& faces);

        /// xyz triples
        std::vector<float> m_verts;
        /// Per-vertex color
        std::vector<float> m_colors;
        /// Per-vertex normal
        std::vector<float> m_normals;
        /// triples of indices into vertex array
        std::vector<unsigned int> m_faces;
        std::vector<unsigned int> m_edges;
};


#endif // DISPLAZ_MESH_H_INCLUDED
