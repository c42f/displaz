// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "PointArray.h"

#include "QtLogger.h"

#include "glutil.h"

#include <QGLShaderProgram>
#include <QTime>

#include <functional>
#include <unordered_map>
#include <fstream>
#include <queue>

#include "ply_io.h"

#include "ClipBox.h"


//------------------------------------------------------------------------------
/// Functor to compute octree child node index with respect to some given split
/// point
struct OctreeChildIdx
{
    const V3f* m_P;
    V3f m_center;
    int operator()(size_t i)
    {
        V3f p = m_P[i];
        return 4*(p.z >= m_center.z) +
               2*(p.y >= m_center.y) +
                 (p.x >= m_center.x);
    }

    OctreeChildIdx(const V3f* P, const V3f& center) : m_P(P), m_center(center) {}
};


struct OctreeNode
{
    OctreeNode* children[8]; ///< Child nodes - order (x + 2*y + 4*z)
    size_t beginIndex;       ///< Begin index of points in this node
    size_t endIndex;         ///< End index of points in this node
    mutable size_t nextBeginIndex; ///< Next index for incremental rendering
    Imath::Box3f bbox;       ///< Actual bounding box of points in node
    V3f center;              ///< center of the node
    float halfWidth;         ///< Half the axis-aligned width of the node.

    OctreeNode(const V3f& center, float halfWidth)
        : beginIndex(0), endIndex(0), nextBeginIndex(0),
        center(center), halfWidth(halfWidth)
    {
        for (int i = 0; i < 8; ++i)
            children[i] = 0;
    }

    ~OctreeNode()
    {
        for (int i = 0; i < 8; ++i)
            delete children[i];
    }

    size_t findNearest(const EllipticalDist& distFunc,
                       const V3d& offset, const V3f* p,
                       double& dist) const
    {
         return beginIndex + distFunc.findNearest(offset, p + beginIndex,
                                                  endIndex - beginIndex,
                                                  &dist);
    }

    size_t size() const { return endIndex - beginIndex; }

    bool isLeaf() const { return beginIndex != endIndex; }

    /// Estimate cost of drawing a single leaf node with to given camera
    /// position, quality, and incremental settings.
    ///
    /// Returns estimate of primitive draw count and whether there's anything
    /// more to draw.
    DrawCount drawCount(const V3f& relCamera,
                        double quality, bool incrementalDraw) const
    {
        assert(isLeaf());
        const double drawAllDist = 100;
        double dist = (this->bbox.center() - relCamera).length();
        double diagRadius = this->bbox.size().length()/2;
        // Subtract bucket diagonal dist, since we really want an approx
        // distance to closest point in the bucket, rather than dist to center.
        dist = std::max(10.0, dist - diagRadius);
        double desiredFraction = std::min(1.0, quality*pow(drawAllDist/dist, 2));
        size_t chunkSize = (size_t)ceil(this->size()*desiredFraction);
        DrawCount drawCount;
        drawCount.numVertices = chunkSize;
        if (incrementalDraw)
        {
            drawCount.numVertices = (this->nextBeginIndex >= this->endIndex) ? 0 :
                std::min(chunkSize, this->endIndex - this->nextBeginIndex);
        }
        drawCount.moreToDraw = this->nextBeginIndex < this->endIndex;
        return drawCount;
    }
};


struct ProgressFunc
{
    PointArray& points;
    size_t totProcessed;

    ProgressFunc(PointArray& points) : points(points), totProcessed(0) {}

    void operator()(size_t additionalProcessed)
    {
        totProcessed += additionalProcessed;
        emit points.loadProgress(int(100*totProcessed/points.pointCount()));
    }
};

