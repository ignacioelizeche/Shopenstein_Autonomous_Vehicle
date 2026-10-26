# Embedded System Design — Memoria de proyecto

## Hardware del proyecto

| Componente | Detalle |
|---|---|
| MCU | **Seeed Wio Terminal** (ATSAMD51, USB CDC en `/dev/ttyACM0`) |
| Wi-Fi | **Seeed XIAO ESP32-C3** (AP `Robot-Autonomo`, dashboard en `http://192.168.4.1`), UART con el Wio |
| Sensor IA | **Grove Vision AI V2** (Himax HX6538, ID `2886:802d`) |
| Conexión | Grove **I2C** (dirección `0x62`), cable de 4 pines |
| Cámara | OV5647 CSI conectada al Vision AI V2 |
| Modelo desplegado | `Person Detection--Swift YOLO` (clase: `person`), vía SenseCraft AI |

## Toolchain (ya instalada en esta máquina)

- `arduino-cli` → `~/.local/bin/arduino-cli`
- Núcleo: `Seeeduino:samd@1.8.6` (instalado)
- URL boards: `https://files.seeedstudio.com/arduino/package_seeeduino_boards_index.json`
- Librerías relevantes en `~/Arduino/libraries/`:
  - `Seeed_Arduino_SSCMA@1.0.3`
  - `ArduinoJson@7.4.3`
- FQBN: `Seeeduino:samd:seeed_wio_terminal`

### Comandos típicos

```bash
# Compilar
arduino-cli compile --fqbn Seeeduino:samd:seeed_wio_terminal <sketch_dir>

# Subir (el puerto aparece tras conectar el Wio Terminal)
arduino-cli upload -p /dev/ttyACM0 --fqbn Seeeduino:samd:seeed_wio_terminal <sketch_dir>

# Monitor serie (115200)
stty -F /dev/ttyACM0 115200 raw -echo
cat /dev/ttyACM0

# Pruebas del motor de rutas (host, sin placas)
tests/run_routing_tests.sh
```

- El núcleo **esp32** (para `esp32_wifi_bridge`, FQBN `esp32:esp32:XIAO_ESP32C3`) **sí** está instalado (`esp32:esp32@3.3.12`): el sketch compila y se sube desde esta máquina.

## Estructura de los sketches

| Sketch | Placa | Archivos |
|---|---|---|
| `robot_master/` | Wio Terminal | `robot_master.ino` (FSM, motores, visión, UART), `GraphUI.h/.cpp` (pantalla), `GraphData.h` (grafo de nodos y aristas) |
| `esp32_wifi_bridge/` | XIAO ESP32-C3 | `esp32_wifi_bridge.ino` (AP Wi-Fi, dashboard web, puente UART), `graph.h/.cpp` (Dijkstra + TSP), `router.h/.cpp` (estado de ruta) |
| `tests/` | host (g++) | `router_test.cpp` (pruebas del motor de rutas), `run_routing_tests.sh` |

- `GraphUI` está adaptado del proyecto `GraphUI_18_10_26` y usa el `TFT_eSPI tft` global del sketch (`extern`); no crea otro objeto ni llama a `tft.begin()`.
- El grafo (nodos A–E, aristas, posiciones en pantalla) está en `GraphData.h`. La cantidad de nodos del ESP32 es `NODE_COUNT` en `graph.h`; si se agrega un nodo hay que tocar **los dos** archivos y los botones A–E del dashboard.
- Pantalla LCD (320x240, `setRotation(3)`): a la derecha el grafo (actual en rojo, siguiente en verde, **ruta planificada en amarillo**); a la izquierda barra de estado, flecha de dirección, FROM/TO y telemetría (ID, score, distancia, inferencia). `uiShowRoute()` borra la pantalla y solo se llama cuando cambian los nodos; `uiShowTelemetry()` se refresca cada 400 ms usando text padding en lugar de `fillRect` (sin parpadeo).
- Font 6 de TFT_eSPI solo trae `1234567890:-.apm`: para letras de nodos usar Font 4.
- Dashboard web: debajo del badge de estado hay dos chips de latencia tipo iOS (`#34c759` verde / `#ffd60a` amarillo / `#ff9f0a` naranja / `#ff453a` rojo / `#8e8e93` gris). **Wi-Fi** = ida y vuelta real de `fetch('/telemetry')` con media móvil; **Enlace** = antigüedad de la última trama `TLM` del Wio (campo `age` del JSON, `-1` si todavía no llegó ninguna). Si el fetch falla, el chip Wi-Fi muestra `ERR` en rojo.
- Dashboard web, tarjeta de ruta: flecha grande rotada según `dir` (0 = sin rumbo), `Siguiente X`, el camino calculado (`A → B → D → E`, actual en rojo y próximo en verde), los objetivos pedidos y el costo total.

