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

#include "qtlogger.h"

struct HCloudNode
{
    HCloudNode* children[8]; ///< Child nodes - order (x + 2*y + 4*z)
    Imath::Box3f bbox;       ///< Bounding box of node

    uint64_t dataOffset;
    uint16_t numOccupiedVoxels;
    uint16_t numLeafPoints;

    HCloudNode(const Box3f& bbox)
        : bbox(bbox),
        dataOffset(0), numOccupiedVoxels(0), numLeafPoints(0)
    {
        for (int i = 0; i < 8; ++i)
            children[i] = 0;
    }

    ~HCloudNode()
    {
        for (int i = 0; i < 8; ++i)
            delete children[i];
    }
};


static HCloudNode* readHCloudIndex(std::istream& in, const Box3f& bbox)
{
    HCloudNode* node = new HCloudNode(bbox);
    uint8_t childNodeMask   = readLE<uint8_t>(in);
    node->dataOffset        = readLE<uint64_t>(in);
    node->numOccupiedVoxels = readLE<uint16_t>(in);
    node->numLeafPoints     = readLE<uint16_t>(in);
    V3f center = bbox.center();
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
    std::ifstream in(fileName.toUtf8(), std::ios::binary);
    HCloudHeader header;
    header.read(in);
    g_logger.info("Header:\n%s", header);

    Box3f offsetBox(header.boundingBox.min - header.offset,
                    header.boundingBox.max - header.offset);
    m_rootNode.reset(readHCloudIndex(in, offsetBox));

//    fields.push_back(GeomField(TypeSpec::vec3float32(), "position", npoints));
//    fields.push_back(GeomField(TypeSpec::float32(), "intensity", npoints));
//    V3f* position = (V3f*)fields[0].as<float>();
//    float* intensity = fields[0].as<float>();

    setFileName(fileName);
    setBoundingBox(header.boundingBox);
    setOffset(header.offset);
    setCentroid(header.boundingBox.center());

    return true;
}


void HCloudView::initializeGL() const
{
}


void drawBounds(HCloudNode* node, const TransformState& transState)
{
    drawBoundingBox(transState, node->bbox, Imath::C3f(1));
    for (int i = 0; i < 8; ++i)
    {
        if (node->children[i])
            drawBounds(node->children[i], transState);
    }
}


void HCloudView::draw(const TransformState& transState, double quality) const
{
    TransformState trans2 = transState.translate(offset());
    drawBounds(m_rootNode.get(), trans2);
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


V3d HCloudView::pickVertex(const V3d& rayOrigin, const V3d& rayDirection,
                           double longitudinalScale, double* distance) const
{
    // FIXME
    return V3d(0,0,0);
}

