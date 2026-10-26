#include "GraphUI.h"

// Landscape 320x240 (setRotation(3) en el sketch).
static const int16_t SCREEN_W = 320;
static const int16_t SCREEN_H = 240;

// Layout: columna izquierda con estado, flecha, FROM/TO y telemetría; el
// grafo va en un panel lateral de alto completo, en su orientación original
// (Entrada A abajo, Salida E arriba), así "arriba" en pantalla es adelante.
//
//   +--------------------+-----------+
//   | ESTADO   0..22     |           |
//   +--------------------+           |
//   |   FLECHA 22..140   |  GRAFO    |
//   |                    |  196..320 |
//   +--------------------+  0..240   |
//   | FROM | TO 140..196 |           |
//   +--------------------+           |
//   | TELEMETRIA 196..240|           |
//   +--------------------+-----------+
static const int16_t PANEL_X = 196;
static const int16_t PANEL_W = SCREEN_W - PANEL_X;
static const int16_t PANEL_Y = 0;
static const int16_t PANEL_H = SCREEN_H;

// El mapa local mide 120x170; se centra dentro del panel.
static const int16_t MINI_X = PANEL_X + (PANEL_W - 120) / 2;
static const int16_t MINI_Y = (PANEL_H - 170) / 2;
static const int16_t NODE_R = 14;

static const int16_t COL_W    = PANEL_X;
static const int16_t STATUS_H = 22;
static const int16_t ARROW_Y  = STATUS_H;
static const int16_t ARROW_H  = 118;
static const int16_t NAMES_Y  = ARROW_Y + ARROW_H;
static const int16_t NAMES_H  = 56;
static const int16_t TLM_Y    = NAMES_Y + NAMES_H;

static const int16_t ARROW_CX  = COL_W / 2;
static const int16_t ARROW_CY  = ARROW_Y + 50;
static const int16_t DIRTEXT_Y = ARROW_Y + 104;
// La flecha original ocupa ~164 px de alto; aquí el panel es más bajo.
static const float   ARROW_SCALE = 0.7f;

static const uint16_t BAR_BG = 0x1082;

extern TFT_eSPI tft;

// ---------------------------------------------------------------- buttons

static const uint32_t DEBOUNCE_MS = 30;

static uint8_t readButtons() {
  uint8_t m = BTN_NONE;
  if (digitalRead(WIO_KEY_A)   == LOW) m |= BTN_KEY_A;
  if (digitalRead(WIO_KEY_B)   == LOW) m |= BTN_KEY_B;
  if (digitalRead(WIO_KEY_C)   == LOW) m |= BTN_KEY_C;
  if (digitalRead(WIO_5S_UP)   == LOW) m |= BTN_UP;
  if (digitalRead(WIO_5S_DOWN) == LOW) m |= BTN_DOWN;
  if (digitalRead(WIO_5S_LEFT) == LOW) m |= BTN_LEFT;
  if (digitalRead(WIO_5S_RIGHT)== LOW) m |= BTN_RIGHT;
  if (digitalRead(WIO_5S_PRESS)== LOW) m |= BTN_PRESS;
  return m;
}

// El loop del robot no puede bloquearse esperando un botón (motores y
// cámara), así que se detectan flancos de bajada con un antirrebote simple.
uint8_t uiPollButtons() {
  static uint8_t  stable = BTN_NONE;
  static uint8_t  lastRaw = BTN_NONE;
  static uint32_t lastChange = 0;

  uint8_t raw = readButtons();
  if (raw != lastRaw) {
    lastRaw = raw;
    lastChange = millis();
    return BTN_NONE;
  }
  if (millis() - lastChange < DEBOUNCE_MS || raw == stable) return BTN_NONE;

  uint8_t pressed = raw & ~stable;
  stable = raw;
  return pressed;
}

void uiBeep() {
  tone(WIO_BUZZER, 2000, 80);
}

// ------------------------------------------------------------- primitives

