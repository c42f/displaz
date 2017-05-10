// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#ifndef DISPLAZ_MESH_H_INCLUDED
#define DISPLAZ_MESH_H_INCLUDED

#include <vector>
#include <memory>

#include <QString>
#include <QMetaType>

#include "Geometry.h"
#include "glutil.h"

class QGLShaderProgram;


/// Mesh of triangles or line segment edges, in indexed format for OpenGL
class TriMesh : public Geometry
{
    public:
        virtual bool loadFile(QString fileName, size_t maxVertexCount);

        virtual void draw(const TransformState& transState, double quality) const;

        virtual void initializeGL();

        virtual void drawFaces(QGLShaderProgram& prog,
                               const TransformState& transState) const;
        virtual void drawEdges(QGLShaderProgram& prog,
                               const TransformState& transState) const;

        virtual size_t pointCount() const { return 0; }

        virtual void estimateCost(const TransformState& transState,
                                  bool incrementalDraw, const double* qualities,
                                  DrawCount* drawCounts, int numEstimates) const;

        virtual bool pickVertex(const V3d& cameraPos,
                                const EllipticalDist& distFunc,
                                V3d& pickedVertex,
                                double* distance = 0,
                                std::string* info = 0) const;

    private:
        void initializeVertexGL(const char * vertArrayName, const std::vector<unsigned int>& elementInds,
                                const char * positionAttrName, const char * normAttrName,
                                const char * colorAttrName, const char* texCoordAttrName);

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
        /// Per-vertex texture coordinates
        std::vector<float> m_texcoords;
        /// Optional texture
        std::unique_ptr<Texture> m_texture;
        /// triples of indices into vertex array
        std::vector<unsigned int> m_triangles;
        std::vector<unsigned int> m_edges;
};


#endif // DISPLAZ_MESH_H_INCLUDED
