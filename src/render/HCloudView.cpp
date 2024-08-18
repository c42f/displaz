// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "HCloudView.h"

#include <cfloat>

#include "hcloud.h"
#include "ClipBox.h"
#include "glutil.h"
#include "QtLogger.h"
#include "Shader.h"
#include "ShaderProgram.h"
#include "streampagecache.h"
#include "util.h"

//------------------------------------------------------------------------------
struct HCloudNode
{
    HCloudNode* children[8]; ///< Child nodes - order (x + 2*y + 4*z)
    Imath::Box3f bbox;       ///< Bounding box of node

    NodeIndexData idata;
    bool isLeaf;

    // List of non-empty voxels inside the node
    std::unique_ptr<float[]> position;
    std::unique_ptr<float[]> intensity;
    std::unique_ptr<float[]> coverage;

    HCloudNode(const Box3f& bbox)
        : bbox(bbox),
        isLeaf(false)
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
        position.reset(new float[3*idata.numPoints]);
        intensity.reset(new float[idata.numPoints]);
        if (idata.flags == IndexFlags_Voxels)
            coverage.reset(new float[idata.numPoints]);
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
    node->idata.flags      = IndexFlags(readLE<uint8_t>(in));
    node->idata.dataOffset = readLE<uint64_t>(in);
    node->idata.numPoints  = readLE<uint32_t>(in);
    uint8_t childNodeMask  = readLE<uint8_t>(in);
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
        HCloudNode* child = readHCloudIndex(in, b);
        // Special case for leaf node points: there's a single child node and
        // it shares the parent bounding box.
        if (child->idata.flags == IndexFlags_Points)
            child->bbox = bbox;
        node->children[i] = child;
    }
    return node;
}


HCloudView::HCloudView() : m_sizeBytes(0) { }


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
    m_shader.reset(new ShaderProgram());
    m_shader->setShaderFromSourceFile("shaders:las_points_lod.glsl");
}

static void drawBounds(QOpenGLShaderProgram& prog, HCloudNode* node, const TransformState& transState)
{
    drawBox(transState, node->bbox, Imath::C3f(1), prog.programId());
    for (int i = 0; i < 8; ++i)
    {
        if (node->children[i])
            drawBounds(prog, node->children[i], transState);
    }
}

/// Read hcloud point data for node
///
/// If the underlying data isn't cached, return false.
static bool readNodeData(HCloudNode* node, const HCloudHeader& header,
                         StreamPageCache& inputCache, double priority)
{
    node->allocateArrays();
    uint64_t offset = node->idata.dataOffset;
    PageCacheReader reader(inputCache, offset);
    int numPoints = node->idata.numPoints;
    reader.read(node->position, 3*numPoints);
    if (node->idata.flags == IndexFlags_Voxels)
        reader.read(node->coverage, numPoints);
    reader.read(node->intensity, numPoints);
    if (reader.bad())
    {
        inputCache.prefetch(offset, reader.attemptedBytesRead(), priority);
        node->freeArrays();
        return false;
    }
    return true;
}


void HCloudView::draw(const TransformState& transStateIn, double quality) const
{
    TransformState transState = transStateIn.translate(offset());
    //drawBounds(m_rootNode.get(), transState);

    V3f cameraPos = V3d(0) * transState.modelViewMatrix.inverse();
    QOpenGLShaderProgram& prog = m_shader->shaderProgram();
    prog.bind();
    glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);

    transState.setUniforms(prog.programId());
    prog.setUniformValue("pointPixelScale", (GLfloat)(0.5 * transState.viewSize.x *
                                                      transState.projMatrix[0][0]));
    prog.enableAttributeArray("position");
    prog.enableAttributeArray("coverage");
    prog.enableAttributeArray("intensity");
    prog.enableAttributeArray("simplifyThreshold");

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

    ClipBox clipBox(transState);

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
        m_sizeBytes += 5*sizeof(float)*m_rootNode->idata.numPoints;
    }
    while (!nodeStack.empty())
    {
        HCloudNode* node = nodeStack.back();
        nodeStack.pop_back();
        int level = levelStack.back();
        levelStack.pop_back();

        if (clipBox.canCull(node->bbox))
            continue;

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
                        m_sizeBytes += 5*sizeof(float)*node->idata.numPoints;
                    else
                        drawNode = true;
                }
            }
        }
        if (drawNode)
        {
            int nvox = node->idata.numPoints;
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

            if (node->idata.flags == IndexFlags_Points)
            {
                prog.disableAttributeArray("coverage");
                prog.setAttributeValue("coverage", 1.0f);
                prog.setUniformValue("markerShape", GLint(1));
                // FIXME: Lod multiplier for points shouldn't be hardcoded here.
                prog.setUniformValue("lodMultiplier", 0.1f);
            }
            else
            {
                prog.setUniformValue("lodMultiplier", GLfloat(0.5*node->radius()/m_header.brickSize));
                prog.setAttributeArray("coverage",  node->coverage.get(),  1);
                // Draw voxels as billboards (not spheres) when drawing MIP
                // levels: the point radius represents a screen coverage in
                // this case, with no sensible interpreation as a radius toward
                // the camera.  If we don't do this, we get problems for multi
                // layer surfaces where the more distant layer can cover the
                // nearer one.
                prog.setUniformValue("markerShape", GLint(0));
            }
            // Debug - draw octree levels
            // prog.setUniformValue("level", level);
            prog.setAttributeArray("position",  node->position.get(),  3);
            prog.setAttributeArray("intensity", node->intensity.get(), 1);
            prog.setAttributeArray("simplifyThreshold", m_simplifyThreshold.data(), 1);
            glDrawArrays(GL_POINTS, 0, nvox);
            if (node->idata.flags == IndexFlags_Points)
                prog.enableAttributeArray("coverage");
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

    glDisable(GL_VERTEX_PROGRAM_POINT_SIZE);
    // prog.release();

    g_logger.info("hcloud: %.1fMB, #nodes = %d, fetched pages = %d, mean voxel size = %.0f",
                  m_sizeBytes/1e6, nodesRendered, fetchedPages,
                  nodesRendered ? double(voxelsRendered)/nodesRendered : 0.0);
}


size_t HCloudView::pointCount() const
{
    // FIXME
    return 0;
}


void HCloudView::estimateCost(const TransformState& transState,
                              bool incrementalDraw, const double* qualities,
                              DrawCount* drawCounts, int numEstimates) const
{
    // FIXME
}


bool HCloudView::pickVertex(const V3d& cameraPos,
                            const EllipticalDist& distFunc,
                            V3d& pickedVertex,
                            double* distance,
                            std::string* info) const
{
    // FIXME: Needs full camera transform to calculate angularSizeLimit, as in
    // draw()
    const double angularSizeLimit = 0.01;
    double minDist = DBL_MAX;
    std::vector<HCloudNode*> nodeStack;
    std::vector<int> levelStack;
    bool foundVertex = false;
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
            double dist = DBL_MAX;
            const V3f* P = reinterpret_cast<const V3f*>(node->position.get());
            size_t idx = distFunc.findNearest(offset(), P, node->idata.numPoints, &dist);
            if (dist < minDist)
            {
                minDist = dist;
                pickedVertex = V3d(P[idx]) + offset();
                foundVertex = true;
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
    return foundVertex;
}

