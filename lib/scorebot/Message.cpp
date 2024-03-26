#include <Message.hpp>

void StateRefreshResponse::update(const StateRefreshRequest& lastReceived) {
    // TODO
}

void StateRefreshRequest::update(StateRefreshResponse* responses, size_t nResponses) {
    for (size_t i=0; i < nResponses; ++i) {
        const StateRefreshResponse& response = responses[i];
    }
    // TODO
}
