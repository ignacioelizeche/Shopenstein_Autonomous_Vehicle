#ifndef GRAPH_DATA_H
#define GRAPH_DATA_H

#include <Arduino.h>
#include <TFT_eSPI.h>

enum UiNode : uint8_t {
  NODE_A = 0,
  NODE_B,
  NODE_C,
  NODE_D,
  NODE_E,
  NODE_COUNT
};

enum UiDirection : uint8_t {
  DIR_NONE = 0,
  DIR_NORTH,
  DIR_SOUTH,
  DIR_EAST,
  DIR_WEST,
  DIR_NORTH_EAST,
  DIR_NORTH_WEST,
  DIR_SOUTH_EAST,
  DIR_SOUTH_WEST
};

struct NodeDef {
  uint8_t  lx, ly;
  uint16_t color;
  char     shape;
  char     letter;
};

struct EdgeDef {
  UiNode a, b;
  uint8_t weight;
};

// Layout copied from the course graph image: Entrance at A (bottom),
// Exit at E (top). Local coords live in a 120 x 170 portrait box that
// gets drawn on the left half of the 320 x 240 landscape screen.
static const NodeDef kNodes[NODE_COUNT] = {
  { 35, 155, TFT_WHITE,  'S', 'A' },  // red star marker
  { 35,  85, TFT_GREEN,  'T', 'B' },  // blue triangle marker
  { 95,  85, TFT_CYAN,   'C', 'C' },  // green dot marker
  { 95,  15, TFT_BLUE,   'D', 'D' },  // yellow diamond marker
  { 35,  15, TFT_WHITE,  'Q', 'E' },  // purple square marker
};

static const EdgeDef kEdges[] = {
  { NODE_A, NODE_B, 2 },
  { NODE_A, NODE_C, 3 },
  { NODE_B, NODE_C, 2 },
  { NODE_B, NODE_D, 3 },
  { NODE_B, NODE_E, 2 },
  { NODE_C, NODE_D, 2 },
  { NODE_D, NODE_E, 2 },
};

static const uint8_t kEdgeCount = sizeof(kEdges) / sizeof(kEdges[0]);

// Peso de la arista que une `a` con `b`, o -1 si no son vecinos directos.
// Debe coincidir con la matriz de esp32_wifi_bridge/graph.cpp.
static inline int edgeWeight(UiNode a, UiNode b) {
  for (uint8_t i = 0; i < kEdgeCount; i++) {
    if ((kEdges[i].a == a && kEdges[i].b == b) ||
        (kEdges[i].a == b && kEdges[i].b == a)) {
      return (int)kEdges[i].weight;
    }
  }
  return -1;
}

static inline bool esArista(UiNode a, UiNode b) { return edgeWeight(a, b) > 0; }

// Dirección de mapa sólo entre nodos conectados: si no hay arista devuelve
// DIR_NONE, así la flecha nunca apunta "a través de la pared".
static inline UiDirection dirBetween(UiNode from, UiNode to) {
  if (from == to || !esArista(from, to)) return DIR_NONE;

  int dx = (int)kNodes[to].lx - (int)kNodes[from].lx;
  int dy = (int)kNodes[to].ly - (int)kNodes[from].ly;

  if (dx == 0) return dy < 0 ? DIR_NORTH : DIR_SOUTH;
  if (dy == 0) return dx < 0 ? DIR_WEST : DIR_EAST;
  if (dx < 0)  return dy < 0 ? DIR_NORTH_WEST : DIR_SOUTH_WEST;
  return dy < 0 ? DIR_NORTH_EAST : DIR_SOUTH_EAST;
}

// Labels shown next to the direction arrow. The map is drawn in its original
// orientation (Entrance A bottom, Exit E top), so "up" on screen is the way
// the car is meant to travel: that is what FORWARD means here.
// Longest entry is "DOWNWARDS" / "FWD-RIGHT" (10 chars), which fits Font 4
// inside the 196px-wide arrow pane.
static inline const char *dirName(UiDirection d) {
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

#endif
