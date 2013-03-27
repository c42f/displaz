#include <string>
#include <vector>

class QGLShaderProgram;

class TriMesh
{
    public:
        /// Read mesh from the given file
        bool readFile(const std::string& fileName);

        /// Draw mesh using current OpenGL context
        void draw(QGLShaderProgram& prog);

    private:
        std::vector<double> m_verts;
        std::vector<float> m_normals;
        std::vector<unsigned int> m_faces;
};
