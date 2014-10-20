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

#include <cassert>
#include <memory>
#include <vector>

#include "../bindings/cpp/displaz.h"

#include "voxelizer.h"

/// Debug plotting for OctreeBuilder
inline void plotBrick(dpz::PointList& dpoints,
                      const VoxelBrick& brick, int level, int leafIdx)
{
    int brickRes = brick.resolution();
    for (int z = 0; z < brickRes; ++z)
    for (int y = 0; y < brickRes; ++y)
    for (int x = 0; x < brickRes; ++x)
    {
        float coverage = brick.coverage(x,y,z);
        if (coverage != 0)
        {
            V3f pos = brick.position(x,y,z);
            dpoints.append(pos.x, pos.y, pos.z,
                           brick.color(x,y,z),
                           coverage,
                           level, leafIdx);
        }
    }
}


/// Octree node holding data size and offset in the serialized stream
struct IndexNode
{
    uint64_t dataOffset;
    uint32_t numVoxels;
    uint32_t numPoints;
    std::unique_ptr<IndexNode> children[8];

    IndexNode()
        : dataOffset(0),
        numVoxels(0),
        numPoints(0)
    { }
};


/// Serializer for octree brick data, retaining the bytes in a buffer until
/// flush() is called.
///
/// We'd like to have a good spatially local ordering in the final stream for
/// efficient viewing, but also a memory efficient depth-first build.  Several
/// NodeOutputQueue objects can be used to buffer output of nodes to reorder
/// the depth first ordering into something more sensible, while retaining good
/// memory characteristics.
class NodeOutputQueue
{
    public:
        NodeOutputQueue() : m_sizeBytes(0) { }

        /// Return current number of buffered nodes
        size_t bufferedNodeCount() const { return m_bufferedNodes.size(); }

        /// Return size of currently buffered data, in bytes
        size_t sizeBytes() const { return m_sizeBytes; }

        std::unique_ptr<IndexNode> write(const VoxelBrick& brick)
        {
            std::unique_ptr<IndexNode> index = serializeBrick(m_bufferedBytes, brick);
            m_sizeBytes = m_bufferedBytes.tellp();
            m_bufferedNodes.push_back(index.get());
            return std::move(index);
        }

        void flush(std::ostream& out)
        {
            if (m_bufferedNodes.empty())
                return;
            // Update to absolute offsets in the real output stream
            uint64_t offset = out.tellp();
            for (size_t i = 0; i < m_bufferedNodes.size(); ++i)
                m_bufferedNodes[i]->dataOffset += offset;
            out << m_bufferedBytes.rdbuf();
            // Clear buffers
            m_bufferedNodes.clear();
            m_bufferedBytes.str("");
            m_sizeBytes = 0;
        }

    private:
        /// Serialize brick to stream; return number of bytes written
        static std::unique_ptr<IndexNode> serializeBrick(
                std::ostream& out, const VoxelBrick& brick)
        {
            std::unique_ptr<IndexNode> index(new IndexNode);
            index->dataOffset = out.tellp();
            // Grab voxels with nonzero coverage
            std::vector<float> positions;
            std::vector<float> coverage;
            std::vector<float> intensity;
            for (int i = 0, iend = brick.numVoxels(); i < iend; ++i)
            {
                float cov = brick.coverage(i);
                if (cov != 0)
                {
                    V3f pos = brick.position(i);
                    positions.push_back(pos.x);
                    positions.push_back(pos.y);
                    positions.push_back(pos.z);
                    coverage.push_back(cov);
                    intensity.push_back(brick.color(i));
                }
            }
            index->numVoxels = (uint32_t)coverage.size();
            index->numPoints = 0; // FIXME
            out.write((const char*)positions.data(), positions.size()*sizeof(float));
            out.write((const char*)coverage.data(),  coverage.size()*sizeof(float));
            out.write((const char*)intensity.data(), intensity.size()*sizeof(float));
            return std::move(index);
        }