/// Create an octree over the given set of points with position P
///
/// The points for consideration in the current node are the set
/// P[inds[beginIndex..endIndex]]; the tree building process sorts the inds
/// array in place so that points for the output leaf nodes are held in
/// the range P[inds[node.beginIndex, node.endIndex)]].  center is the central
/// split point for splitting children of the current node; radius is the
/// current node radius measured along one of the axes.
static OctreeNode* makeTree(int depth, size_t* inds,
                            size_t beginIndex, size_t endIndex,
                            const V3f* P, const V3f& center,
                            float halfWidth, ProgressFunc& progressFunc)
{
    OctreeNode* node = new OctreeNode(center, halfWidth);
    const size_t pointsPerNode = 100000;
    // Limit max depth of tree to prevent infinite recursion when
    // greater than pointsPerNode points lie at the same position in
    // space.  floats effectively have 24 bit of precision in the
    // mantissa, so there's never any point splitting more than 24 times.
    const int maxDepth = 24;
    size_t* beginPtr = inds + beginIndex;
    size_t* endPtr = inds + endIndex;
    if (endIndex - beginIndex <= pointsPerNode || depth >= maxDepth)
    {
        // Leaf node: set up indices into point list
        std::random_shuffle(beginPtr, endPtr);
        for (size_t i = beginIndex; i < endIndex; ++i)
            node->bbox.extendBy(P[inds[i]]);
        node->beginIndex = beginIndex;
        node->endIndex = endIndex;
        progressFunc(endIndex - beginIndex);
        return node;
    }
    // Partition points into the 8 child nodes
    size_t* childRanges[9] = {0};
    multi_partition(beginPtr, endPtr, OctreeChildIdx(P, center), &childRanges[1], 8);
    childRanges[0] = beginPtr;
    // Recursively generate child nodes
    float h = halfWidth/2;
    for (int i = 0; i < 8; ++i)
    {
        size_t childBeginIndex = childRanges[i]   - inds;
        size_t childEndIndex   = childRanges[i+1] - inds;
        if (childEndIndex == childBeginIndex)
            continue;
        V3f c = center + V3f((i     % 2 == 0) ? -h : h,
                             ((i/2) % 2 == 0) ? -h : h,
                             ((i/4) % 2 == 0) ? -h : h);
        node->children[i] = makeTree(depth+1, inds, childBeginIndex,
                                     childEndIndex, P, c, h, progressFunc);
        node->bbox.extendBy(node->children[i]->bbox);
    }
    return node;
}


//------------------------------------------------------------------------------
// PointArray implementation
PointArray::PointArray()
    : m_npoints(0),
    m_positionFieldIdx(-1),
    m_P(0)
{ }


PointArray::~PointArray()
{ }


/// Load point cloud in text format, assuming fields XYZ
bool PointArray::loadText(QString fileName, size_t maxPointCount,
                          std::vector<GeomField>& fields, V3d& offset,
                          size_t& npoints, uint64_t& totalPoints)
{
    V3d Psum(0);
    // Use C file IO here, since it's about 40% faster than C++ streams for
    // large text files (tested on linux x86_64, gcc 4.6.3).
    FILE* inFile = fopen(fileName.toUtf8(), "r");
    if (!inFile)
        return false;
    fseek(inFile, 0, SEEK_END);
    const size_t numBytes = ftell(inFile);
    fseek(inFile, 0, SEEK_SET);
    std::vector<Imath::V3d> points;
    Imath::V3d p;
    size_t readCount = 0;
    // Read three doubles; "%*[^\n]" discards up to just before end of line
    while (fscanf(inFile, " %lf %lf %lf%*[^\n]", &p.x, &p.y, &p.z) == 3)
    {
        points.push_back(p);
        ++readCount;
        if (readCount % 10000 == 0)
            emit loadProgress(int(100*ftell(inFile)/numBytes));
    }
    fclose(inFile);
    totalPoints = points.size();
    npoints = points.size();
    // Zero points + nonzero bytes => bad text file
    if (totalPoints == 0 && numBytes != 0)
        return false;
    if (totalPoints > 0)
        offset = points[0];
    fields.push_back(GeomField(TypeSpec::vec3float32(), "position", npoints));
    V3f* position = (V3f*)fields[0].as<float>();
    for (size_t i = 0; i < npoints; ++i)
        position[i] = points[i] - offset;
    return true;
}


