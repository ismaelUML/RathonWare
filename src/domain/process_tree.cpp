#include "process_tree.h"
#include <queue>
#include <algorithm>

namespace Rathon::Domain {

std::unordered_map<uint32_t, std::vector<uint32_t>> ProcessTreeResolver::buildAdjacencyMap(
    const std::vector<std::pair<uint32_t, uint32_t>>& pairs)
{
    std::unordered_map<uint32_t, std::vector<uint32_t>> adj;
    for (const auto& [parent, child] : pairs) {
        adj[parent].push_back(child);
    }
    return adj;
}

std::vector<uint32_t> ProcessTreeResolver::collectDescendantsBfs(
    uint32_t rootPid,
    const std::unordered_map<uint32_t, std::vector<uint32_t>>& tree)
{
    std::vector<uint32_t> order;
    std::unordered_set<uint32_t> visited;
    std::queue<uint32_t> queue;

    queue.push(rootPid);
    visited.insert(rootPid);

    while (!queue.empty()) {
        uint32_t current = queue.front();
        queue.pop();
        order.push_back(current);

        auto it = tree.find(current);
        if (it == tree.end()) {
            continue;
        }

        for (uint32_t child : it->second) {
            if (!isProtectedPid(child) && visited.insert(child).second) {
                queue.push(child);
            }
        }
    }

    return order;
}

std::vector<uint32_t> ProcessTreeResolver::resolveBottomUpKillOrder(
    uint32_t targetPid,
    const std::vector<std::pair<uint32_t, uint32_t>>& parentChildPairs)
{
    if (isProtectedPid(targetPid)) {
        return {};
    }

    auto tree = buildAdjacencyMap(parentChildPairs);
    auto traversal = collectDescendantsBfs(targetPid, tree);

    // Reverse top-down traversal to obtain bottom-up termination order (leafs first, root last)
    std::reverse(traversal.begin(), traversal.end());
    return traversal;
}

} // namespace Rathon::Domain