## Protocolo UART Wio ⇄ ESP32 (`Serial1`, 115200, líneas terminadas en `\n`)

| Sentido | Trama | Significado |
|---|---|---|
| ESP → Wio | `JOY:x,y` | Joystick, x/y en −100..100 |
| ESP → Wio | `CMD:AUTO` / `CMD:STOP` | Cambio de modo |
| ESP → Wio | `NODE:X` / `NODE:-` | Respuesta a `REQ:NEXT`: siguiente salto (siempre adyacente), o `-` si no hay más |
| ESP → Wio | `PATH:ABDE` | Ruta completa calculada por el ESP32 desde el nodo actual (el Wio la dibuja en amarillo). `PATH:` vacío = sin ruta |
| ESP → Wio | `SET:X` | El usuario fija desde el celular en qué nodo está el auto |
| Wio → ESP | `TLM:id,score,x,y,w,h,infer,dist,estado` | Telemetría cada 200 ms |
| Wio → ESP | `REQ:NEXT` | Pide el siguiente salto; se repite cada 500 ms mientras no tenga uno |
| Wio → ESP | `POS:actual,siguiente,dir` | Posición en el grafo (`siguiente` = `-` si no hay) y dirección de mapa 0..8 (`UiDirection`). Se envía al cambiar y junto con cada TLM |

Flujo de ruta: el usuario toca nodos (objetivos) en el dashboard → el ESP32 calcula el camino más corto y lo entrega salto por salto → el Wio pide con `REQ:NEXT`, recibe `NODE:X` + `PATH:...`, y dibuja flecha, TO y ruta. Al llegar (parada autónoma por marcador cercano, o **KEY_A / botón 5-way** a mano) el Wio marca el nodo como actual, emite un beep, envía `POS` y vuelve a pedir. Tras cada llegada el auto queda detenido hasta un nuevo `CMD:AUTO`.

## Ruta más corta (Dijkstra + TSP)

El ESP32 es el único que calcula rutas; el Wio solo ejecuta un salto por vez y dibuja. Archivos:

| Archivo | Contenido |
|---|---|
| `esp32_wifi_bridge/graph.h/.cpp` | Matriz de pesos, `shortestPath()` (Dijkstra), `calculateRoute()` (permutaciones de objetivos), `isEdge()`, `dirName()` |
| `esp32_wifi_bridge/router.h/.cpp` | Estado: objetivos, nodo actual, salto en vuelo, plan y camino restante. Sin dependencias de Arduino (testeable en host) |
| `esp32_wifi_bridge/esp32_wifi_bridge.ino` | Glue HTTP/UART: llama al `Router` y arma el JSON / las tramas |
| `robot_master/GraphData.h` | `kEdges` (pesos), `esArista()` y `dirBetween()` que devuelve `DIR_NONE` si no hay arista |

Reglas:

- **Solo aristas**: todo salto que recibe el Wio es un vecino directo; `dirBetween()` y el handler de `NODE:` rechazan cualquier otro par, así la flecha nunca cruza el plano.
- **Orden óptimo**: con varios objetivos se prueban todas las permutaciones y gana la de menor costo (TSP de `graph.cpp`). Ej.: tocar `D` y después `B` produce `A→B→D→E`, no `A→D→B→E`.
- **Salida E**: la ruta siempre cierra en `E` (`NODO_SALIDA = 4`). Si no quedan objetivos pero la ruta estaba activa, el último tramo va a `E`; sin objetivos y sin ruta activa el auto espera.
- **Desempate**: si dos caminos cuestan lo mismo gana el de menor índice de nodo (misma regla que `graph.cpp`). Ej.: `C→E` = `C→B→E` (costo 4).
- **Replanteo**: al llegar a un nodo, al agregar un objetivo o al corregir la posición. Mientras hay un salto en vuelo **no** se recalcula (la ruta no cambia bajo los pies).
- **Pesos duplicados**: Arduino no puede incluir headers entre carpetas de sketch, así que la matriz de `graph.cpp` y `kEdges` de `GraphData.h` deben coincidir (`A-B 2 · A-C 3 · B-C 2 · B-D 3 · B-E 2 · C-D 2 · D-E 2`). Si cambia una, cambiar la otra.
- Ejemplos verificados: `A→D` = `A,B,D` (5), `A→E` = `A,B,E` (4), `C→E` = `C,B,E` (4), `B→D` = `B,D` (3).

