#pragma once

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>

namespace Rathon::Domain {

// Pure Domain Service for Process Tree operations.
// Encapsulates the algorithmic business logic for process tree resolution and safety rules:
// - Protected PID invariant (PIDs <= 4 must never be touched to protect OS integrity).
// - Breadth-First Search (BFS) descendant discovery.
// - Bottom-up reverse ordering so child workers are terminated before their root parent.
class ProcessTreeResolver {
public:
    static bool isProtectedPid(uint32_t pid) noexcept {
        return pid <= 4;
    }

    // Resolves all descendant PIDs of targetPid in bottom-up order (children first, target last).
    // Complexity: O(N) where N is number of processes. Cyclomatic complexity <= 5 per helper.
    static std::vector<uint32_t> resolveBottomUpKillOrder(
        uint32_t targetPid,
        const std::vector<std::pair<uint32_t, uint32_t>>& parentChildPairs
    );

private:
    static std::unordered_map<uint32_t, std::vector<uint32_t>> buildAdjacencyMap(
        const std::vector<std::pair<uint32_t, uint32_t>>& pairs
    );

    static std::vector<uint32_t> collectDescendantsBfs(
        uint32_t rootPid,
        const std::unordered_map<uint32_t, std::vector<uint32_t>>& tree
    );
};

} // namespace Rathon::Domain