        std::vector<IndexNode*> m_bufferedNodes;
        uint64_t m_sizeBytes;
        std::stringstream m_bufferedBytes;
};



/// Class for building octrees in a bottom up fashion
///
/// The user must supply octree leaf nodes in Morton order; the internal nodes
/// will be built from these
class OctreeBuilder
{
    public:
        OctreeBuilder(std::ostream& output, int brickRes, int leafDepth,
                      const Imath::V3d& positionOffset,
                      const Imath::Box3d& rootBound, Logger& logger)
            : m_output(output),
            m_brickRes(brickRes),
            m_levelInfo(leafDepth+1),
            m_debugPlot(false),
            m_logger(logger)
        {
            // Fill as much of the header in as possible; we will fill the rest
            // in as we go.
            m_header.boundingBox = rootBound; // FIXME
            m_header.treeBoundingBox = rootBound;
            m_header.offset = positionOffset;
            m_header.brickSize = brickRes;
            // Write dummy header - will come back to fill this in later
            m_header.write(m_output);
            // Data starts directly after header
            m_header.dataOffset = m_output.tellp();
            if (m_debugPlot)
            {
                m_dpoints.addAttribute<float>("position", 3)
                    .addAttribute<float>("intensity", 1)
                    .addAttribute<float>("coverage", 1)
                    .addAttribute<int>("treeLevel", 1)
                    .addAttribute<int>("leafIdx", 1);
            }
        }

        void addNode(int level, int64_t mortonIndex,
                     std::unique_ptr<VoxelBrick> node)
        {
            addNode(level, mortonIndex, std::move(node),
                    std::unique_ptr<IndexNode>());
        }

        void finish()
        {
            // Sweep from leaves to root, flushing any last pending bricks
            for (int i = (int)m_levelInfo.size() - 1; i > 0; --i)
                downsampleLevel(m_levelInfo[i], i);
            assert (m_rootNode);
            // Flush output queues from root to leaves.  This order is useful
            // if page caching starts at the root node data offset, but
            // somewhat irrelevant otherwise.
            for (int i = 0; i < (int)m_levelInfo.size(); ++i)
                flushQueue(m_levelInfo[i].outputQueue, i);
            m_header.indexOffset = m_output.tellp();
            // TODO: Fill numPoints
            writeIndex(m_output, m_rootNode.get());
            m_output.seekp(0);
            m_header.write(m_output);
            m_logger.debug("Wrote hcloud header:\n%s", m_header);
            if (m_debugPlot)
            {
                // Debug plotting
                dpz::Displaz dwin;
                dwin.hold(true);
                dwin.plot(m_dpoints);
            }
        }

        std::unique_ptr<IndexNode> root() { return std::move(m_rootNode); }

    private:
        struct OctreeLevelInfo
        {
            /// Morton index of parent node of currently pending nodes
            int64_t parentMortonIndex;
            /// List of pending nodes
            std::vector<std::unique_ptr<VoxelBrick>> pendingNodes;
            std::vector<std::unique_ptr<IndexNode>> pendingIndexNodes;
            size_t processedNodeCount;
            NodeOutputQueue outputQueue;

            OctreeLevelInfo()
                : parentMortonIndex(INT64_MIN),
                pendingNodes(8),
                pendingIndexNodes(8),
                processedNodeCount(0)
            { }

            bool hasNodes() const { return parentMortonIndex != INT64_MIN; }
        };

