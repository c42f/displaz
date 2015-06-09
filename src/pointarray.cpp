// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#include "pointarray.h"

#include "qtlogger.h"

#include "glutil.h"

#include <QtOpenGL/QGLShaderProgram>
#include <QtCore/QTime>

#include <unordered_map>
#include <fstream>

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
    float radius;            ///< Distance to center to node edge along an axis

    OctreeNode(const V3f& center, float radius)
        : beginIndex(0), endIndex(0), nextBeginIndex(0),
        center(center), radius(radius)
    {
        for (int i = 0; i < 8; ++i)
            children[i] = 0;
    }

    ~OctreeNode()
    {
        for (int i = 0; i < 8; ++i)
            delete children[i];
    }

/*    void print(const int indent, const bool treeWalkDown, const V3f *const P) const
    {
        char s[256];
        for(int i = 0; i < indent; i++)
            s[i] = ' ';
         s[indent] = 0;
        printf("%sCenter = (%f %f %f)\n", s, center.x, center.y, center.z);
        printf("%sradius = %f\n", s, radius);
        printf("%sCenter = (%f %f %f)\n", s, bbox.center().x, bbox.center().y, bbox.center().z);
        printf("%sBox    = (%f %f %f)\n", s, bbox.size().x, bbox.size().y, bbox.size().z);
        printf("%sbeginIndex = %lu\n", s, beginIndex);
        printf("%sendIndex   = %lu\n", s, endIndex);

        if(endIndex > beginIndex)
        {
            V3f xMin( FLT_MAX,  FLT_MAX,  FLT_MAX);
            V3f xMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
            for(size_t i = beginIndex; i < endIndex; i++)
            {
                const V3f& p = P[i];
                xMin.x = p.x < xMin.x ? p.x : xMin.x;
                xMin.y = p.y < xMin.y ? p.y : xMin.y;
                xMin.z = p.z < xMin.z ? p.z : xMin.z;
                xMax.x = p.x > xMax.x ? p.x : xMax.x;
                xMax.y = p.y > xMax.y ? p.y : xMax.y;
                xMax.z = p.z > xMax.z ? p.z : xMax.z;
            }
            printf("%sxMin   = (%f %f %f)\n", s, xMin.x, xMin.y, xMin.z);
            printf("%sxMax   = (%f %f %f)\n", s, xMax.x, xMax.y, xMax.z);
        }

        if(treeWalkDown)
            for (int i = 0; i < 8; ++i)
                if(children[i] != 0)
                {
                    printf("%sChild[%i]:\n", s, i);
                    children[i]->print(indent + 2, treeWalkDown, P);
                }
    }*/

    void closestPoint(
        const V3f *const P,
        const V3f& rayOrigin,
        const V3f& rayDir,
        float& rToAxis,
        float& xToOrigin,
        size_t& iToAxis,
        size_t& iToOrigin) const
    {
        // This function does three things:
        // (1) Firstly it determines whether or not a ray from rayOrigin in rayDir will pass near
        //     or through the octree node and only proceed if it does
        // (2) For each point within the octree node it finds the distance of closest approach
        //     to the vector from rayOrigin in rayDir and tracks the point with the minimum distance
        //     and point index in rToAxis and iToAxis respectively
        // (3) For points within a cylinder around the ray (determined by maxRToAxis) it returns the minimum
        //     distance to the origin and point index in xToOrigin and iToOrigin respectively

        const float maxRToAxis = 0.1;

        {
            const float diagRadius = 1.2*bbox.size().length()/2; // Sphere diameter is length of box diagonal

            V3f o2c = bbox.center() - rayOrigin; // vector from rayOrigin to box center
            const float o2cl = o2c.length();

            if(o2cl > diagRadius)
            {
                // rayOrigin lies outside of bounding sphere

                const float cosA = (o2c ^ rayDir)/o2cl;  // cosine of angle between rayDir and vector from origin to center

                if(cosA < FLT_MIN)
                    return; // rayDir points to side or behind with respect to direction from origin to center of box

                const float sinA = sqrtf(1 - cosA*cosA); // sine of angle between rayDir and vector from origin to center

                if(sinA/cosA > diagRadius/o2cl)
                    return; // rayDir does not pass within diagRadius of box center
            }
        }

        // now scan though all the points within this node
        for(size_t i = beginIndex; i < endIndex; i++)
        {
            V3f o2p = P[i] - rayOrigin; // vector from ray origin to point

            const float l2 = o2p.length2(); // square of hypotenuse length

            if(l2 < FLT_MIN)
                continue; // point lies at rayOrigin - this is not the point we are looking for

            const float a = o2p ^ rayDir; // distance along ray to point of closest approach to test point

            if(a < FLT_MIN)
                continue; // point lies behind rayOrigin

            const float r = sqrtf(l2 - a*a);

            if(r < rToAxis)
            {
                // new closest point to axis
                rToAxis = r;
                iToAxis = i;
            }

            if(r < maxRToAxis)
            {
                // point within cylinder
                if(a < xToOrigin)
                {
                    xToOrigin = a;
                    iToOrigin = i;
                }
            }
        }
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
                            float radius, ProgressFunc& progressFunc)
{
    OctreeNode* node = new OctreeNode(center, radius);
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
        std::random_shuffle(beginPtr, endPtr);
        for (size_t i = beginIndex; i < endIndex; ++i)
            node->bbox.extendBy(P[inds[i]]);
        node->beginIndex = beginIndex;
        node->endIndex = endIndex;
        progressFunc(endIndex - beginIndex);
        return node;
    }
    // Partition points into the 8 child nodes
    size_t* childRanges[9];
    multi_partition(beginPtr, endPtr, OctreeChildIdx(P, center), childRanges+1, 8);
    childRanges[0] = beginPtr;
    // Recursively generate child nodes
    float r = radius/2;
    for (int i = 0; i < 8; ++i)
    {
        size_t childBeginIndex = childRanges[i]   - inds;
        size_t childEndIndex   = childRanges[i+1] - inds;
        if (childEndIndex == childBeginIndex)
            continue;
        V3f c = center + V3f((i     % 2 == 0) ? -r : r,
                             ((i/2) % 2 == 0) ? -r : r,
                             ((i/4) % 2 == 0) ? -r : r);
        node->children[i] = makeTree(depth+1, inds, childBeginIndex,
                                     childEndIndex, P, c, r, progressFunc);
        node->bbox.extendBy(node->children[i]->bbox);
    }
    return node;
}


