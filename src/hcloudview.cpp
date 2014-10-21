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

#include "hcloudview.h"

#include "hcloud.h"
#include "glutil.h"
#include "qtlogger.h"
#include "shader.h"
#include "streampagecache.h"
#include "util.h"

#include <queue>

//------------------------------------------------------------------------------
struct HCloudNode
{
    HCloudNode* children[8]; ///< Child nodes - order (x + 2*y + 4*z)
    Imath::Box3f bbox;       ///< Bounding box of node

    uint64_t dataOffset;
    uint32_t numOccupiedVoxels;
    uint32_t numLeafPoints;
    bool isLeaf;

    // List of non-empty voxels inside the node
    std::unique_ptr<float[]> position;
    std::unique_ptr<float[]> intensity;
    std::unique_ptr<float[]> coverage;

    HCloudNode(const Box3f& bbox)
        : bbox(bbox),
        dataOffset(0), numOccupiedVoxels(0), numLeafPoints(0), isLeaf(false)
    {
        for (int i = 0; i < 8; ++i)
            children[i] = 0;
    }

    ~HCloudNode()
    {
        for (int i = 0; i < 8; ++i)
            delete children[i];
    }

    bool isCached() const { return position.get() != 0; }

    float radius() const { return bbox.max.x - bbox.min.x; }

    /// Allocate arrays for storing point data
    ///
    /// (TODO: need some sort of LRU cache)
    void allocateArrays()
    {
        position.reset(new float[3*numOccupiedVoxels]);
        intensity.reset(new float[numOccupiedVoxels]);
        coverage.reset(new float[numOccupiedVoxels]);
    }

    void freeArrays()
    {
        position.reset();
        intensity.reset();
        coverage.reset();
    }
};


static HCloudNode* readHCloudIndex(std::istream& in, const Box3f& bbox)
{
    HCloudNode* node = new HCloudNode(bbox);
    uint8_t childNodeMask   = readLE<uint8_t>(in);
    node->dataOffset        = readLE<uint64_t>(in);
    node->numOccupiedVoxels = readLE<uint32_t>(in);
    node->numLeafPoints     = readLE<uint32_t>(in);
    V3f center = bbox.center();
    node->isLeaf = (childNodeMask == 0);
    for (int i = 0; i < 8; ++i)
    {
        if (!((childNodeMask >> i) & 1))
            continue;
        Box3f b = bbox;
        if (i % 2 == 0)
            b.max.x = center.x;
        else
            b.min.x = center.x;
        if ((i/2) % 2 == 0)
            b.max.y = center.y;
        else
            b.min.y = center.y;
        if ((i/4) % 2 == 0)
            b.max.z = center.z;
        else
            b.min.z = center.z;
//        tfm::printf("Read %d: %.3f - %.3f\n", childNodeMask, b.min, b.max);
        node->children[i] = readHCloudIndex(in, b);
    }
    return node;
}


HCloudView::HCloudView() { }


HCloudView::~HCloudView() { }


bool HCloudView::loadFile(QString fileName, size_t maxVertexCount)
{
    m_input.open(fileName.toUtf8(), std::ios::binary);
    m_header.read(m_input);
    g_logger.info("Header:\n%s", m_header);

    Box3f offsetBox(m_header.boundingBox.min - m_header.offset,
                    m_header.boundingBox.max - m_header.offset);
    m_input.seekg(m_header.indexOffset);
    m_rootNode.reset(readHCloudIndex(m_input, offsetBox));
    m_inputCache.reset(new StreamPageCache(m_input));

//    fields.push_back(GeomField(TypeSpec::vec3float32(), "position", npoints));
//    fields.push_back(GeomField(TypeSpec::float32(), "intensity", npoints));
//    V3f* position = (V3f*)fields[0].as<float>();
//    float* intensity = fields[0].as<float>();

    setFileName(fileName);
    setBoundingBox(m_header.boundingBox);
    setOffset(m_header.offset);
    setCentroid(m_header.boundingBox.center());

    return true;
}


void HCloudView::initializeGL()
{
    m_shader.reset(new ShaderProgram(QGLContext::currentContext()));
    m_shader->setShaderFromSourceFile("shaders:las_points_lod.glsl");
}


static void drawBounds(HCloudNode* node, const TransformState& transState)
{
    drawBoundingBox(transState, node->bbox, Imath::C3f(1));
    for (int i = 0; i < 8; ++i)
    {
        if (node->children[i])
            drawBounds(node->children[i], transState);
    }
}