static int16_t nodeX(UiNode n) { return MINI_X + kNodes[n].lx; }
static int16_t nodeY(UiNode n) { return MINI_Y + kNodes[n].ly; }

// Light fills need dark ink to stay readable inside the 28px circle.
static uint16_t nodeInk(uint16_t fill) {
  return (fill == TFT_WHITE || fill == TFT_CYAN || fill == TFT_GREEN)
       ? TFT_BLACK : TFT_WHITE;
}

static void drawNodeFilled(UiNode n, uint16_t fill) {
  int16_t x = nodeX(n);
  int16_t y = nodeY(n);

  tft.fillCircle(x, y, NODE_R, fill);
  // Hairline ring so blue circles still read as separate objects where a
  // blue edge runs into them.
  tft.drawCircle(x, y, NODE_R, TFT_WHITE);

  char buf[2] = { kNodes[n].letter, '\0' };
  tft.setTextColor(nodeInk(fill), fill);
  tft.setTextDatum(MC_DATUM);
  // Font 2 (16px) fits inside the 28px circle. Font 4 would overflow it.
  tft.setTextFont(2);
  tft.drawString(buf, x, y);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
}

// Dashed vertical stub, used for the Entrance/Exit markers.
static void drawDashedV(int16_t x, int16_t y0, int16_t y1, uint16_t col) {
  int16_t step = (y1 > y0) ? 4 : -4;
  for (int16_t y = y0; (step > 0) ? (y < y1) : (y > y1); y += step) {
    int16_t yEnd = y + ((step > 0) ? 2 : -2);
    tft.drawLine(x, y, x, yEnd, col);
  }
}

// ------------------------------------------------------------------ uiBegin

void uiBegin() {
  pinMode(WIO_KEY_A,   INPUT_PULLUP);
  pinMode(WIO_KEY_B,   INPUT_PULLUP);
  pinMode(WIO_KEY_C,   INPUT_PULLUP);
  pinMode(WIO_5S_UP,   INPUT_PULLUP);
  pinMode(WIO_5S_DOWN, INPUT_PULLUP);
  pinMode(WIO_5S_LEFT, INPUT_PULLUP);
  pinMode(WIO_5S_RIGHT,INPUT_PULLUP);
  pinMode(WIO_5S_PRESS,INPUT_PULLUP);
  pinMode(WIO_BUZZER,  OUTPUT);
}

// ------------------------------------------------------------ minimap draw