//------------------------------------------------------------------------------
// PointArray implementation
PointArray::PointArray()
    : m_npoints(0),
    m_positionFieldIdx(-1),
    m_P(0),
    m_lastVertexPickIdx(0)
{ }


PointArray::~PointArray()
{ }


/// Load point cloud in text format, assuming fields XYZ
bool PointArray::loadText(QString fileName, size_t maxPointCount,
                          std::vector<GeomField>& fields, V3d& offset,
                          size_t& npoints, uint64_t& totPoints,
                          Imath::Box3d& bbox, V3d& centroid)
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
    totPoints = points.size();
    npoints = points.size();
    // Zero points + nonzero bytes => bad text file
    if (totPoints == 0 && numBytes != 0)
        return false;
    if (totPoints > 0)
        offset = points[0];
    fields.push_back(GeomField(TypeSpec::vec3float32(), "position", npoints));
    V3f* position = (V3f*)fields[0].as<float>();
    for (size_t i = 0; i < npoints; ++i)
    {
        position[i] = points[i] - offset;
        bbox.extendBy(points[i]);
        Psum += points[i];
    }
    if (npoints > 0)
        centroid = (1.0/npoints)*Psum;
    return true;
}


/// Load ascii version of the point cloud library PCD format
bool PointArray::loadPly(QString fileName, size_t maxPointCount,
                         std::vector<GeomField>& fields, V3d& offset,
                         size_t& npoints, uint64_t& totPoints,
                         Imath::Box3d& bbox, V3d& centroid)
{
    std::unique_ptr<t_ply_, int(*)(p_ply)> ply(
            ply_open(fileName.toUtf8().constData(), NULL, 0, NULL), ply_close);
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
    totPoints = npoints;

    // Compute bounding box and centroid
    const V3f* P = (V3f*)fields[0].as<float>();
    V3d Psum(0);
    for (size_t i = 0; i < npoints; ++i)
    {
        Psum += P[i];
        bbox.extendBy(P[i]);
    }
    if (npoints > 0)
        centroid = (1.0/npoints) * Psum;
    centroid += offset;
    bbox.min += offset;
    bbox.max += offset;
    return true;
}


