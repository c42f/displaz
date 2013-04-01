#include <string>
#include <vector>

#include "util.h"

class QGLShaderProgram;

class TriMesh
{
    public:
        TriMesh() : m_offset(0) {}
        /// Read mesh from the given file
        bool readFile(const std::string& fileName);

        /// Draw mesh using current OpenGL context
        void drawFaces(QGLShaderProgram& prog);
        void drawEdges(QGLShaderProgram& prog);

        const V3d& offset() const { return m_offset; }

    private:
        static void makeSmoothNormals(std::vector<float>& normals,
                                      const std::vector<float>& verts,
                                      const std::vector<unsigned int>& faces);

        static void makeEdges(std::vector<unsigned int>& edges,
                              const std::vector<unsigned int>& faces);

        V3d m_offset;
        std::vector<float> m_verts;
        std::vector<float> m_normals;
        std::vector<unsigned int> m_faces;
        std::vector<unsigned int> m_edges;
};
