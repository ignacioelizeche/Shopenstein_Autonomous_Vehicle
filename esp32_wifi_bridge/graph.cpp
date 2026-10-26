/*
 * graph.cpp - Dijkstra + ruta óptima multi-objetivo (TSP) para el ESP32.
 *
 * Port de graph.cpp/Descargas con la misma semántica:
 *   - Dijkstra con desempate por el menor índice de nodo.
 *   - calculateRoute prueba todas las permutaciones de los objetivos y se
 *     queda con la de menor costo, terminando siempre en el nodo de salida.
 *   - Sin objetivos no hay ruta (el auto espera en vez de irse solo a E).
 *
 * Ver la advertencia de sincronización de pesos en graph.h.
 */

#include "graph.h"

#include <algorithm>

namespace {

// Matriz simétrica de pesos. Espejo exacto de kEdges en GraphData.h.
const uint8_t kWeights[NODE_COUNT][NODE_COUNT] = {
    /*        A            B            C            D            E   */
    /* A */ {0,           2,           3,           NO_EDGE,     NO_EDGE},
    /* B */ {2,           0,           2,           3,           2},
    /* C */ {3,           2,           0,           2,           NO_EDGE},
    /* D */ {NO_EDGE,     3,           2,           0,           2},
    /* E */ {NO_EDGE,     2,           NO_EDGE,     2,           0},
};

bool validNode(uint8_t node) { return node < NODE_COUNT; }

// Une `segment` al final de `route`; false si no encadenan o no entra.
bool appendSegment(Route &route, const Route &segment) {
  if (!segment.valid || segment.length < 1 || route.length == 0) return false;
  if (route.nodes[route.length - 1] != segment.nodes[0]) return false;
  if (segment.length - 1 > MAX_ROUTE_NODES - route.length) return false;
  for (uint8_t i = 1; i < segment.length; ++i) {
    route.nodes[route.length++] = segment.nodes[i];
  }
  route.cost = static_cast<uint16_t>(route.cost + segment.cost);
  return true;
}

}  // namespace

uint8_t edgeWeight(uint8_t a, uint8_t b) {
  if (!validNode(a) || !validNode(b)) return NO_EDGE;
  return kWeights[a][b];
}

bool isEdge(uint8_t a, uint8_t b) { return edgeWeight(a, b) != NO_EDGE; }

const char *dirName(GraphDirection d) {
  switch (d) {
    case DIR_NORTH:      return "FORWARD";
    case DIR_SOUTH:      return "DOWNWARDS";
    case DIR_EAST:       return "RIGHT";
    case DIR_WEST:       return "LEFT";
    case DIR_NORTH_EAST: return "FWD-RIGHT";
    case DIR_NORTH_WEST: return "FWD-LEFT";
    case DIR_SOUTH_EAST: return "DOWN-RIGHT";
    case DIR_SOUTH_WEST: return "DOWN-LEFT";
    default:             return "STOPPED";
  }
}

Route shortestPath(uint8_t startNode, uint8_t endNode) {
  Route result;
  if (!validNode(startNode) || !validNode(endNode)) return result;

  uint16_t distance[NODE_COUNT] = {};
  int8_t previous[NODE_COUNT];
  bool reached[NODE_COUNT] = {};
  bool settled[NODE_COUNT] = {};
  for (uint8_t i = 0; i < NODE_COUNT; ++i) previous[i] = -1;
  reached[startNode] = true;

  for (uint8_t step = 0; step < NODE_COUNT; ++step) {
    int8_t current = -1;
    for (uint8_t node = 0; node < NODE_COUNT; ++node) {
      if (reached[node] && !settled[node] &&
          (current < 0 || distance[node] < distance[current])) {
        current = static_cast<int8_t>(node);
      }
    }
    if (current < 0) break;

    settled[current] = true;
    if (current == endNode) break;

    for (uint8_t next = 0; next < NODE_COUNT; ++next) {
      const uint8_t cost = kWeights[current][next];
      if (settled[next] || cost == NO_EDGE) continue;
      const uint16_t candidate =
          static_cast<uint16_t>(distance[current] + cost);
      if (!reached[next] || candidate < distance[next]) {
        reached[next] = true;
        distance[next] = candidate;
        previous[next] = current;
      }
    }
  }
  if (!settled[endNode]) return result;

  uint8_t reverse[NODE_COUNT];
  uint8_t length = 0;
  for (int8_t node = static_cast<int8_t>(endNode); node >= 0;
       node = previous[node]) {
    if (length == NODE_COUNT) return Route{};
    reverse[length++] = static_cast<uint8_t>(node);
  }

  result.length = length;
  result.cost = distance[endNode];
  result.valid = true;
  for (uint8_t i = 0; i < length; ++i) {
    result.nodes[i] = reverse[length - 1 - i];
  }
  return result;
}

Route calculateRoute(uint8_t startNode, const uint8_t targets[],
                     uint8_t targetCount, uint8_t exitNode) {
  Route best;
  if (!validNode(startNode) || !validNode(exitNode) ||
      targetCount > MAX_TARGETS || (targetCount > 0 && targets == nullptr)) {
    return best;
  }

  uint8_t order[MAX_TARGETS] = {};
  uint8_t count = 0;
  for (uint8_t i = 0; i < targetCount; ++i) {
    if (!validNode(targets[i])) return best;
    order[count++] = targets[i];
  }
  std::sort(order, order + count);
  count = static_cast<uint8_t>(std::unique(order, order + count) - order);

  // Sin objetivos no hay ruta: el auto no debe arrancar solo hacia la salida.
  if (count == 0) return best;

  do {
    Route candidate;
    candidate.nodes[0] = startNode;
    candidate.length = 1;
    candidate.valid = true;

    bool visited[NODE_COUNT] = {};
    visited[startNode] = true;

    for (uint8_t i = 0; i < count && candidate.valid; ++i) {
      if (visited[order[i]]) continue;
      const Route segment =
          shortestPath(candidate.nodes[candidate.length - 1], order[i]);
      candidate.valid = appendSegment(candidate, segment);
      if (candidate.valid) {
        for (uint8_t j = 0; j < segment.length; ++j) {
          visited[segment.nodes[j]] = true;
        }
      }
    }
    if (candidate.valid) {
      candidate.valid = appendSegment(
          candidate,
          shortestPath(candidate.nodes[candidate.length - 1], exitNode));
    }
    if (candidate.valid && (!best.valid || candidate.cost < best.cost)) {
      best = candidate;
    }
  } while (count > 1 && std::next_permutation(order, order + count));

  return best;
}