/// Load ascii version of the point cloud library PCD format
bool PointArray::loadPly(QString fileName, size_t maxPointCount,
                         std::vector<GeomField>& fields, V3d& offset,
                         size_t& npoints, uint64_t& totalPoints)
{
    std::unique_ptr<t_ply_, int(*)(p_ply)> ply(
            ply_open(fileName.toUtf8().constData(), logRplyError, 0, NULL), ply_close);
    if (!ply || !ply_read_header(ply.get()))
        return false;
    // Parse out header data
    p_ply_element vertexElement = findVertexElement(ply.get(), npoints);
    if (vertexElement)
    {
        if (!loadPlyVertexProperties(fileName, ply.get(), vertexElement, fields, offset, npoints))
            return false;
    }
    else
    {
        if (!loadDisplazNativePly(fileName, ply.get(), fields, offset, npoints))
            return false;
    }
    totalPoints = npoints;
    return true;
}


bool PointArray::loadFile(QString fileName, size_t maxPointCount)
{
    QTime loadTimer;
    loadTimer.start();
    setFileName(fileName);
    // Read file into point data fields.  Use very basic file type detection
    // based on extension.
    uint64_t totalPoints = 0;
    V3d offset(0);
    emit loadStepStarted("Reading file");
    if (fileName.endsWith(".las") || fileName.endsWith(".laz"))
    {
        if (!loadLas(fileName, maxPointCount, m_fields, offset, m_npoints, totalPoints))
            return false;
    }
    else if (fileName.endsWith(".ply"))
    {
        if (!loadPly(fileName, maxPointCount, m_fields, offset, m_npoints, totalPoints))
            return false;
    }
#if 0
    else if (fileName.endsWith(".dat"))
    {
        // Load crappy db format for debugging
        std::ifstream file(fileName.toUtf8(), std::ios::binary);
        file.seekg(0, std::ios::end);
        totalPoints = file.tellg()/(4*sizeof(float));
        file.seekg(0);
        m_fields.push_back(GeomField(TypeSpec::vec3float32(), "position", totalPoints));
        m_fields.push_back(GeomField(TypeSpec::float32(), "intensity", totalPoints));
        float* position = m_fields[0].as<float>();
        float* intensity = m_fields[1].as<float>();
        for (size_t i = 0; i < totalPoints; ++i)
        {
            file.read((char*)position, 3*sizeof(float));
            file.read((char*)intensity, 1*sizeof(float));
            bbox.extendBy(V3d(position[0], position[1], position[2]));
            position += 3;
            intensity += 1;
        }
        m_npoints = totalPoints;
    }
#endif
    else
    {
        // Last resort: try loading as text
        if (!loadText(fileName, maxPointCount, m_fields, offset, m_npoints, totalPoints))
            return false;
    }
    // Search for position field
    m_positionFieldIdx = -1;
    for (size_t i = 0; i < m_fields.size(); ++i)
    {
        if (m_fields[i].name == "position" && m_fields[i].spec == TypeSpec::vec3float32())
        {
            m_positionFieldIdx = (int)i;
            break;
        }
    }
    if (m_positionFieldIdx == -1)
    {
        g_logger.error("No position field found in file %s", fileName);
        return false;
    }
    m_P = (V3f*)m_fields[m_positionFieldIdx].as<float>();

    // Compute bounding box and centroid
    Imath::Box3d bbox;
    V3d centroid(0);
    V3d Psum(0);
    for (size_t i = 0; i < m_npoints; ++i)
    {
        Psum += m_P[i];
        bbox.extendBy(m_P[i]);
    }
    if (m_npoints > 0)
        centroid = (1.0/m_npoints) * Psum;
    centroid += offset;
    bbox.min += offset;
    bbox.max += offset;

    setBoundingBox(bbox);
    setOffset(offset);
    setCentroid(centroid);
    emit loadProgress(100);
    g_logger.info("Loaded %d of %d points from file %s in %.2f seconds",
                  m_npoints, totalPoints, fileName, loadTimer.elapsed()/1000.0);
    if (totalPoints == 0)
    {
        m_rootNode.reset(new OctreeNode(V3f(0), 1));
        return true;
    }

    // Sort points into octree order
    emit loadStepStarted("Sorting points");
    std::unique_ptr<size_t[]> inds(new size_t[m_npoints]);
    for (size_t i = 0; i < m_npoints; ++i)
        inds[i] = i;
    // Expand the bound so that it's cubic.  Not exactly sure it's required
    // here, but cubic nodes sometimes work better the points are better
    // distributed for LoD, splitting is unbiased, etc.
    Imath::Box3f rootBound(bbox.min - offset, bbox.max - offset);
    V3f diag = rootBound.size();
    float rootRadius = std::max(std::max(diag.x, diag.y), diag.z) / 2;
    ProgressFunc progressFunc(*this);
    m_rootNode.reset(makeTree(0, &inds[0], 0, m_npoints, &m_P[0],
                              rootBound.center(), rootRadius, progressFunc));
    // Reorder point fields into octree order
    emit loadStepStarted("Reordering fields");
    for (size_t i = 0; i < m_fields.size(); ++i)
    {
        g_logger.debug("Reordering field %d: %s", i, m_fields[i]);
        reorder(m_fields[i], inds.get(), m_npoints);
        emit loadProgress(int(100*(i+1)/(m_fields.size()+1))); // denominator +1 for permutation reorder below
    }
    m_P = (V3f*)m_fields[m_positionFieldIdx].as<float>();

    // The index we want to store is the reverse permutation of the index above
    // This is necessary if we want to mutate the data later
    m_inds = std::unique_ptr<uint32_t[]>(new uint32_t[m_npoints]);
    for (size_t i = 0; i < m_npoints; ++i) m_inds[inds[i]] = i; // Works for m_npoints < UINT32_MAX
    emit loadProgress(int(100));

    return true;
}