// Entrance / Exit stubs, matching the dashed lines in the course image.
static void drawMapStubs() {
  tft.setTextDatum(TL_DATUM);
  tft.setTextFont(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  drawDashedV(nodeX(NODE_A), nodeY(NODE_A) + NODE_R + 2, PANEL_H - 8, TFT_BLUE);
  tft.drawString("IN", nodeX(NODE_A) + 10, PANEL_H - 20);

  drawDashedV(nodeX(NODE_E), PANEL_Y + 8, nodeY(NODE_E) - NODE_R - 2, TFT_BLUE);
  tft.drawString("OUT", nodeX(NODE_E) + 10, PANEL_Y + 8);
}

static void drawEdges(uint16_t color) {
  for (uint8_t i = 0; i < kEdgeCount; i++) {
    tft.drawLine(nodeX(kEdges[i].a), nodeY(kEdges[i].a),
                 nodeX(kEdges[i].b), nodeY(kEdges[i].b),
                 color);
  }
}

// Ruta calculada por el ESP32 ("ABDE"): se pinta en amarillo para que se vea
// el camino completo, no solo el próximo salto. Se dibuja en 2 px para que
// destaque sobre las aristas azules.
static void drawPathEdges(const char *path) {
  if (path == nullptr) return;
  for (const char *p = path; p[0] != '\0' && p[1] != '\0'; ++p) {
    int8_t a = (int8_t)(p[0] - 'A');
    int8_t b = (int8_t)(p[1] - 'A');
    if (a < 0 || a >= NODE_COUNT || b < 0 || b >= NODE_COUNT) continue;
    if (!esArista((UiNode)a, (UiNode)b)) continue;
    int16_t x0 = nodeX((UiNode)a), y0 = nodeY((UiNode)a);
    int16_t x1 = nodeX((UiNode)b), y1 = nodeY((UiNode)b);
    tft.drawLine(x0, y0, x1, y1, TFT_YELLOW);
    tft.drawLine(x0 + 1, y0, x1 + 1, y1, TFT_YELLOW);
  }
}

static void drawNodes(uint16_t fill) {
  for (uint8_t i = 0; i < NODE_COUNT; i++) {
    drawNodeFilled((UiNode)i, fill);
  }
}

// Panel lateral: grafo en azul, ruta planificada en amarillo, el nodo donde
// está el auto en rojo y el nodo al que va en verde.
static void drawGraphPanel(UiNode cur, UiNode next, bool hasNext, const char *path) {
  drawMapStubs();
  drawEdges(TFT_BLUE);
  drawPathEdges(path);
  drawNodes(TFT_BLUE);

  drawNodeFilled(cur, TFT_RED);
  if (!hasNext) return;

  if (cur == next) {
    // Staying put: red body with a green ring, so one node still shows both.
    tft.drawCircle(nodeX(next), nodeY(next), NODE_R + 3, TFT_GREEN);
    tft.drawCircle(nodeX(next), nodeY(next), NODE_R + 4, TFT_GREEN);
  } else {
    drawNodeFilled(next, TFT_GREEN);
  }
}

// --------------------------------------------------------------- arrow pane

// Clockwise rotation factors for the north-pointing base chevron: a screen
// y grows downward, so x' = x*c - y*s, y' = x*s + y*c with these values
// sends "up" to up-right at 45 deg, to right at 90 deg, and so on.
static void dirRot(UiDirection d, float &c, float &s) {
  const float k = 0.70710678f;
  switch (d) {
    case DIR_NORTH_EAST: c =  k; s =  k; break;
    case DIR_EAST:       c = 0.f; s = 1.f; break;
    case DIR_SOUTH_EAST: c = -k; s =  k; break;
    case DIR_SOUTH:      c = -1.f; s = 0.f; break;
    case DIR_SOUTH_WEST: c = -k; s = -k; break;
    case DIR_WEST:       c = 0.f; s = -1.f; break;
    case DIR_NORTH_WEST: c =  k; s = -k; break;
    case DIR_NORTH:
    default:             c = 1.f; s = 0.f; break;
  }
}

// Chevron pointing straight up in its base frame: head triangle plus a
// rectangular shaft, rotated as a whole.
static void drawArrow(UiDirection d) {
  static const int16_t px[7] = {   0, -45,  45, -14,  14, -14,  14 };
  static const int16_t py[7] = { -52, -22, -22, -22, -22,  52,  52 };

  if (d == DIR_NONE) {
    // No travel: grey cross instead of a chevron.
    for (int8_t o = -2; o <= 2; o++) {
      tft.drawLine(ARROW_CX - 24 + o, ARROW_CY - 24 + o,
                   ARROW_CX + 24 + o, ARROW_CY + 24 + o, TFT_DARKGREY);
      tft.drawLine(ARROW_CX - 24 + o, ARROW_CY + 24 + o,
                   ARROW_CX + 24 + o, ARROW_CY - 24 + o, TFT_DARKGREY);
    }
    return;
  }

  float c, s;
  dirRot(d, c, s);
  c *= ARROW_SCALE;
  s *= ARROW_SCALE;

  int16_t x[7], y[7];
  for (uint8_t i = 0; i < 7; i++) {
    float fx = (float)px[i] * c - (float)py[i] * s;
    float fy = (float)px[i] * s + (float)py[i] * c;
    x[i] = ARROW_CX + (int16_t)((fx >= 0.0f) ? (fx + 0.5f) : (fx - 0.5f));
    y[i] = ARROW_CY + (int16_t)((fy >= 0.0f) ? (fy + 0.5f) : (fy - 0.5f));
  }

  // Head, then the shaft quad as two triangles.
  tft.fillTriangle(x[0], y[0], x[1], y[1], x[2], y[2], TFT_CYAN);
  tft.fillTriangle(x[3], y[3], x[4], y[4], x[6], y[6], TFT_CYAN);
  tft.fillTriangle(x[3], y[3], x[6], y[6], x[5], y[5], TFT_CYAN);
}

// -------------------------------------------------------------- names pane

// FROM a la izquierda, TO a la derecha.
static void drawNamesRect(UiNode cur, UiNode next, bool hasNext) {
  struct { const char *label; char letter; uint16_t col; int16_t xc; } cols[2] = {
    { "FROM", kNodes[cur].letter,                TFT_RED,   COL_W / 4 },
    { "TO",   hasNext ? kNodes[next].letter : '-',
              hasNext ? TFT_GREEN : TFT_DARKGREY,           3 * COL_W / 4 },
  };

  char buf[2] = { '\0', '\0' };
  for (uint8_t i = 0; i < 2; i++) {
    tft.setTextDatum(TC_DATUM);
    tft.setTextFont(2);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString(cols[i].label, cols[i].xc, NAMES_Y + 4);

    // Font 4, not Font 6: Font 6 only ships 1234567890:-.apm, so it would
    // draw nothing at all for the letters A-E.
    buf[0] = cols[i].letter;
    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(4);
    tft.setTextColor(cols[i].col, TFT_BLACK);
    tft.drawString(buf, cols[i].xc, NAMES_Y + 36);
  }

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.setTextFont(2);
}

// Las líneas que separan los paneles.
static void drawDividers() {
  tft.drawFastVLine(PANEL_X, 0, SCREEN_H, TFT_DARKGREY);
  tft.drawFastHLine(0, NAMES_Y, COL_W, TFT_DARKGREY);
  tft.drawFastHLine(0, TLM_Y, COL_W, TFT_DARKGREY);
}

void uiShowRoute(UiNode cur, UiNode next, bool hasNext, const char *path) {
  tft.setTextSize(1);
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, COL_W, STATUS_H, BAR_BG);
  drawDividers();

  drawGraphPanel(cur, next, hasNext, path);

  // Direction comes from the node positions on the unrotated map, so the
  // arrow always agrees with what the map shows.
  UiDirection d = hasNext ? dirBetween(cur, next) : DIR_NONE;
  drawArrow(d);
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(4);
  tft.setTextColor(hasNext ? TFT_WHITE : TFT_DARKGREY, TFT_BLACK);
  tft.drawString(hasNext ? dirName(d) : "WAITING", ARROW_CX, DIRTEXT_Y);

  drawNamesRect(cur, next, hasNext);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
}

// ---------------------------------------------------------- telemetry pane

void uiShowTelemetry(const UiTelemetry &t) {
  tft.setTextSize(1);
  tft.setTextFont(2);

  // El padding borra el texto anterior sin fillRect, así no parpadea.
  tft.setTextDatum(MC_DATUM);
  tft.setTextPadding(COL_W - 4);
  tft.setTextColor(t.estadoColor, BAR_BG);
  tft.drawString(t.estado, COL_W / 2, STATUS_H / 2);

  char buf[40];
  tft.setTextDatum(TL_DATUM);
  tft.setTextPadding(COL_W - 8);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  if (t.score > 0) {
    snprintf(buf, sizeof(buf), "ID %d  %d%%", t.targetID, t.score);
  } else {
    snprintf(buf, sizeof(buf), "ID --");
  }
  tft.drawString(buf, 6, TLM_Y + 4);

  if (t.distCm > 0) {
    snprintf(buf, sizeof(buf), "~%d cm   %d ms", t.distCm, t.inferMs);
  } else {
    snprintf(buf, sizeof(buf), "-- cm   %d ms", t.inferMs);
  }
  tft.drawString(buf, 6, TLM_Y + 22);

  tft.setTextPadding(0);
}
