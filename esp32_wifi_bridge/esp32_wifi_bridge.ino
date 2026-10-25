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
</style>
</head>
<body>

<h1>AUTO AUTÓNOMO</h1>
<div id="statusBadge" class="badge">CONECTANDO...</div>

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

// --- TELEMETRÍA ---
function fetchTelemetry() {
  fetch('/telemetry')
    .then(r => r.json())
    .then(d => {
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
      
      updateRadar(d);
    })
    .catch(() => {});
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

void handleTelemetry() {
    String json = "{";
    json += "\"status\":\"" + robotStatus + "\",";
    json += "\"id\":" + String(targetID) + ",";
    json += "\"score\":" + String(score) + ",";
    json += "\"x\":" + String(boxX) + ",";
    json += "\"y\":" + String(boxY) + ",";
    json += "\"w\":" + String(boxW) + ",";
    json += "\"h\":" + String(boxH) + ",";
    json += "\"infer\":" + String(inferTime) + ",";
    json += "\"dist\":" + String(estDist);
    json += "}";
    server.send(200, "application/json", json);
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
        }
    }
}