void PointArray::mutate(std::shared_ptr<GeometryMutator> mutator)
{
    // Now we need to find the matching columns
    auto npoints = mutator->pointCount();
    const std::vector<GeomField>& mutFields = mutator->fields();
    auto mutIdx = mutator->index();

    if (m_npoints > UINT32_MAX)
    {
        g_logger.error("Mutation with more than 2^32 points is not supported");
        return;
    }

    // Check index is valid
    for (size_t j = 0; j < npoints; ++j)
    {
        if (mutIdx[j] >= m_npoints)
        {
            g_logger.error("Index out of bounds - got %d (should be between zero and %d)", mutIdx[j], m_npoints-1);
            return;
        }
    }

    for (size_t mutFieldIdx = 0; mutFieldIdx < mutFields.size(); ++mutFieldIdx)
    {
        if (mutFields[mutFieldIdx].name == "index")
            continue;

        // Attempt to find a matching index
        int foundIdx = -1;
        for (size_t fieldIdx = 0; fieldIdx < m_fields.size(); ++fieldIdx)
        {
            if (m_fields[fieldIdx].name == mutFields[mutFieldIdx].name)
            {
                if (!(m_fields[fieldIdx].spec == mutFields[mutFieldIdx].spec))
                {
                    g_logger.warning("Fields with name \"%s\" do not have matching types, skipping.", m_fields[fieldIdx].name);
                    break;
                }

                if (mutFields[mutFieldIdx].name == "position")
                    g_logger.warning("Moving points by large distances may result in visual artefacts");

                foundIdx = (int)fieldIdx;
                break;
            }
        }

        if (foundIdx == -1)
        {
            g_logger.warning("Couldn't find a field labeled \"%s\"", mutFields[mutFieldIdx].name);
            continue;
        }

        if (mutFields[mutFieldIdx].name == "position")
        {
            assert(m_fields[foundIdx].spec == TypeSpec::float32());
            // Special case for floating point position with offset.
            float* dest = m_fields[foundIdx].as<float>();
            const float* src = mutFields[mutFieldIdx].as<float>();
            V3d off = offset() - mutator->offset();
            for (size_t j = 0; j < npoints; ++j)
            {
                float* d = &dest[3*m_inds[mutIdx[j]]];
                const float* s = &src[3*j];
                d[0] = s[0] - off.x;
                d[1] = s[1] - off.y;
                d[2] = s[2] - off.z;
            }
        }
        else
        {
            // Now we copy data from the mutator to the object
            char* dest = m_fields[foundIdx].data.get();
            char* src = mutFields[mutFieldIdx].data.get();
            size_t fieldsize = m_fields[foundIdx].spec.size();
            for (size_t j = 0; j < npoints; ++j)
            {
                memcpy(dest + fieldsize*m_inds[mutIdx[j]], src + fieldsize*j, fieldsize);
            }
        }
    }
}