bool PointArray::loadFile(QString fileName, size_t maxPointCount)
{
    QTime loadTimer;
    loadTimer.start();
    setFileName(fileName);
    // Read file into point data fields.  Use very basic file type detection
    // based on extension.
    uint64_t totPoints = 0;
    Imath::Box3d bbox;
    V3d offset(0);
    V3d centroid(0);
    emit loadStepStarted("Reading file");
    if (fileName.endsWith(".las") || fileName.endsWith(".laz"))
    {
        if (!loadLas(fileName, maxPointCount, m_fields, offset,
                     m_npoints, totPoints, bbox, centroid))
        {
            return false;
        }
    }
    else if (fileName.endsWith(".ply"))
    {
        if (!loadPly(fileName, maxPointCount, m_fields, offset,
                     m_npoints, totPoints, bbox, centroid))
        {
            return false;
        }
    }
#if 0
    else if (fileName.endsWith(".dat"))
    {
        // Load crappy db format for debugging
        std::ifstream file(fileName.toUtf8(), std::ios::binary);
        file.seekg(0, std::ios::end);
        totPoints = file.tellg()/(4*sizeof(float));
        file.seekg(0);
        m_fields.push_back(GeomField(TypeSpec::vec3float32(), "position", totPoints));
        m_fields.push_back(GeomField(TypeSpec::float32(), "intensity", totPoints));
        float* position = m_fields[0].as<float>();
        float* intensity = m_fields[1].as<float>();
        for (size_t i = 0; i < totPoints; ++i)
        {
            file.read((char*)position, 3*sizeof(float));
            file.read((char*)intensity, 1*sizeof(float));
            bbox.extendBy(V3d(position[0], position[1], position[2]));
            position += 3;
            intensity += 1;
        }
        m_npoints = totPoints;
    }
#endif
    else
    {
        // Last resort: try loading as text
        if (!loadText(fileName, maxPointCount, m_fields, offset,
                      m_npoints, totPoints, bbox, centroid))
        {
            return false;
        }
    }
    // Search for position field
    m_positionFieldIdx = -1;
    for (size_t i = 0; i < m_fields.size(); ++i)
    {
        if (m_fields[i].name == "position" && m_fields[i].spec.count == 3)
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
    setBoundingBox(bbox);
    setOffset(offset);
    setCentroid(centroid);
    emit loadProgress(100);
    g_logger.info("Loaded %d of %d points from file %s in %.2f seconds",
                  m_npoints, totPoints, fileName, loadTimer.elapsed()/1000.0);
    if (totPoints == 0)
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
        emit loadProgress(int(100*(i+1)/m_fields.size()));
    }
    m_P = (V3f*)m_fields[m_positionFieldIdx].as<float>();

    return true;
}


