#ifndef GRAPH_UI_H
#define GRAPH_UI_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "GraphData.h"

// Adaptado de GraphUI_18_10_26 para robot_master. Usa el `tft` global del
// sketch (ya inicializado con tft.begin() y setRotation(3)).

enum ButtonMask : uint8_t {
  BTN_NONE    = 0x00,
  BTN_KEY_A   = 0x01,
  BTN_KEY_B   = 0x02,
  BTN_KEY_C   = 0x04,
  BTN_UP      = 0x08,
  BTN_DOWN    = 0x10,
  BTN_LEFT    = 0x20,
  BTN_RIGHT   = 0x40,
  BTN_PRESS   = 0x80
};

// Telemetría que se muestra en la barra de estado y el panel inferior.
struct UiTelemetry {
  const char *estado;
  uint16_t    estadoColor;
  int         targetID;
  int         score;
  int         distCm;
  int         inferMs;
};

// Configura botones y buzzer.
void uiBegin();

// Dibuja el marco completo: flecha de dirección, FROM/TO y el grafo lateral
// con `cur` en rojo, `next` en verde y las aristas de `path` (ruta calculada
// por el ESP32, ej. "ABDE") en amarillo. Si hasNext es false, el auto está
// esperando que el ESP32 le mande el siguiente nodo. Llamar solo cuando
// cambian los nodos: borra la pantalla.
void uiShowRoute(UiNode cur, UiNode next, bool hasNext, const char *path);

// Redibuja solo la barra de estado y el panel de telemetría, sin parpadeo.
void uiShowTelemetry(const UiTelemetry &t);

// No bloqueante: devuelve los botones recién presionados desde la última llamada.
uint8_t uiPollButtons();

void uiBeep();

#endif
