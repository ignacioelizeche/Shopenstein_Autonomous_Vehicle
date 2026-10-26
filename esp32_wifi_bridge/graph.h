/*
 * graph.h - Grafo del circuito y rutas mínimas para el ESP32 XIAO.
 *
 * Adaptado de graph.cpp (Dijkstra + orden óptimo de objetivos). El ESP32 es el
 * dueño de la ruta: recibe los objetivos que el usuario toca en el celular y
 * los convierte en una secuencia de nodos adyacentes que el Wio ejecuta de a
 * un salto por vez.
 *
 * IMPORTANTE - LOS PESOS DEBEN COINCIDIR CON robot_master/GraphData.h:
 *   A-B 2 · A-C 3 · B-C 2 · B-D 3 · B-E 2 · C-D 2 · D-E 2
 * (Entrada en A, salida en E). Si se agrega/quita una arista o cambia un peso,
 * hay que editar los dos archivos: Arduino no puede incluir headers de otra
 * carpeta de sketch, por eso la matriz está duplicada.
 */

#ifndef GRAPH_H
#define GRAPH_H

#include <stdint.h>

constexpr uint8_t NODE_COUNT = 5;
constexpr uint8_t MAX_TARGETS = 8;  // objetivos que el usuario puede encolar
constexpr uint8_t NO_EDGE = 0xFF;   // "sin arista" en la matriz de pesos

// Capacidad de una ruta completa: arranque + un tramo mínimo por objetivo
// (NODE_COUNT-1 saltos como máximo) + el tramo final a la salida.
constexpr uint8_t MAX_ROUTE_NODES = 1 + (MAX_TARGETS + 1) * (NODE_COUNT - 1);

// Mismo orden numérico que UiDirection en robot_master/GraphData.h, para que
// el Wio y el dashboard hablen el mismo idioma sin conversiones.
enum GraphDirection : uint8_t {
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

struct Route {
  uint8_t nodes[MAX_ROUTE_NODES] = {};
  uint8_t length = 0;
  uint16_t cost = 0;
  bool valid = false;
};

// Peso de la arista entre `a` y `b` (NO_EDGE si no están conectados).
uint8_t edgeWeight(uint8_t a, uint8_t b);

// ¿Existe una arista directa entre los dos nodos?
bool isEdge(uint8_t a, uint8_t b);

// Etiqueta corta de la dirección ("FORWARD", "RIGHT", ...). La dirección la
// calcula el Wio (es dueño de las coordenadas del mapa) y la manda en `POS:`;
// el ESP32 solo la traduce para el dashboard.
const char *dirName(GraphDirection d);

// Camino más corto entre dos nodos (Dijkstra). `valid` = false si no existe.
Route shortestPath(uint8_t startNode, uint8_t endNode);

// Ruta que pasa por todos los `targets` en el orden más barato (fuerza bruta
// sobre las permutaciones) y termina en `exitNode`. Sin objetivos devuelve una
// ruta inválida: el auto no arranca solo hacia la salida.
Route calculateRoute(uint8_t startNode, const uint8_t targets[],
                     uint8_t targetCount, uint8_t exitNode);

#endif