### Campos de `/telemetry` relacionados con la ruta

| Campo | Ejemplo | Significado |
|---|---|---|
| `cur` | `"A"` | Nodo actual del auto |
| `next` | `"B"` | Próximo salto (o `-`) |
| `path` | `"ABDE"` | Ruta completa desde el nodo actual |
| `targets` | `"DB"` | Objetivos pedidos por el usuario (orden de toque) |
| `cost` | `7` | Costo total del plan |
| `dir` / `dirName` | `1` / `"FORWARD"` | Dirección de mapa del próximo salto (la calcula el Wio y la manda en `POS:`) |
| `queue` | `"BDE"` | Compatibilidad: `path` sin el nodo actual |

### Pruebas del motor de rutas

```bash
tests/run_routing_tests.sh   # g++ en el host: Dijkstra, TSP, replanteo, borrado
```

La lógica de `graph.cpp`/`router.cpp` no depende de Arduino, así que se prueba sin placas; el sketch solo hace de glue.

## Pitfalls conocidos (lecciones de la sesión 2026-10-14)

### 1. Firma de `SSCMA::begin()` — orden de parámetros

```cpp
// CORRECTO (el 2º parámetro es rst, NO address):
bool begin(TwoWire *wire, int32_t rst, uint16_t address,
           uint32_t wait_delay, uint32_t clock);

AI.begin(&Wire, -1, 0x62, 5, 100000); // rst=-1 (sin reset)
// O también: AI.begin(&Wire); // usa defaults: rst=-1, addr=0x62, wait=2, clock=400k
```

**NUNCA** escribir `AI.begin(&Wire, 0x62, ...)` — eso usa `0x62` como pin de reset y desplaza todos los demás parámetros.

### 2. `invoke()` con `show=true` → `ENOTSUP` (ret=8)

`show=true` pide la imagen JPEG y exige buffer RX ≥16 KB. En SAMD el default es 4 KB:

```cpp
// MAL: retorna 8 (CMD_ENOTSUP) en Wio Terminal
AI.invoke(1, false, true);

// BIEN: sin imagen
AI.invoke(1, true, false);
```

Si se necesita imagen: `AI.set_rx_buffer(16 * 1024)` antes de invocar (Wio Terminal tiene RAM suficiente).

### 3. Parámetro `filter` invertido respecto a DIFFERED

El código de la librería hace `AT+INVOKE=times, !filter, filter`, o sea:

| `filter` | Comando AT | DIFFERED | RESULT_ONLY | Comportamiento |
|---|---|---|---|---|
| `false` | `AT+INVOKE=1,1,0` | 1 | 0 | Solo emite evento si el resultado **cambia** |
| `true` | `AT+INVOKE=1,0,1` | 0 | 1 | **Siempre** emite evento |

**Trampa**: con `filter=false` y sin personas en escena, `boxes` queda siempre `[]`, "nunca cambia", y la librería retorna timeout (`ret=3`) aunque la inferencia sí corrió. Usar `filter=true` para debugging/confiabilidad.

### 4. Estado inicial del módulo: BREAK + MODEL antes del primer invoke

Tras un arranque del módulo (o subida de sketch que resetea el Wio Terminal y corta poder Grove al Vision AI), conviene:

```cpp
AI.write("AT+BREAK\r\n", 10);   // matar tareas colgadas
delay(300);
AI.write("AT+MODEL=1\r\n", 12); // cargar modelo flash id=1 explícitamente
delay(500);
```

Sin esto, el primer invoke puede no producir evento aunque `begin()` haya devuelto OK.

### 5. Tiempos de arranque

- El Vision AI V2 tarda ~2–5 s tras power-on en aceptar comandos.
- Usar `delay(5000)` tras `Wire.begin()` antes de `AI.begin()` es más seguro que 3 s.
- I2C a **100 kHz** (`clock=100000`) es más estable con cables Grove largos; 400 kHz también funciona.

### 6. Permisos serie en Linux

El usuario `Ignacio` está en el grupo `uucp`, necesario para acceder a `/dev/ttyACM0` (`crw-rw---- root uucp`).

### 7. Dispositivos I2C típicos del bus (para escáner)

