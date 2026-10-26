/*
 * ESP32-C3 WiFi Bridge & Web Control Dashboard (con Joystick Virtual)
 * Board: Seeed Studio XIAO ESP32C3
 *
 * Funciones:
 *   1. Crea red WiFi Access Point "Robot-Autonomo" (192.168.4.1).
 *   2. Servidor Web con JOYSTICK VIRTUAL táctil para celular.
 *   3. Radar de Visión en vivo (Grove Vision AI V2).
 *   4. Comunicación Serial bidireccional con Wio Terminal (D6 TX, D7 RX).
 */

#include <WiFi.h>
#include <WebServer.h>
#include "graph.h"
#include "router.h"

const char* AP_SSID = "Robot-Autonomo";
const char* AP_PASS = "12345678";

WebServer server(80);

// Telemetría recibida del Wio Terminal
String robotStatus = "STANDBY";
int targetID = -1;
int score = 0;
int boxX = 0, boxY = 0, boxW = 0, boxH = 0;
int inferTime = 0;
int estDist = 0;
// millis() de la última trama TLM recibida del Wio (para marcar demora de enlace)
unsigned long lastTlmAt = 0;

// Ruta por nodos. El usuario encola objetivos (A..E) desde el celular; el
// Router calcula la ruta mínima (Dijkstra + orden óptimo TSP) y entrega al Wio
// un salto adyacente por REQ:NEXT. El Wio solo ejecuta y dibuja.
const uint8_t NODO_SALIDA = 4;   // E: la ruta siempre termina en la salida
Router router(NODO_SALIDA, 0);   // arranca en A (entrada)
uint8_t dirActual = DIR_NONE;    // dirección reportada por el Wio en POS:

// Hardware Serial hacia Wio Terminal (RX=D7, TX=D6)
#define WIO_RX_PIN D7
#define WIO_TX_PIN D6

