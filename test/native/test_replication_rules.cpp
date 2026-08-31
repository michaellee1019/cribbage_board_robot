#if !defined(ARDUINO)

#include <ReplicationRules.hpp>

#include <cassert>
#include <iostream>

using scorebot::Revision;
using scorebot::RevisionOrder;
using scorebot::PendingDisposition;

int main() {
    assert(scorebot::compareRevision({2, 7}, {2, 7}) == RevisionOrder::Equal);
    assert(scorebot::compareRevision({2, 6}, {2, 7}) == RevisionOrder::Older);
    assert(scorebot::compareRevision({1, 99}, {2, 0}) == RevisionOrder::Older);
    assert(scorebot::compareRevision({2, 8}, {2, 7}) == RevisionOrder::Newer);
    assert(scorebot::compareRevision({3, 0}, {2, 99}) == RevisionOrder::Newer);
    assert(scorebot::reconcileOperationId(3, 9) == 9);
    assert(scorebot::reconcileOperationId(11, 9) == 11);
    assert(scorebot::operationAcknowledged(9, 9));
    assert(scorebot::operationAcknowledged(9, 10));
    assert(!scorebot::operationAcknowledged(9, 8));
    assert(!scorebot::operationAcknowledged(0, 10));
    assert(scorebot::pendingDisposition(false, false, false, true) ==
           PendingDisposition::Keep);
    assert(scorebot::pendingDisposition(true, true, false, false) ==
           PendingDisposition::Clear);
    assert(scorebot::pendingDisposition(true, false, true, true) ==
           PendingDisposition::Clear);
    assert(scorebot::pendingDisposition(true, false, false, true) ==
           PendingDisposition::Restore);
    assert(scorebot::pendingDisposition(true, false, false, false) ==
           PendingDisposition::Keep);
    std::cout << "Replication-rule tests passed\n";
}

#endif