| Dirección | Dispositivo |
|---|---|
| `0x28` | Acelerómetro/interno Wio Terminal |
| `0x62` | Grove Vision AI V2 |
| `0x79` | Otro periférico onboard (LCD u otro) |

### 8. `WiFi.h: No such file or directory` al compilar el sketch del ESP32

Síntoma: el IDE (o `arduino-cli`) falla en `esp32_wifi_bridge.ino:12` con
`fatal error: WiFi.h: No such file or directory`. **No falta ninguna librería**:
la placa seleccionada es el Wio Terminal (`Seeeduino:samd:seeed_wio_terminal`),
que no tiene Wi-Fi. Hay que compilar ese sketch con la placa del ESP32:

- IDE: *Tools → Board → esp32 → XIAO_ESP32C3* (y *Tools → Port → `/dev/ttyACM0`*).
- CLI: `arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32C3 esp32_wifi_bridge`

Comando que reproduce el error (para confirmar el diagnóstico):
`arduino-cli compile --fqbn Seeeduino:samd:seeed_wio_terminal esp32_wifi_bridge`.

### 9. La flecha solo puede apuntar por aristas

Si el `NODE:X` que llega no es vecino directo del nodo actual, el Wio lo
descarta y vuelve a pedir; `dirBetween()` devuelve `DIR_NONE` (cruz gris) para
pares sin arista. Si la pantalla queda en "STOPPED" con un TO válido, revisar
que `kEdges` de `GraphData.h` y la matriz de `esp32_wifi_bridge/graph.cpp`
tengan exactamente las mismas aristas y pesos.

## Sketch de referencia funcional (verificado 2026-10-14)

```cpp
#include <Seeed_Arduino_SSCMA.h>

SSCMA AI;

void setup()
{
    Serial.begin(115200);
    while (!Serial) delay(10);

    Wire.begin();
    delay(5000);

    // begin(wire, rst, address, wait_delay, clock)
    bool ok = AI.begin(&Wire, -1, 0x62, 5, 100000);
    Serial.print("AI.begin() = ");
    Serial.println(ok ? "OK" : "FALLO");

    Serial.print("ID: ");
    Serial.println(AI.ID(true));
    Serial.print("Name: ");
    Serial.println(AI.name(true));

    AI.write("AT+BREAK\r\n", 10);
    delay(300);
    AI.write("AT+MODEL=1\r\n", 12);
    delay(500);
}

void loop()
{
    // filter=true → DIFFERED=0 → siempre emite evento
    // show=false → no necesita buffer de imagen
    int ret = AI.invoke(1, true, false);
    if (ret == 0)
    {
        Serial.print("infer=");
        Serial.print(AI.perf().inference);
        Serial.println("ms");
        for (int i = 0; i < AI.boxes().size(); i++)
        {
            Serial.print("Box target=");
            Serial.print(AI.boxes()[i].target);
            Serial.print(" score=");
            Serial.print(AI.boxes()[i].score);
            Serial.print(" x=");
            Serial.print(AI.boxes()[i].x);
            Serial.print(" y=");
            Serial.print(AI.boxes()[i].y);
            Serial.print(" w=");
            Serial.print(AI.boxes()[i].w);
            Serial.print(" h=");
            Serial.println(AI.boxes()[i].h);
        }
        if (AI.boxes().size() == 0)
            Serial.println("(sin detecciones)");
    }
    else
    {
        Serial.print("invoke failed ret=");
        Serial.println(ret);
    }
    delay(500);
}
```

Referencias útiles del diagnóstico (comandos AT crudos vía `AI.write`/`AI.available`/`AI.read`):

- `AT+VER?` → firmware `2025.01.02`
- `AT+STAT?` → `is_ready: 1`
- `AT+MODELS?` → `[{"id":1,"type":0,"address":4194304,"size":0}]`
- `AT+MODEL?` → modelo activo
- `AT+SENSORS?` → cámara `240x240 Auto`, `state:1`
- `AT+INFO?` → metadata base64 del modelo (Person Detection Swift YOLO)

## Errores de la librería SSCMA 1.0.3 (conocidos)

- `invoke()` con `filter=false` usa DIFFERED=1 → no emite evento si resultado igual al anterior.
- `strncmp(..., sizeof(cmd))` en `wait()` compara solo 4 bytes (sizeof puntero), suficiente para "INVO" pero frágil.
- `show=true` retorna `ENOTSUP` en plataformas con `SSCMA_MAX_RX_SIZE < 16*1024` (todo salvo ESP32).