const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
<title>Robot Control Dashboard</title>
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
  body { background: #11131c; color: #f0f2f5; text-align: center; padding: 10px; touch-action: manipulation; user-select: none; }
  h1 { font-size: 1.2rem; color: #00d2ff; margin-bottom: 4px; letter-spacing: 1px; }
  .badge { display: inline-block; padding: 4px 14px; border-radius: 12px; font-weight: bold; font-size: 0.85rem; margin-bottom: 8px; background: #262938; color: #a0aec0; }
  .badge.active { background: #00c853; color: #fff; }
  .badge.alert { background: #ff3d00; color: #fff; }

  /* Latencia / demora de enlace (paleta tipo iOS) */
  .latency-row { display: flex; justify-content: center; gap: 8px; margin-bottom: 10px; }
  .latency-chip {
    --c: #8e8e93;
    display: inline-flex; align-items: center; gap: 6px;
    padding: 4px 10px 4px 9px; border-radius: 999px;
    background: rgba(118, 118, 128, 0.22);
    font-size: 0.7rem; font-weight: 600; color: #e5e5ea;
    letter-spacing: 0.2px;
  }
  .latency-chip .dot { width: 8px; height: 8px; border-radius: 50%; background: var(--c); box-shadow: 0 0 6px var(--c); transition: background 0.3s, box-shadow 0.3s; }
  .latency-chip .lbl { color: #a1a1aa; font-weight: 500; }
  .latency-chip .val { font-variant-numeric: tabular-nums; min-width: 46px; text-align: right; }
  .latency-chip[data-state="good"] { --c: #34c759; }
  .latency-chip[data-state="ok"]   { --c: #ffd60a; }
  .latency-chip[data-state="warn"] { --c: #ff9f0a; }
  .latency-chip[data-state="bad"]  { --c: #ff453a; }
  .latency-chip[data-state="idle"] { --c: #8e8e93; }

  /* Vision Canvas */
  .vision-box { background: #191c2b; border: 2px solid #282d42; border-radius: 12px; padding: 6px; margin: 0 auto 10px auto; max-width: 280px; }
  canvas { background: #0a0b12; border-radius: 8px; width: 100%; max-width: 250px; height: 210px; display: block; margin: 0 auto; }
  
  .telemetry { display: flex; justify-content: space-around; background: #191c2b; border-radius: 10px; padding: 8px; margin-bottom: 10px; font-size: 0.78rem; max-width: 320px; margin-left: auto; margin-right: auto; }
  .telemetry div { flex: 1; }
  .telemetry strong { display: block; font-size: 0.95rem; color: #00d2ff; margin-top: 2px; }

  /* Joystick Virtual */
  .joy-wrapper { display: flex; flex-direction: column; align-items: center; justify-content: center; margin-bottom: 12px; }
  .joy-base {
    position: relative;
    width: 170px;
    height: 170px;
    background: radial-gradient(circle, #252b3e 0%, #151824 100%);
    border: 3px solid #384261;
    border-radius: 50%;
    touch-action: none;
    box-shadow: inset 0 0 15px rgba(0,0,0,0.7), 0 4px 12px rgba(0,0,0,0.5);
  }
  .joy-knob {
    position: absolute;
    top: 55px;
    left: 55px;
    width: 60px;
    height: 60px;
    background: radial-gradient(circle at 35% 35%, #00e5ff 0%, #0077b6 100%);
    border-radius: 50%;
    box-shadow: 0 4px 15px rgba(0, 229, 255, 0.4);
    pointer-events: none;
    transform: translate(0px, 0px);
  }

  /* Action Buttons */
  .actions { display: flex; gap: 8px; max-width: 300px; margin: 0 auto; }
  .btn-mode { flex: 1; padding: 12px; border-radius: 10px; border: none; font-weight: bold; font-size: 0.85rem; cursor: pointer; }
  .btn-auto { background: #00c853; color: #fff; box-shadow: 0 3px 8px rgba(0,200,83,0.3); }
  .btn-auto:active { background: #5efc82; }
  .btn-stop { background: #d32f2f; color: #fff; box-shadow: 0 3px 8px rgba(211,47,47,0.3); }
  .btn-stop:active { background: #ff6659; }

  /* Ruta por nodos */
  .route { background: #191c2b; border-radius: 10px; padding: 8px; margin: 0 auto 10px auto; max-width: 320px; font-size: 0.8rem; }
  .route-head { font-size: 0.72rem; color: #8892b0; margin-bottom: 4px; }
  .route strong { color: #00d2ff; font-size: 1rem; }
  .node-row { display: flex; gap: 6px; margin-bottom: 8px; }
  .btn-node { flex: 1; padding: 10px 0; border-radius: 8px; border: 2px solid #384261; background: #262938; color: #f0f2f5; font-weight: bold; font-size: 1rem; cursor: pointer; }
  .btn-node:active { background: #384261; }
  .route.pos-mode .btn-node { border-color: #ffab00; }
  .route-actions { display: flex; gap: 6px; }
  .btn-small { flex: 1; padding: 8px; border-radius: 8px; border: none; background: #262938; color: #f0f2f5; font-size: 0.78rem; cursor: pointer; }
  .route.pos-mode #btnPos { background: #ffab00; color: #11131c; }

  /* Ruta óptima: flecha de dirección + camino calculado (paleta iOS) */
  .route-nav { display: flex; align-items: center; gap: 10px; margin: 6px 0 10px 0; text-align: left; }
  .dir-arrow {
    flex: 0 0 46px; height: 46px; border-radius: 14px;
    display: flex; align-items: center; justify-content: center;
    background: rgba(10, 132, 255, 0.16); color: #64d2ff;
    font-size: 1.7rem; line-height: 1; font-weight: 700;
    transition: transform 0.25s ease, color 0.25s, background 0.25s;
  }
  .dir-arrow.idle { background: rgba(118, 118, 128, 0.22); color: #8e8e93; }
  .route-info { flex: 1; min-width: 0; }
  .route-next { font-size: 0.74rem; color: #a1a1aa; }
  .route-next strong { color: #30d158; font-size: 1.05rem; }
  .dir-name {
    display: inline-block; margin-left: 6px; padding: 1px 8px; border-radius: 999px;
    background: rgba(10, 132, 255, 0.16); color: #64d2ff;
    font-size: 0.64rem; font-weight: 600; letter-spacing: 0.3px;
  }
  .route-path { margin-top: 4px; font-size: 0.9rem; color: #f0f2f5; font-weight: 700; overflow-wrap: anywhere; }
  .route-path .here { color: #ff453a; }
  .route-path .next { color: #30d158; }
  .route-path .sep { color: #64748b; font-weight: 400; }
  .route-meta { margin-top: 4px; font-size: 0.66rem; color: #718096; }
</style>
</head>
<body>

<h1>AUTO AUTÓNOMO</h1>
<div id="statusBadge" class="badge">CONECTANDO...</div>

<div class="latency-row">
  <div class="latency-chip" id="chipWifi" data-state="idle" title="Ida y vuelta de la petición HTTP (Wi-Fi)">
    <span class="dot"></span><span class="lbl">Wi-Fi</span><span class="val" id="valWifi">--</span>
  </div>
  <div class="latency-chip" id="chipLink" data-state="idle" title="Tiempo desde la última telemetría del Wio Terminal (UART)">
    <span class="dot"></span><span class="lbl">Enlace</span><span class="val" id="valLink">--</span>
  </div>
</div>

<div class="vision-box">
  <div style="font-size: 0.72rem; color: #8892b0; margin-bottom: 4px;">RADAR DE VISIÓN (Grove Vision AI V2)</div>
  <canvas id="radar" width="240" height="240"></canvas>
</div>

<div class="telemetry">
  <div>ID<strong id="tlmID">--</strong></div>
  <div>Confianza<strong id="tlmScore">--%</strong></div>
  <div>Distancia<strong id="tlmDist">-- cm</strong></div>
  <div>Inferencia<strong id="tlmInfer">-- ms</strong></div>
</div>

<!-- RUTA POR NODOS -->
<div class="route" id="routeBox">
  <div class="route-head">RUTA ÓPTIMA POR NODOS</div>
  <div class="route-nav">
    <div class="dir-arrow idle" id="dirArrow">&#8593;</div>
    <div class="route-info">
      <div class="route-next">Siguiente <strong id="rNext">-</strong><span class="dir-name" id="rDir">EN ESPERA</span></div>
      <div class="route-path" id="rPath">—</div>
      <div class="route-meta">Pedido: <span id="rTargets">—</span> &middot; Coste <span id="rCost">0</span></div>
    </div>
  </div>
  <div class="node-row">
    <button class="btn-node" onclick="nodeTap('A')">A</button>
    <button class="btn-node" onclick="nodeTap('B')">B</button>
    <button class="btn-node" onclick="nodeTap('C')">C</button>
    <button class="btn-node" onclick="nodeTap('D')">D</button>
    <button class="btn-node" onclick="nodeTap('E')">E</button>
  </div>
  <div class="route-actions">
    <button class="btn-small" id="btnPos" onclick="togglePosMode()">📍 Fijar posición</button>
    <button class="btn-small" onclick="routeCmd('clear=1')">Borrar ruta</button>
  </div>
</div>

<!-- JOYSTICK VIRTUAL -->
<div class="joy-wrapper">
  <div class="joy-base" id="joyBase">
    <div class="joy-knob" id="joyKnob"></div>
  </div>
  <div style="font-size: 0.75rem; color: #718096; margin-top: 6px;">Desliza para manejar · Suelta para frenar</div>
</div>

<div class="actions">
  <button class="btn-mode btn-auto" onclick="sendCmd('AUTO')">🚀 INICIAR AUTÓNOMO</button>
  <button class="btn-mode btn-stop" onclick="sendCmd('STOP')">🛑 FRENAR</button>
</div>

<script>
// --- RADAR VISIÓN ---
const canvas = document.getElementById('radar');
const ctx = canvas.getContext('2d');

function updateRadar(target) {
  ctx.clearRect(0, 0, 240, 240);
  ctx.strokeStyle = '#1e2436';
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.moveTo(120, 0); ctx.lineTo(120, 240);
  ctx.moveTo(0, 120); ctx.lineTo(240, 120);
  ctx.stroke();

  ctx.strokeStyle = '#232a40';
  ctx.beginPath();
  ctx.arc(120, 120, 45, 0, 2 * Math.PI);
  ctx.arc(120, 120, 90, 0, 2 * Math.PI);
  ctx.stroke();

  if (target.score > 20 && target.w > 0) {
    ctx.strokeStyle = '#00d2ff';
    ctx.lineWidth = 2;
    ctx.fillStyle = 'rgba(0, 210, 255, 0.18)';
    ctx.fillRect(target.x, target.y, target.w, target.h);
    ctx.strokeRect(target.x, target.y, target.w, target.h);

    ctx.fillStyle = '#00d2ff';
    ctx.font = 'bold 12px sans-serif';
    ctx.fillText('ID:' + target.id + ' (' + target.score + '%)', target.x + 2, Math.max(14, target.y - 4));
  }
}

// --- LATENCIA / DEMORA (paleta tipo iOS) ---// Wi-Fi: ida y vuelta real de /telemetry. Enlace: antigüedad de la última
// telemetría recibida por el Wio (el Wio manda TLM cada 200 ms).
let wifiEma = null;

function setChip(chipId, valId, ms, limits) {
  const chip = document.getElementById(chipId);
  const val = document.getElementById(valId);
  if (ms == null || ms < 0 || !isFinite(ms)) {
    chip.dataset.state = 'idle';
    val.innerText = '--';
    return;
  }
  val.innerText = Math.round(ms) + ' ms';
  if (ms <= limits[0]) chip.dataset.state = 'good';
  else if (ms <= limits[1]) chip.dataset.state = 'ok';
  else if (ms <= limits[2]) chip.dataset.state = 'warn';
  else chip.dataset.state = 'bad';
}

// --- RUTA ÓPTIMA ---
// El ESP32 calcula el camino (Dijkstra + orden óptimo). Acá solo se dibuja la
// dirección del próximo salto y el camino restante.
const DIR_DEG = [0, 0, 180, 90, 270, 45, 315, 135, 225];

function renderRoute(d) {
  const hasNext = d.next && d.next !== '-';
  const dir = Number(d.dir) || 0;
  const arrow = document.getElementById('dirArrow');
  arrow.style.transform = 'rotate(' + (DIR_DEG[dir] || 0) + 'deg)';
  arrow.classList.toggle('idle', !hasNext || dir === 0);

  document.getElementById('rNext').innerText = hasNext ? d.next : '-';
  document.getElementById('rDir').innerText = hasNext ? d.dirName : 'EN ESPERA';

  const path = d.path || '';
  const rp = document.getElementById('rPath');
  if (!path) {
    rp.innerText = '—';
  } else {
    rp.innerHTML = path.split('').map((ch, i) => {
      const cls = i === 0 ? 'here' : (i === 1 ? 'next' : '');
      return '<span class="' + cls + '">' + ch + '</span>';
    }).join(' <span class="sep">→</span> ');
  }

  document.getElementById('rTargets').innerText =
    d.targets ? d.targets.split('').join(' → ') : '—';
  document.getElementById('rCost').innerText = d.cost || 0;
}

// --- TELEMETRÍA ---
function fetchTelemetry() {
  const t0 = performance.now();
  fetch('/telemetry', { cache: 'no-store' })
    .then(r => r.json())
    .then(d => {
      const rtt = performance.now() - t0;
      wifiEma = wifiEma == null ? rtt : wifiEma * 0.7 + rtt * 0.3;
      setChip('chipWifi', 'valWifi', wifiEma, [40, 90, 180]);
      setChip('chipLink', 'valLink', d.age, [350, 700, 1400]);

      document.getElementById('statusBadge').innerText = d.status;
      if (d.status === 'AUTONOMO') {
        document.getElementById('statusBadge').className = 'badge active';
      } else if (d.status.includes('LLEGADA') || d.status.includes('STOP')) {
        document.getElementById('statusBadge').className = 'badge alert';
      } else {
        document.getElementById('statusBadge').className = 'badge';
      }

      document.getElementById('tlmID').innerText = d.id >= 0 ? d.id : '--';
      document.getElementById('tlmScore').innerText = d.score > 0 ? d.score + '%' : '--';
      document.getElementById('tlmDist').innerText = d.dist > 0 ? d.dist + ' cm' : '--';
      document.getElementById('tlmInfer').innerText = d.infer > 0 ? d.infer + ' ms' : '--';

      renderRoute(d);
      updateRadar(d);
    })
    .catch(() => {
      // Sin respuesta: el AP o el ESP32 no contestan.
      wifiEma = null;
      document.getElementById('chipWifi').dataset.state = 'bad';
      document.getElementById('valWifi').innerText = 'ERR';
      setChip('chipLink', 'valLink', -1, []);
    });
}
setInterval(fetchTelemetry, 300);

// --- JOYSTICK TÁCTIL ---
const joyBase = document.getElementById('joyBase');
const joyKnob = document.getElementById('joyKnob');
let isDragging = false;
let joyCenter = { x: 0, y: 0 };
const maxRadius = 50;
let lastDriveSend = 0;

function getEventPos(e) {
  if (e.touches && e.touches.length > 0) {
    return { x: e.touches[0].clientX, y: e.touches[0].clientY };
  }
  return { x: e.clientX, y: e.clientY };
}

function startDrag(e) {
  isDragging = true;
  const rect = joyBase.getBoundingClientRect();
  joyCenter = { x: rect.left + rect.width / 2, y: rect.top + rect.height / 2 };
  handleDrag(e);
}

function handleDrag(e) {
  if (!isDragging) return;
  e.preventDefault();
  const pos = getEventPos(e);
  let dx = pos.x - joyCenter.x;
  let dy = pos.y - joyCenter.y;
  let dist = Math.sqrt(dx * dx + dy * dy);

  if (dist > maxRadius) {
    dx = (dx / dist) * maxRadius;
    dy = (dy / dist) * maxRadius;
  }

  joyKnob.style.transform = `translate(${dx}px, ${dy}px)`;

  // Normalizar entre -100 y 100
  // Invertir dy para que arriba sea positivo (+)
  let normX = Math.round((dx / maxRadius) * 100);
  let normY = Math.round((-dy / maxRadius) * 100);

  // Zona muerta central
  if (Math.abs(normX) < 12) normX = 0;
  if (Math.abs(normY) < 12) normY = 0;

  const now = Date.now();
  if (now - lastDriveSend > 70) {
    lastDriveSend = now;
    sendDrive(normX, normY);
  }
}

function stopDrag() {
  if (!isDragging) return;
  isDragging = false;
  joyKnob.style.transition = 'transform 0.15s ease-out';
  joyKnob.style.transform = 'translate(0px, 0px)';
  setTimeout(() => { joyKnob.style.transition = ''; }, 150);
  sendDrive(0, 0);
}

joyBase.addEventListener('touchstart', startDrag, { passive: false });
window.addEventListener('touchmove', handleDrag, { passive: false });
window.addEventListener('touchend', stopDrag);

joyBase.addEventListener('mousedown', startDrag);
window.addEventListener('mousemove', handleDrag);
window.addEventListener('mouseup', stopDrag);

function sendCmd(cmd) {
  fetch('/cmd?action=' + cmd).catch(() => {});
}

function sendDrive(x, y) {
  fetch(`/drive?x=${x}&y=${y}`).catch(() => {});
}

// --- RUTA POR NODOS ---
// Normal: tocar un nodo lo agrega al final de la ruta.
// Modo "Fijar posición": tocar un nodo le dice al auto que está ahí.
let posMode = false;

function togglePosMode() {
  posMode = !posMode;
  document.getElementById('routeBox').classList.toggle('pos-mode', posMode);
}

function nodeTap(n) {
  if (posMode) {
    routeCmd('pos=' + n);
    togglePosMode();
  } else {
    routeCmd('add=' + n);
  }
}

function routeCmd(q) {
  fetch('/route?' + q).then(fetchTelemetry).catch(() => {});
}
</script>
</body>
</html>
)rawliteral";

void handleRoot() {
    server.send_P(200, "text/html", DASHBOARD_HTML);
}

void handleCmd() {
    if (server.hasArg("action")) {
        String action = server.arg("action");
        Serial1.print("CMD:");
        Serial1.print(action);
        Serial1.print("\n");

        if (action == "AUTO") robotStatus = "AUTONOMO";
        else if (action == "STOP") robotStatus = "MANUAL - STOP";
        else robotStatus = "MANUAL - " + action;

        server.send(200, "application/json", "{\"status\":\"ok\"}");
    } else {
        server.send(400, "text/plain", "Missing action");
    }
}

void handleDrive() {
    if (server.hasArg("x") && server.hasArg("y")) {
        int x = server.arg("x").toInt();
        int y = server.arg("y").toInt();

        // Enviar coordenadas normalizadas del Joystick al Wio Terminal: JOY:x,y
        Serial1.print("JOY:");
        Serial1.print(x);
        Serial1.print(",");
        Serial1.print(y);
        Serial1.print("\n");

        if (x == 0 && y == 0) {
            robotStatus = "MANUAL - STOP";
        } else {
            robotStatus = "MANUAL - JOYSTICK";
        }

        server.send(200, "application/json", "{\"status\":\"ok\"}");
    } else {
        server.send(400, "text/plain", "Missing x or y");
    }
}

// --------------------------------------------------------------- ruta
// Glue UART/HTTP: toda la lógica vive en Router (router.h/.cpp).
bool letraAIndice(char c, uint8_t &out) {
    if (c < 'A' || c >= 'A' + NODE_COUNT) return false;
    out = (uint8_t)(c - 'A');
    return true;
}

// El Wio dibuja la ruta completa en el minimapa con esta trama.
void enviarPath() {
    Serial1.print("PATH:");
    Serial1.print(router.remainingPath());
    Serial1.print("\n");
}

void handleTelemetry() {
    const char next = router.peekNextHop();
    const String path = router.remainingPath();
    const String targets = router.targetsText();

    long age = lastTlmAt ? (long)(millis() - lastTlmAt) : -1;
    String json = "{";
    json += "\"status\":\"" + robotStatus + "\",";
    json += "\"id\":" + String(targetID) + ",";
    json += "\"score\":" + String(score) + ",";
    json += "\"x\":" + String(boxX) + ",";
    json += "\"y\":" + String(boxY) + ",";
    json += "\"w\":" + String(boxW) + ",";
    json += "\"h\":" + String(boxH) + ",";
    json += "\"infer\":" + String(inferTime) + ",";
    json += "\"dist\":" + String(estDist) + ",";
    json += "\"age\":" + String(age) + ",";
    json += "\"cur\":\"" + String(router.currentLetter()) + "\",";
    json += "\"next\":\"" + String(next) + "\",";
    json += "\"path\":\"" + path + "\",";
    json += "\"queue\":\"" + path.substring(1) + "\",";  // compat: sin el nodo actual
    json += "\"targets\":\"" + targets + "\",";
    json += "\"cost\":" + String(router.plannedCost()) + ",";
    json += "\"dir\":" + String(dirActual) + ",";
    json += "\"dirName\":\"" + String(dirName((GraphDirection)dirActual)) + "\"";
    json += "}";
    server.send(200, "application/json", json);
}

// /route?add=X encola un objetivo (el Router recalcula la ruta óptima),
// /route?pos=X fija la posición del auto, /route?clear=1 borra todo.
void handleRoute() {
    if (server.hasArg("add")) {
        uint8_t n;
        if (letraAIndice(server.arg("add").charAt(0), n)) router.addTarget(n);
    } else if (server.hasArg("pos")) {
        char n = server.arg("pos").charAt(0);
        uint8_t idx;
        if (letraAIndice(n, idx)) {
            router.setCurrent(idx);
            Serial1.print("SET:");
            Serial1.print(n);
            Serial1.print("\n");
        }
    } else if (server.hasArg("clear")) {
        router.clearTargets();
        enviarPath();  // el Wio borra la ruta dibujada
    } else {
        server.send(400, "text/plain", "Missing add, pos or clear");
        return;
    }
    server.send(200, "application/json", "{\"status\":\"ok\"}");
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    // Serial1 hacia Wio Terminal (RX=D7, TX=D6 a 115200)
    Serial1.begin(115200, SERIAL_8N1, WIO_RX_PIN, WIO_TX_PIN);

    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);

    server.on("/", HTTP_GET, handleRoot);
    server.on("/cmd", HTTP_GET, handleCmd);
    server.on("/drive", HTTP_GET, handleDrive);
    server.on("/telemetry", HTTP_GET, handleTelemetry);
    server.on("/route", HTTP_GET, handleRoute);

    server.begin();
    Serial.println("Servidor Web con Joystick listo!");
}

void loop() {
    server.handleClient();

    // Leer telemetría del Wio Terminal
    if (Serial1.available()) {
        String line = Serial1.readStringUntil('\n');
        line.trim();
        if (line.startsWith("TLM:")) {
            lastTlmAt = millis();
            line = line.substring(4);
            int vals[7];
            int lastPos = 0;
            for (int i = 0; i < 7; i++) {
                int commaPos = line.indexOf(',', lastPos);
                if (commaPos != -1) {
                    vals[i] = line.substring(lastPos, commaPos).toInt();
                    lastPos = commaPos + 1;
                } else {
                    vals[i] = line.substring(lastPos).toInt();
                    lastPos = line.length();
                }
            }
            targetID = vals[0];
            score = vals[1];
            boxX = vals[2];
            boxY = vals[3];
            boxW = vals[4];
            boxH = vals[5];
            inferTime = vals[6];

            if (lastPos < line.length()) {
                int nextComma = line.indexOf(',', lastPos);
                if (nextComma != -1) {
                    estDist = line.substring(lastPos, nextComma).toInt();
                    robotStatus = line.substring(nextComma + 1);
                } else {
                    estDist = line.substring(lastPos).toInt();
                }
            }
        } else if (line == "REQ:NEXT") {
            // El Wio pide el siguiente salto: se le entrega el próximo nodo del
            // plan (siempre adyacente) y la ruta restante para el minimapa.
            Serial1.print("NODE:");
            Serial1.print(router.takeNextHop());
            Serial1.print("\n");
            enviarPath();
        } else if (line.startsWith("POS:") && line.length() >= 9) {
            // POS:actual,siguiente,dir  (siguiente = '-' si no hay salto)
            uint8_t idx;
            if (letraAIndice(line.charAt(4), idx)) router.setCurrent(idx);
            int dirComma = line.indexOf(',', 7);
            if (dirComma != -1) {
                uint8_t dir = (uint8_t)line.substring(dirComma + 1).toInt();
                dirActual = dir <= DIR_SOUTH_WEST ? dir : DIR_NONE;
            }
        }
    }
}
