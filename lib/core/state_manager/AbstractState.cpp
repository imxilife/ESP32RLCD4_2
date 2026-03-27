#include "AbstractState.h"
#include "StateManager.h"

void AbstractState::requestTransition(StateId id) {
    if (manager_) manager_->requestTransition(id);
}