bool PointArray::pickVertex(const V3d& cameraPos,
                            const EllipticalDist& distFunc,
                            V3d& pickedVertex,
                            double* distance,
                            std::string* info) const
{
    if (m_npoints == 0)
        return false;

    double closestDist = DBL_MAX;
    size_t closestIdx = 0;

    typedef std::pair<double, const OctreeNode*> PriorityNode;

    auto makePriortyNode = [&](const OctreeNode* node)
    {
        // Create (priority,node) pair with priority given by lower bound of
        // distance for pickVertex() vertex search.
        Box3d bbox(offset() + node->bbox.min, offset() + node->bbox.max);
        return PriorityNode(distFunc.boundNearest(bbox), node);
    };

    // Search for the closest point by putting nodes into a priority queue,
    // with closer nodes having higher priority.  Keep track of the current
    // closest point; as soon as the next priority node is further away than
    // this, we're done.
    std::priority_queue<PriorityNode, std::vector<PriorityNode>,
                        std::greater<PriorityNode>> pendingNodes;
    pendingNodes.push(makePriortyNode(m_rootNode.get()));
    while (!pendingNodes.empty())
    {
        auto nextNode = pendingNodes.top();
        double nextMinDist = nextNode.first;
        if (nextMinDist > closestDist)
            break;
        const OctreeNode* node = nextNode.second;
        pendingNodes.pop();

        if (!node->isLeaf())
        {
            for (int i = 0; i < 8; ++i)
            {
                OctreeNode* n = node->children[i];
                if (n)
                    pendingNodes.push(makePriortyNode(n));
            }
        }
        else
        {
            double dist = 0;
            size_t idx = node->findNearest(distFunc, offset(), m_P, dist);
            if(dist < closestDist)
            {
                closestDist = dist;
                closestIdx = idx;
            }
        }
    }

    if(closestDist == DBL_MAX)
        return false;

    *distance = closestDist;
    pickedVertex = V3d(m_P[closestIdx]) + offset();

    if (info)
    {
        // Format all selected point attributes for user display
        // TODO: Make the type dispatch machinary generic & put in TypeSpec
        std::ostringstream out;
        for (size_t i = 0; i < m_fields.size(); ++i)
        {
            const GeomField& field = m_fields[i];
            tfm::format(out, "  %s = ", field.name);
            if (field.name == "position")
            {
                // Special case for position, since it has an associated offset
                const float* p = (float*)(field.data.get() + closestIdx*field.spec.size());
                tfm::format(out, "%.3f %.3f %.3f\n",
                            p[0] + offset().x, p[1] + offset().y, p[2] + offset().z);
            }
            else
            {
                field.format(out, closestIdx);
                tfm::format(out, "\n");
            }
        }
        *info = out.str();
    }

    return true;
}


void PointArray::estimateCost(const TransformState& transState,
                              bool incrementalDraw, const double* qualities,
                              DrawCount* drawCounts, int numEstimates) const
{
    TransformState relativeTrans = transState.translate(offset());
    V3f relCamera = relativeTrans.cameraPos();
    ClipBox clipBox(relativeTrans);

    std::vector<const OctreeNode*> nodeStack;
    nodeStack.push_back(m_rootNode.get());
    while (!nodeStack.empty())
    {
        const OctreeNode* node = nodeStack.back();
        nodeStack.pop_back();
        if (clipBox.canCull(node->bbox))
            continue;
        if (!node->isLeaf())
        {
            for (int i = 0; i < 8; ++i)
            {
                OctreeNode* n = node->children[i];
                if (n)
                    nodeStack.push_back(n);
            }
            continue;
        }
        for (int i = 0; i < numEstimates; ++i)
        {
            drawCounts[i] += node->drawCount(relCamera, qualities[i],
                                             incrementalDraw);
        }
    }
}