/// Read hcloud point data for node
///
/// If the underlying data isn't cached, return false.
static bool readNodeData(HCloudNode* node, const HCloudHeader& header,
                         StreamPageCache& inputCache, double priority)
{
    node->allocateArrays();
    int nvox = node->numOccupiedVoxels;
    uint64_t offset = node->dataOffset;
    uint64_t length = nvox*sizeof(float)*5;
    if (!inputCache.read((char*)node->position.get(), offset, 3*sizeof(float)*nvox) ||
        !inputCache.read((char*)node->coverage.get(), offset + 3*sizeof(float)*nvox, sizeof(float)*nvox) ||
        !inputCache.read((char*)node->intensity.get(), offset + 4*sizeof(float)*nvox, sizeof(float)*nvox))
    {
        inputCache.prefetch(offset, length, priority);
        node->freeArrays();
        return false;
    }

    return true;
}


void HCloudView::draw(const TransformState& transStateIn, double quality)
{
    TransformState transState = transStateIn.translate(offset());
    //drawBounds(m_rootNode.get(), transState);

    V3f cameraPos = V3d(0) * transState.modelViewMatrix.inverse();
    QGLShaderProgram& prog = m_shader->shaderProgram();
    prog.bind();
    glEnable(GL_POINT_SPRITE);
    glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);

    transState.setUniforms(prog.programId());
    prog.enableAttributeArray("position");
    prog.enableAttributeArray("coverage");
    prog.enableAttributeArray("intensity");
    prog.enableAttributeArray("simplifyThreshold");
    prog.setUniformValue("pointPixelScale", (GLfloat)(0.5 * transState.viewSize.x *
                                                      transState.projMatrix[0][0]));

    // TODO: Ultimately should scale angularSizeLimit with the quality, something
    // like this:
    // const double angularSizeLimit = 0.01/std::min(1.0, quality);
    // g_logger.info("quality = %f", quality);
    const double pixelsPerVoxel = 2;
    const double fieldOfView = 60*M_PI/180; // FIXME - shouldn't be hardcoded...
    double pixelsPerRadian = transStateIn.viewSize.y / fieldOfView;
    const double angularSizeLimit = pixelsPerVoxel*m_header.brickSize/pixelsPerRadian;

    size_t nodesRendered = 0;
    size_t voxelsRendered = 0;

    const size_t fetchQuota = 10;
    size_t fetchedPages = m_inputCache->fetchNow(fetchQuota);

    // Render out nodes which are cached or can now be read from the page cache
    const double rootPriority = 1000;
    std::vector<HCloudNode*> nodeStack;
    std::vector<int> levelStack;
    if (m_rootNode->isCached())
    {
        levelStack.push_back(0);
        nodeStack.push_back(m_rootNode.get());
    }
    else if (readNodeData(m_rootNode.get(), m_header, *m_inputCache, rootPriority))
    {
        nodeStack.push_back(m_rootNode.get());
        levelStack.push_back(0);
        m_sizeBytes += 5*sizeof(float)*m_rootNode->numOccupiedVoxels;
    }
    while (!nodeStack.empty())
    {
        HCloudNode* node = nodeStack.back();
        nodeStack.pop_back();
        int level = levelStack.back();
        levelStack.pop_back();

        double angularSize = node->radius()/(node->bbox.center() - cameraPos).length();
        bool drawNode = angularSize < angularSizeLimit || node->isLeaf;
        if (!drawNode)
        {
            // Want to descend into child nodes - try to cache them and if we
            // can't, force current node to be drawn.
            for (int i = 0; i < 8; ++i)
            {
                HCloudNode* n = node->children[i];
                if (n && !n->isCached())
                {
                    if (readNodeData(n, m_header, *m_inputCache, angularSize))
                        m_sizeBytes += 5*sizeof(float)*node->numOccupiedVoxels;
                    else
                        drawNode = true;
                }
            }
        }
        if (drawNode)
        {
            int nvox = node->numOccupiedVoxels;
            // Ensure large enough array of simplification thresholds
            //
            // Nvidia cards seem to clamp the minimum gl_PointSize to 1, so
            // tiny points get too much emphasis.  We can avoid this
            // problem by randomly removing such points according to the
            // correct probability.  This needs to happen coherently
            // between frames to avoid shimmer, so generate a single array
            // for it and reuse it every time.
            int nsimp = (int)m_simplifyThreshold.size();
            if (nvox > nsimp)
            {
                m_simplifyThreshold.resize(nvox);
                for (int i = nsimp; i < nvox; ++i)
                    m_simplifyThreshold[i] = float(rand())/RAND_MAX;
            }

            // We draw points as billboards (not spheres) when drawing MIP
            // levels: the point radius represents a screen coverage in this
            // case, with no sensible interpreation as a radius toward the
            // camera.  If we don't do this, we get problems for multi layer
            // surfaces where the more distant layer can cover the nearer one.
            prog.setUniformValue("markerShape", GLint(0));
            // Debug - draw octree levels
            // prog.setUniformValue("level", level);
            prog.setUniformValue("lodMultiplier", GLfloat(0.5*node->radius()/m_header.brickSize));
            prog.setAttributeArray("position",  node->position.get(),  3);
            prog.setAttributeArray("intensity", node->intensity.get(), 1);
            prog.setAttributeArray("coverage",  node->coverage.get(),  1);
            prog.setAttributeArray("simplifyThreshold", m_simplifyThreshold.data(), 1);
            glDrawArrays(GL_POINTS, 0, nvox);
            nodesRendered++;
            voxelsRendered += nvox;
        }
        else
        {
            for (int i = 0; i < 8; ++i)
            {
                HCloudNode* n = node->children[i];
                if (n)
                {
                    nodeStack.push_back(n);
                    levelStack.push_back(level+1);
                }
            }
        }
    }

    prog.disableAttributeArray("position");
    prog.disableAttributeArray("coverage");
    prog.disableAttributeArray("intensity");
    prog.disableAttributeArray("simplifyThreshold");

    glDisable(GL_POINT_SPRITE);
    glDisable(GL_VERTEX_PROGRAM_POINT_SIZE);
    prog.release();

    g_logger.info("hcloud: %.1fMB, #nodes = %d, fetched pages = %d, mean voxel size = %.0f",
                  m_sizeBytes/1e6, nodesRendered, fetchedPages,
                  double(voxelsRendered)/nodesRendered);
}


