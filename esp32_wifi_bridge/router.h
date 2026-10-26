/*
 * router.h - Estado de la ruta del robot (ESP32).
 *
 * Máquina de estados sin dependencias de Arduino: el sketch solo le pasa los
 * eventos que llegan por HTTP/UART y ella devuelve el próximo salto, la ruta
 * restante y los objetivos. Así la lógica se puede probar en el host
 * (tests/router_test.cpp) y el .ino queda como simple glue.
 *
 * Invariantes:
 *   - Solo se entregan saltos que son aristas reales del grafo.
 *   - La ruta se replantea cuando el auto llega a un nodo, cuando el usuario
 *     agrega un objetivo o cuando corrige la posición.
 *   - Mientras hay un salto en vuelo no se recalcula nada: la ruta no cambia
 *     debajo de los pies del auto.
 *   - Sin objetivos no hay ruta; si ya había una en curso, se cierra en la
 *     salida (E).
 */

#ifndef ROUTER_H
#define ROUTER_H

#include <stdint.h>

#include "graph.h"

class Router {
 public:
  Router(uint8_t exitNode, uint8_t startNode);

  // --- objetivos (lo que el usuario toca en el dashboard) ---
  bool addTarget(uint8_t node);   // false si inválido, repetido o lista llena
  void clearTargets();            // borra objetivos y ruta
  uint8_t targetCount() const { return targetCount_; }
  const char *targetsText();      // "DE"

  // --- posición del auto (llegada real o corrección manual) ---
  void setCurrent(uint8_t node);
  uint8_t current() const { return current_; }
  char currentLetter() const { return nodeLetter(current_); }

  // --- saltos (un nodo adyacente por vez) ---
  char takeNextHop();             // consume el plan; '-' si no hay más
  char peekNextHop();             // no consume (para el dashboard)
  bool hasNextHop();

  // --- datos para dashboard y minimapa ---
  const char *remainingPath();    // "ABDE" desde el nodo actual
  uint16_t plannedCost();

 private:
  void ensurePlan();
  void replan();
  void removeTarget(uint8_t node);
  static char nodeLetter(uint8_t node) { return (char)('A' + node); }

  uint8_t exitNode_;
  uint8_t current_;
  uint8_t targets_[MAX_TARGETS] = {};
  uint8_t targetCount_ = 0;
  bool routeActive_ = false;

  Route plan_;
  uint8_t planIndex_ = 0;
  char hopInFlight_ = '-';
  bool dirty_ = true;

  char pathText_[MAX_ROUTE_NODES + 1] = {};
  char targetsText_[MAX_TARGETS + 1] = {};
};

#endif
