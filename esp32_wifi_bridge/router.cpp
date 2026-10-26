#include "router.h"

Router::Router(uint8_t exitNode, uint8_t startNode)
    : exitNode_(exitNode), current_(startNode) {}

// ---------------------------------------------------------------- objetivos

bool Router::addTarget(uint8_t node) {
  if (node >= NODE_COUNT || targetCount_ >= MAX_TARGETS) return false;
  for (uint8_t i = 0; i < targetCount_; ++i) {
    if (targets_[i] == node) return false;  // ya está en la lista
  }
  targets_[targetCount_++] = node;
  routeActive_ = true;
  dirty_ = true;
  return true;
}

void Router::clearTargets() {
  targetCount_ = 0;
  routeActive_ = false;
  plan_ = Route{};
  planIndex_ = 0;
  hopInFlight_ = '-';
  dirty_ = false;  // no hay nada que replantear: se espera al usuario
  pathText_[0] = '\0';
}

const char *Router::targetsText() {
  for (uint8_t i = 0; i < targetCount_; ++i) {
    targetsText_[i] = nodeLetter(targets_[i]);
  }
  targetsText_[targetCount_] = '\0';
  return targetsText_;
}

void Router::removeTarget(uint8_t node) {
  for (uint8_t i = 0; i < targetCount_;) {
    if (targets_[i] == node) {
      for (uint8_t j = i; j + 1 < targetCount_; ++j) targets_[j] = targets_[j + 1];
      --targetCount_;
    } else {
      ++i;
    }
  }
}

// ------------------------------------------------------------------ posición

void Router::setCurrent(uint8_t node) {
  if (node >= NODE_COUNT || node == current_) return;
  current_ = node;
  removeTarget(node);   // el objetivo visitado deja de pedirse
  hopInFlight_ = '-';   // el salto que traía terminó
  dirty_ = true;
}

// --------------------------------------------------------------------- plan

void Router::ensurePlan() {
  // Con un salto en vuelo no se replantea: el dashboard y el Wio deben seguir
  // mostrando el mismo camino que el auto está recorriendo.
  if (hopInFlight_ != '-' || !dirty_) return;
  replan();
}

void Router::replan() {
  planIndex_ = 0;
  dirty_ = false;

  if (targetCount_ == 0) {
    // Sin objetivos: si la ruta estaba activa, se cierra yendo a la salida.
    if (routeActive_ && current_ != exitNode_) {
      plan_ = shortestPath(current_, exitNode_);
    } else {
      plan_ = Route{};
      routeActive_ = false;
    }
    return;
  }

  plan_ = calculateRoute(current_, targets_, targetCount_, exitNode_);
}

char Router::takeNextHop() {
  if (hopInFlight_ != '-') return hopInFlight_;  // reentrega idempotente

  ensurePlan();
  while (plan_.valid && planIndex_ < plan_.length &&
         plan_.nodes[planIndex_] == current_) {
    ++planIndex_;  // el nodo actual no es un salto
  }
  if (!plan_.valid || planIndex_ >= plan_.length) return '-';

  hopInFlight_ = nodeLetter(plan_.nodes[planIndex_++]);
  return hopInFlight_;
}

char Router::peekNextHop() {
  if (hopInFlight_ != '-') return hopInFlight_;
  ensurePlan();

  uint8_t i = planIndex_;
  while (plan_.valid && i < plan_.length && plan_.nodes[i] == current_) ++i;
  if (!plan_.valid || i >= plan_.length) return '-';
  return nodeLetter(plan_.nodes[i]);
}

bool Router::hasNextHop() { return peekNextHop() != '-'; }

const char *Router::remainingPath() {
  ensurePlan();
  if (!plan_.valid) {
    pathText_[0] = '\0';
    return pathText_;
  }
  for (uint8_t i = 0; i < plan_.length; ++i) {
    pathText_[i] = nodeLetter(plan_.nodes[i]);
  }
  pathText_[plan_.length] = '\0';
  return pathText_;
}

uint16_t Router::plannedCost() {
  ensurePlan();
  return plan_.valid ? plan_.cost : 0;
}