size_t HCloudView::pointCount() const
{
    // FIXME
    return 0;
}


size_t HCloudView::simplifiedPointCount(const V3d& cameraPos,
                                        bool incrementalDraw) const
{
    // FIXME
    return 0;
}


V3d HCloudView::pickVertex(const V3d& cameraPos,
                           const V3d& rayOrigin, const V3d& rayDirection,
                           double longitudinalScale, double* distance) const
{
    // FIXME: Needs full camera transform to calculate angularSizeLimit, as in
    // draw()
    const double angularSizeLimit = 0.01;
    double minDist = DBL_MAX;
    V3d selectedVertex(0);
    std::vector<HCloudNode*> nodeStack;
    std::vector<int> levelStack;
    levelStack.push_back(0);
    if (m_rootNode->isCached())
        nodeStack.push_back(m_rootNode.get());
    while (!nodeStack.empty())
    {
        HCloudNode* node = nodeStack.back();
        nodeStack.pop_back();
        int level = levelStack.back();
        levelStack.pop_back();
        double angularSize = node->radius()/(node->bbox.center() + offset() - cameraPos).length();
        bool useNode = angularSize < angularSizeLimit || node->isLeaf;
        if (!useNode)
        {
            bool childrenCached = true;
            for (int i = 0; i < 8; ++i)
            {
                HCloudNode* n = node->children[i];
                if (n && !n->isCached())
                    childrenCached = false;
            }
            if (!childrenCached)
                useNode = true;
        }
        if (useNode)
        {
            int nvox = node->numOccupiedVoxels;
            double dist = DBL_MAX;
            const V3f* P = reinterpret_cast<const V3f*>(node->position.get());
            size_t idx = closestPointToRay(P, nvox, rayOrigin - offset(),
                                           rayDirection, longitudinalScale, &dist);
            if (dist < minDist)
            {
                minDist = dist;
                selectedVertex = V3d(P[idx]) + offset();
            }
        }
        else
        {
            for (int i = 0; i < 8; ++i)
            {
                HCloudNode* n = node->children[i];
                if (n)
                {
                    nodeStack.push_back(n);
                    levelStack.push_back(level+1);
                }
            }
        }
    }
    return selectedVertex;
}