        void addNode(int level, int64_t mortonIndex,
                     std::unique_ptr<VoxelBrick> node,
                     std::unique_ptr<IndexNode> indexNode)
        {
            assert(level < (int)m_levelInfo.size());
            OctreeLevelInfo& levelInfo = m_levelInfo[level];
            if (m_debugPlot)
                plotBrick(m_dpoints, *node, level, levelInfo.processedNodeCount);
            ++levelInfo.processedNodeCount;
            if (level == 0)
            {
                assert(indexNode);
                m_rootNode = std::move(indexNode);
                return;
            }
            int64_t parentIndex = mortonIndex/8;
            int childNumber = mortonIndex - 8*parentIndex;
            assert(childNumber < 8);
            if (!levelInfo.hasNodes())
            {
                // Special one time case for when no nodes are cached yet
                levelInfo.parentMortonIndex = parentIndex;
                levelInfo.pendingNodes[childNumber] = std::move(node);
                levelInfo.pendingIndexNodes[childNumber] = std::move(indexNode);
                return;
            }
            if (parentIndex != levelInfo.parentMortonIndex)
            {
                assert(levelInfo.parentMortonIndex < parentIndex);
                downsampleLevel(levelInfo, level);
                levelInfo.parentMortonIndex = parentIndex;
            }
            assert(!levelInfo.pendingNodes[childNumber]);
            assert(!levelInfo.pendingIndexNodes[childNumber]);
            levelInfo.pendingNodes[childNumber] = std::move(node);
            levelInfo.pendingIndexNodes[childNumber] = std::move(indexNode);
        }

        void downsampleLevel(OctreeLevelInfo& levelInfo, int level)
        {
            // Render downsampled brick from children
            VoxelBrick* brickChildren[8] = {0};
            for (int i = 0; i < 8; ++i)
                brickChildren[i] = levelInfo.pendingNodes[i].get();
            std::unique_ptr<VoxelBrick> brick(new VoxelBrick(m_brickRes));
            brick->renderFromBricks(brickChildren);
            // Serialize, grabbing the serialization index for later
            NodeOutputQueue& queue = levelInfo.outputQueue;
            std::unique_ptr<IndexNode> indexNode = queue.write(*brick);
            const size_t maxQueueBytes = 10*1024*1024;
            if (queue.sizeBytes() > maxQueueBytes)
                flushQueue(queue, level);
            for (int i = 0; i < 8; ++i)
                indexNode->children[i] = std::move(levelInfo.pendingIndexNodes[i]);
            // Push new brick and index up the tree
            addNode(level - 1, levelInfo.parentMortonIndex,
                    std::move(brick), std::move(indexNode));
            // Deallocate bricks at current level
            for (int i = 0; i < 8; ++i)
                levelInfo.pendingNodes[i].reset();
        }

        void flushQueue(NodeOutputQueue& queue, int level)
        {
            m_logger.debug("Flushing buffer for level %d: %.2f MiB",
                           level, queue.sizeBytes()/(1024.0*1024.0));
            queue.flush(m_output);
        }

        static void writeIndex(std::ostream& out, const IndexNode* rootNode)
        {
            std::vector<const IndexNode*> nodeStack;
            nodeStack.push_back(rootNode);
            while (!nodeStack.empty())
            {
                const IndexNode* node = nodeStack.back();
                nodeStack.pop_back();
                uint8_t childNodeMask = 0;
                // Pack presence of children as bits into a uint8_t
                for (int i = 0; i < 8; ++i)
                    childNodeMask |= bool(node->children[i]) << i;
                writeLE<uint8_t> (out, childNodeMask);
                writeLE<uint64_t>(out, node->dataOffset);
                writeLE<uint32_t>(out, node->numVoxels);
                writeLE<uint32_t>(out, node->numPoints);
                // Backward iteration here ensures children are ordered from 0
                // to 7 on disk.
                for (int i = 7; i >= 0; --i)
                {
                    const IndexNode* n = node->children[i].get();
                    if (n)
                        nodeStack.push_back(n);
                }
            }
        }

        HCloudHeader m_header;
        std::ostream& m_output;
        int m_brickRes;
        int64_t m_writtenNodeCount;
        bool m_hasNodes;
        std::vector<OctreeLevelInfo> m_levelInfo;
        std::unique_ptr<IndexNode> m_rootNode;

        bool m_debugPlot;
        dpz::PointList m_dpoints;

        Logger& m_logger;
};

