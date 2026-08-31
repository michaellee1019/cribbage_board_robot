#pragma once

#include <algorithm>
#include <cstdint>

namespace scorebot {

struct Revision {
    uint32_t term{0};
    uint32_t version{0};
};

enum class RevisionOrder { Older, Equal, Newer };

constexpr RevisionOrder compareRevision(Revision incoming, Revision current) {
    if (incoming.term < current.term ||
        (incoming.term == current.term && incoming.version < current.version)) {
        return RevisionOrder::Older;
    }
    if (incoming.term == current.term && incoming.version == current.version) {
        return RevisionOrder::Equal;
    }
    return RevisionOrder::Newer;
}

// A valid equal revision is a heartbeat. It restores transport liveness and
// carries the high-water mark needed for idempotent player requests, but does
// not need another flash write.
constexpr uint32_t reconcileOperationId(uint32_t local, uint32_t replicated) {
    return std::max(local, replicated);
}

}  // namespace scorebot