V3d PointArray::pickVertex(const V3d& cameraPos,
                           const V3d& rayOrigin, const V3d& rayDirection,
                           double longitudinalScale, double* distance,
                           std::string* info) const
{
    // NOTE rayOrigin here is not cameraPos but something else
    // This routine uses cameraPos as the rayOrigin point

    if (m_npoints == 0)
        return V3d(0);

    const V3f cameraPosOffset = cameraPos - offset();

    float rToAxis    = FLT_MAX;
    float xToOrigin  = FLT_MAX;
    size_t iToAxis   = 0;
    size_t iToOrigin = 0;

    std::vector<const OctreeNode*> nodeStack;
    nodeStack.push_back(m_rootNode.get());
    while (!nodeStack.empty())
    {
        const OctreeNode* node = nodeStack.back();
        nodeStack.pop_back();

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

        node->closestPoint(m_P, cameraPosOffset, rayDirection, rToAxis, xToOrigin, iToAxis, iToOrigin);
    }

    size_t idx;

    if(xToOrigin == FLT_MAX)
    {
        // no points within cylinder
        if(rToAxis == FLT_MAX)
            idx = m_lastVertexPickIdx; // no nearest point to axis found either
        else
            idx = iToAxis;
    }
    else
        idx = iToOrigin;

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
                const float* p = (float*)(field.data.get() + idx*field.spec.size());
                tfm::format(out, "%.3f %.3f %.3f\n",
                            p[0] + offset().x, p[1] + offset().y, p[2] + offset().z);
                continue;
            }
            for (int j = 0; j < field.spec.count; ++j)
            {
                const char* data = field.data.get() + idx*field.spec.size() + j*field.spec.elsize;
                switch (field.spec.type)
                {
                    case TypeSpec::Float:
                        switch (field.spec.elsize)
                        {
                            case 4:  tfm::format(out, "%g", *(float*)data);  break;
                            case 8:  tfm::format(out, "%g", *(double*)data); break;
                            default: tfm::format(out, "?"); break;
                        }
                        break;
                    case TypeSpec::Int:
                        switch (field.spec.elsize)
                        {
                            case 1:  tfm::format(out, "%d", *(int8_t*)data);  break;
                            case 2:  tfm::format(out, "%d", *(int16_t*)data); break;
                            case 4:  tfm::format(out, "%d", *(int32_t*)data); break;
                            case 8:  tfm::format(out, "%d", *(int64_t*)data); break;
                            default: tfm::format(out, "?"); break;
                        }
                        break;
                    case TypeSpec::Uint:
                        switch (field.spec.elsize)
                        {
                            case 1:  tfm::format(out, "%d", *(uint8_t*)data);  break;
                            case 2:  tfm::format(out, "%d", *(uint16_t*)data); break;
                            case 4:  tfm::format(out, "%d", *(uint32_t*)data); break;
                            case 8:  tfm::format(out, "%d", *(uint64_t*)data); break;
                            default: tfm::format(out, "?"); break;
                        }
                        break;
                    default:
                        tfm::format(out, "unknown");
                }
                if (j < field.spec.count - 1)
                    tfm::format(out, " ");
                else
                    tfm::format(out, "\n");
            }
        }
        *info = out.str();
    }

    m_lastVertexPickIdx = idx;

    return V3d(m_P[idx]) + offset();
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


static void drawTree(const TransformState& transState, const OctreeNode* node)
{
    Imath::Box3f bbox(node->center - Imath::V3f(node->radius),
                        node->center + Imath::V3f(node->radius));
    drawBoundingBox(transState, bbox, Imath::C3f(1));
    drawBoundingBox(transState, node->bbox, Imath::C3f(1,0,0));
    for (int i = 0; i < 8; ++i)
    {
        OctreeNode* n = node->children[i];
        if (n)
            drawTree(transState, n);
    }
}

void PointArray::drawTree(const TransformState& transState) const
{
    ::drawTree(transState, m_rootNode.get());
}


DrawCount PointArray::drawPoints(QGLShaderProgram& prog, const TransformState& transState,
                                 double quality, bool incrementalDraw) const
{
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
            prog.enableAttributeArray(attributes[i]->location);
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
        size_t idx = node->beginIndex;
        if (!incrementalDraw)
            node->nextBeginIndex = node->beginIndex;

        DrawCount nodeDrawCount = node->drawCount(relCamera, quality, incrementalDraw);
        drawCount += nodeDrawCount;

        idx = node->nextBeginIndex;
        if (nodeDrawCount.numVertices == 0)
            continue;
        for (size_t i = 0, k = 0; i < m_fields.size(); k+=m_fields[i].spec.arraySize(), ++i)
        {
            const GeomField& field = m_fields[i];
            int arraySize = field.spec.arraySize();
            int vecSize = field.spec.vectorSize();
            for (int j = 0; j < arraySize; ++j)
            {
                const ShaderAttribute* attr = attributes[k+j];
                if (!attr)
                    continue;
                char* data = field.data.get() + idx*field.spec.size() +
                             j*field.spec.elsize;
                if (attr->baseType == TypeSpec::Int || attr->baseType == TypeSpec::Uint)
                {
                    glVertexAttribIPointer(attr->location, vecSize,
                                           glBaseType(field.spec), 0, data);
                }
                else
                {
                    glVertexAttribPointer(attr->location, vecSize,
                                          glBaseType(field.spec),
                                          field.spec.fixedPoint, 0, data);
                }
            }
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
            prog.disableAttributeArray(attributes[i]->location);
    }
    return drawCount;
}


