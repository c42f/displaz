// Copyright 2015, Christopher J. Foster and the other displaz contributors.
// Use of this code is governed by the BSD-style license found in LICENSE.txt

#ifndef DISPLAZ_OCTREE_BUILDER_H_INCLUDED
#define DISPLAZ_OCTREE_BUILDER_H_INCLUDED

#include <cassert>
#include <memory>
#include <vector>

#include "voxelizer.h"
#include "util.h"

/// Octree node holding data size and offset in the serialized stream
struct IndexNode
{
    NodeIndexData idata;
    std::unique_ptr<IndexNode> children[8];
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

        /// Serialize node data to output queue
        template<typename NodeDataT>
        std::unique_ptr<IndexNode> write(const NodeDataT& nodeData)
        {
            uint64_t dataOffset = m_bufferedBytes.tellp();
            std::unique_ptr<IndexNode> index(new IndexNode);
            index->idata = nodeData.serialize(m_bufferedBytes);
            index->idata.dataOffset = dataOffset;
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
                m_bufferedNodes[i]->idata.dataOffset += offset;
            out << m_bufferedBytes.rdbuf();
            // Clear buffers
            m_bufferedNodes.clear();
            m_bufferedBytes.str("");
            m_sizeBytes = 0;
        }

    private:
        std::vector<IndexNode*> m_bufferedNodes;
        uint64_t m_sizeBytes;
        std::stringstream m_bufferedBytes;
};



/// Class for building octrees in a bottom up fashion
///
/// The user must supply octree leaf nodes in Morton order; the internal nodes
/// will be built from these.  This correponds to a depth first traversal of
/// the whole tree, but only storing O(log(N)) full bricks in memory at any one
/// time.  As soon as a brick is no longer needed it is serialized to an output
/// queue and deallocated.  A lightweight index describing the node is kept and
/// written separately at the end of the file.
///
class OctreeBuilder
{
    public:
        OctreeBuilder(std::ostream& output, int brickRes, int leafDepth,
                      const Imath::V3d& positionOffset,
                      const Imath::Box3d& rootBound, Logger& logger)
            : m_output(output),
            m_brickRes(brickRes),
            m_levelInfo(leafDepth+2),
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
        }

        /// Add voxel brick and accompanying source points to the cloud
        void addNode(int level, int64_t mortonIndex,
                     std::unique_ptr<VoxelBrick> voxelBrick,
                     LeafPointData& leafPointData)
        {
            assert(level < (int)m_levelInfo.size() + 1);
            std::unique_ptr<IndexNode> brickIndex =
                writeNodeData(m_levelInfo[level].outputQueue, level, *voxelBrick);
            std::unique_ptr<IndexNode> pointsIndex =
                writeNodeData(m_levelInfo[level+1].outputQueue, level+1, leafPointData);
            // Link up leaf points as the first (and only) child of the brick.
            // Note that since we're not breaking the leaf points up into
            // octants, this introduces a special case when constructing
            // bounding boxes which can be detected using the node flags.
            brickIndex->children[0] = std::move(pointsIndex);
            addNode(level, mortonIndex, std::move(voxelBrick), std::move(brickIndex));
        }

        void finish()
        {
            // Sweep from leaves to root, flushing any last pending bricks
            int numInternalLevels = (int)m_levelInfo.size() - 1;
            for (int i = numInternalLevels - 1; i > 0; --i)
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
            int64_t processedNodeCount;
            NodeOutputQueue outputQueue;

            OctreeLevelInfo()
                : parentMortonIndex(INT64_MIN),
                pendingNodes(8),
                pendingIndexNodes(8),
                processedNodeCount(0)
            { }

            bool hasNodes() const { return parentMortonIndex != INT64_MIN; }

#           ifdef _MSC_VER
            // Workaround for MSVC broken automatically generated move
            // constructor/assignment.  These should never be called anyway.
            OctreeLevelInfo(OctreeLevelInfo&& rhs)
                : parentMortonIndex(INT64_MIN),
                pendingNodes(8),
                pendingIndexNodes(8),
                processedNodeCount(0)
            {
                assert(0 && "MSVC workaround.  Should not be called.");
            }
            OctreeLevelInfo& operator=(OctreeLevelInfo&& rhs)
            {
                assert(0 && "MSVC workaround.  Should not be called.");
                return *this;
            }
#           endif
        };

        void addNode(int level, int64_t mortonIndex,
                     std::unique_ptr<VoxelBrick> node,
                     std::unique_ptr<IndexNode> indexNode)
        {
            assert(level < (int)m_levelInfo.size());
            OctreeLevelInfo& levelInfo = m_levelInfo[level];
            ++levelInfo.processedNodeCount;
            if (level == 0)
            {
                assert(indexNode);
                m_rootNode = std::move(indexNode);
                return;
            }
            int64_t parentIndex = mortonIndex/8;
            int childNumber = int(mortonIndex - 8*parentIndex);
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
                // When `node` isn't a child of the node at level-1, finish
                // and push it up the tree, replacing with the parent of
                // `node` so we can set `node` as a child.
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
            // Create new brick by downsampling childern at `level+1`
            VoxelBrick* brickChildren[8] = {0};
            for (int i = 0; i < 8; ++i)
                brickChildren[i] = levelInfo.pendingNodes[i].get();
            std::unique_ptr<VoxelBrick> brick(new VoxelBrick(m_brickRes));
            brick->renderFromBricks(brickChildren);
            // Serialize brick to queue
            std::unique_ptr<IndexNode> indexNode =
                writeNodeData(levelInfo.outputQueue, level, *brick);
            // Link child indices into newly created node index
            for (int i = 0; i < 8; ++i)
                indexNode->children[i] = std::move(levelInfo.pendingIndexNodes[i]);
            // Push new brick and index up the tree
            addNode(level - 1, levelInfo.parentMortonIndex,
                    std::move(brick), std::move(indexNode));
            // Deallocate bricks at current level
            for (int i = 0; i < 8; ++i)
                levelInfo.pendingNodes[i].reset();
        }

        template<typename NodeDataT>
        std::unique_ptr<IndexNode> writeNodeData(NodeOutputQueue& queue, int level,
                                                 const NodeDataT& nodeData)
        {
            std::unique_ptr<IndexNode> index = queue.write(nodeData);
            const size_t maxQueueBytes = 10*1024*1024;
            if (queue.sizeBytes() >= maxQueueBytes)
                flushQueue(queue, level);
            return std::move(index);
        }

        /// Flush the given queue, and log a message
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
                writeLE<uint8_t> (out, node->idata.flags);
                writeLE<uint64_t>(out, node->idata.dataOffset);
                writeLE<uint32_t>(out, node->idata.numPoints);
                writeLE<uint8_t> (out, childNodeMask);
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
        std::vector<OctreeLevelInfo> m_levelInfo;
        std::unique_ptr<IndexNode> m_rootNode;

        Logger& m_logger;
};


#endif // DISPLAZ_OCTREE_BUILDER_H_INCLUDED