static void drawTree(QGLShaderProgram& prog, const TransformState& transState, const OctreeNode* node)
{
    Imath::Box3f bbox(node->center - Imath::V3f(node->halfWidth),
                      node->center + Imath::V3f(node->halfWidth));

    drawBox(transState, bbox, Imath::C3f(1), prog.programId());
    drawBox(transState, node->bbox, Imath::C3f(1,0,0), prog.programId());

    for (int i = 0; i < 8; ++i)
    {
        OctreeNode* n = node->children[i];
        if (n)
            drawTree(prog, transState, n);
    }
}

void PointArray::drawTree(QGLShaderProgram& prog, const TransformState& transState) const
{
    ::drawTree(prog, transState, m_rootNode.get());
}

void PointArray::initializeGL()
{
    Geometry::initializeGL();

    GLuint vao;
    glGenVertexArrays(1, &vao);
    setVAO("points", vao);

    GLuint vbo;
    glGenBuffers(1, &vbo);
    setVBO("point_buffer", vbo);
}

void PointArray::draw(const TransformState& transState, double quality) const
{
}

DrawCount PointArray::drawPoints(QGLShaderProgram& prog, const TransformState& transState,
                                 double quality, bool incrementalDraw) const
{
    GLuint vao = getVAO("points");
    glBindVertexArray(vao);

    GLuint vbo = getVBO("point_buffer");
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    TransformState relativeTrans = transState.translate(offset());
    relativeTrans.setUniforms(prog.programId());
    //printActiveShaderAttributes(prog.programId());
    std::vector<ShaderAttribute> activeAttrs = activeShaderAttributes(prog.programId());
    // Figure out shader locations for each point field
    // TODO: attributeLocation() forces the OpenGL usage here to be
    // synchronous.  Does this matter?  (Alternative: bind them ourselves.)
    std::vector<const ShaderAttribute*> attributes;
    for (size_t i = 0; i < m_fields.size(); ++i)
    {
        const GeomField& field = m_fields[i];
        if (field.spec.isArray())
        {
            for (int j = 0; j < field.spec.count; ++j)
            {
                std::string name = tfm::format("%s[%d]", field.name, j);
                attributes.push_back(findAttr(name, activeAttrs));
            }
        }
        else
        {
            attributes.push_back(findAttr(field.name, activeAttrs));
        }
    }
    // Zero out active attributes in case they don't have associated fields
    GLfloat zeros[16] = {0};
    for (size_t i = 0; i < activeAttrs.size(); ++i)
    {
        prog.setAttributeValue((int)i, zeros, activeAttrs[i].rows,
                               activeAttrs[i].cols);
    }
    // Enable attributes which have associated fields
    for (size_t i = 0; i < attributes.size(); ++i)
    {
        if (attributes[i])
            glEnableVertexAttribArray(attributes[i]->location);
    }

    // Compute number of bytes required to store all attributes of a vertex, in
    // bytes.
    size_t perVertexBytes = 0;
    for (size_t i = 0; i < m_fields.size(); ++i)
    {
        const GeomField &field = m_fields[i];
        unsigned int arraySize = field.spec.arraySize();
        unsigned int vecSize = field.spec.vectorSize();
        perVertexBytes += arraySize * vecSize * field.spec.elsize; //sizeof(glBaseType(field.spec));
    }

    DrawCount drawCount;
    ClipBox clipBox(relativeTrans);

    // Draw points in each bucket, with total number drawn depending on how far
    // away the bucket is.  Since the points are shuffled, this corresponds to
    // a stochastic simplification of the full point cloud.
    V3f relCamera = relativeTrans.cameraPos();
    std::vector<const OctreeNode*> nodeStack;
    nodeStack.push_back(m_rootNode.get());
    while (!nodeStack.empty())
    {
        const OctreeNode* node = nodeStack.back();
        nodeStack.pop_back();
        if (clipBox.canCull(node->bbox))
            continue;
        if (!node->isLeaf())
        {
            for (int i = 0; i < 8; ++i)
            {
                OctreeNode* n = node->children[i];
                if (n)
                    nodeStack.push_back(n);
            }
            continue;
        }
        if (!incrementalDraw)
            node->nextBeginIndex = node->beginIndex;

        DrawCount nodeDrawCount = node->drawCount(relCamera, quality, incrementalDraw);
        drawCount += nodeDrawCount;

        if (nodeDrawCount.numVertices == 0)
            continue;

        if (m_fields.size() < 1)
            continue;

        // Create a new uninitialized buffer for the current node, reserving
        // enough space for the entire set of vertex attributes which will be
        // passed to the shader.
        //
        // (This new memory area will be bound to the "point_buffer" VBO until
        // the memory is orphaned by calling glBufferData() next time through
        // the loop.  The orphaned memory should be cleaned up by the driver,
        // and this may actually be quite efficient, see
        // http://stackoverflow.com/questions/25111565/how-to-deallocate-glbufferdata-memory
        // http://hacksoflife.blogspot.com.au/2015/06/glmapbuffer-no-longer-cool.html )
        GLsizeiptr nodeBufferSize = perVertexBytes * nodeDrawCount.numVertices;
        glBufferData(GL_ARRAY_BUFFER, nodeBufferSize, NULL, GL_STREAM_DRAW);

        GLintptr bufferOffset = 0;
        for (size_t i = 0, k = 0; i < m_fields.size(); k+=m_fields[i].spec.arraySize(), ++i)
        {
            const GeomField& field = m_fields[i];
            int arraySize = field.spec.arraySize();
            int vecSize = field.spec.vectorSize();

            // TODO?: Could use a single data-array that isn't split into
            // vertex / normal / color / etc. sections, but has interleaved
            // data ?  OpenGL has a stride value in glVertexAttribPointer for
            // exactly this purpose, which should be used for better efficiency
            // here we write only the current attribute data into this the
            // buffer (e.g. all positions, then all colors)
            GLsizeiptr fieldBufferSize = arraySize * vecSize * field.spec.elsize * nodeDrawCount.numVertices;

            // Upload raw data for `field` to the appropriate part of the buffer.
            char* bufferData = field.data.get() + node->nextBeginIndex*field.spec.size();
            glBufferSubData(GL_ARRAY_BUFFER, bufferOffset, fieldBufferSize, bufferData);

            // Tell OpenGL how to interpret the buffer of raw data which was
            // just uploaded.  This should be a single call, but OpenGL spec
            // insanity says we need `arraySize` calls (though arraySize=1
            // for most usage.)
            for (int j = 0; j < arraySize; ++j)
            {
                const ShaderAttribute* attr = attributes[k+j];
                if (!attr)
                    continue;

                GLintptr arrayElementOffset = bufferOffset + j*field.spec.elsize;

                if (attr->baseType == TypeSpec::Int || attr->baseType == TypeSpec::Uint)
                {
                    glVertexAttribIPointer(attr->location, vecSize, glBaseType(field.spec),
                                           0, (const GLvoid *)arrayElementOffset);
                }
                else
                {
                    glVertexAttribPointer(attr->location, vecSize, glBaseType(field.spec),
                                          field.spec.fixedPoint, 0, (const GLvoid *)arrayElementOffset);
                }
            }

            bufferOffset += fieldBufferSize;
        }

        glDrawArrays(GL_POINTS, 0, (GLsizei)nodeDrawCount.numVertices);
        node->nextBeginIndex += nodeDrawCount.numVertices;
    }
    //tfm::printf("Drew %d of total points %d, quality %f\n", totDraw, m_npoints, quality);

    // Disable all attribute arrays - leaving these enabled seems to screw with
    // the OpenGL fixed function pipeline in unusual ways.
    for (size_t i = 0; i < attributes.size(); ++i)
    {
        if (attributes[i])
            glDisableVertexAttribArray(attributes[i]->location);
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return drawCount;
}
