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


/// Class for building octrees in a bottom up fashion
///
/// The user must supply octree leaf nodes in Morton order; the internal nodes
/// will be built from these
class OctreeBuilder
{
    public:
        OctreeBuilder(int brickRes, int leafDepth)
            : m_brickRes(brickRes),
            m_levelInfo(leafDepth+1)
        {
            m_dpoints.addAttribute<float>("position", 3)
                .addAttribute<float>("intensity", 1)
                .addAttribute<float>("coverage", 1)
                .addAttribute<int>("treeLevel", 1)
                .addAttribute<int>("leafIdx", 1);
        }

        void addNode(int level, int64_t mortonIndex,
                     std::unique_ptr<VoxelBrick> node)
        {
            assert(level < (int)m_levelInfo.size());
            OctreeLevelInfo& levelInfo = m_levelInfo[level];
            plotBrick(m_dpoints, *node, level, levelInfo.processedNodeCount);
            ++levelInfo.processedNodeCount;
            if (level == 0)
            {
                m_rootNode = std::move(node);
                return;
            }
            int64_t parentIndex = mortonIndex/8;
            if (!levelInfo.hasNodes())
            {
                // Special one time case for when no nodes are cached yet
                levelInfo.parentMortonIndex = parentIndex;
                levelInfo.pendingNodes[mortonIndex - 8*parentIndex] = std::move(node);
                return;
            }
            if (parentIndex != levelInfo.parentMortonIndex)
            {
                assert(levelInfo.parentMortonIndex < parentIndex);
                downsampleLevel(levelInfo, level);
                levelInfo.parentMortonIndex = parentIndex;
            }
            std::unique_ptr<VoxelBrick>& pendingSlot =
                levelInfo.pendingNodes[mortonIndex - 8*parentIndex];
            assert(!pendingSlot);
            pendingSlot = std::move(node);
        }

        void finish()
        {
            for (int i = (int)m_levelInfo.size() - 1; i > 0; --i)
                downsampleLevel(m_levelInfo[i], i);
            dpz::Displaz dwin;
            dwin.hold(true);
            dwin.plot(m_dpoints);
        }

        std::unique_ptr<VoxelBrick> root() { return std::move(m_rootNode); }

    private:
        struct OctreeLevelInfo
        {
            /// Morton index of parent node of currently pending nodes
            int64_t parentMortonIndex;
            /// List of pending nodes
            std::vector<std::unique_ptr<VoxelBrick>> pendingNodes;
            size_t processedNodeCount;

            OctreeLevelInfo()
                : parentMortonIndex(INT64_MIN),
                pendingNodes(8),
                processedNodeCount(0)
            { }

            bool hasNodes() const { return parentMortonIndex != INT64_MIN; }
        };

        void downsampleLevel(OctreeLevelInfo& levelInfo, int level)
        {
            // Construct downsampled node from current leaf node set
            VoxelBrick* children[8] = {0};
            for (int i = 0; i < 8; ++i)
                children[i] = levelInfo.pendingNodes[i].get();
            std::unique_ptr<VoxelBrick> brick(new VoxelBrick(m_brickRes));
            brick->renderFromBricks(children);
            addNode(level - 1, levelInfo.parentMortonIndex, std::move(brick));
            // Downsampling complete - flush more detailed levels to output
            // queue.
            for (int i = 0; i < 8; ++i)
                levelInfo.pendingNodes[i].reset();
        }

        int m_brickRes;
        int64_t m_brickIndex;
        bool m_hasNodes;
        std::vector<OctreeLevelInfo> m_levelInfo;
        std::unique_ptr<VoxelBrick> m_rootNode;

        // Debug
        dpz::PointList m_dpoints;
};

