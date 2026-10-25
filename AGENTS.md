# Embedded System Design — Memoria de proyecto

## Hardware del proyecto

| Componente | Detalle |
|---|---|
| MCU | **Seeed Wio Terminal** (ATSAMD51, USB CDC en `/dev/ttyACM0`) |
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
```

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
