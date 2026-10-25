# Diagrama de Bloques Funcional del Sistema (Inputs & Outputs)

**Proyecto:** Auto Autónomo Guiado por Marcadores Visuales  
**Materia:** Embedded System Design (UPTP - 6to Semestre)  
**Hito:** Midterm  

---

## 1. Diagrama de Bloques Funcional

```mermaid
flowchart LR
    subgraph UI ["1. Interfaz de Usuario"]
        PHONE["Smartphone (Navegador Web)
        ------------------------
        IN: Estado, Radar, Telemetria
        OUT: Toques de Joystick y Botones"]
    end

    subgraph COMM ["2. Modulo de Comunicaciones"]
        ESP["Seeed XIAO ESP32-C3
        ------------------------
        IN: Wi-Fi HTTP Requests
            UART Telemetria (TLM)
        OUT: Servidor Web (Dashboard)
             UART Comandos (JOY/CMD)"]
    end

    subgraph BRAIN ["3. Controlador Maestro"]
        WIO["Seeed Wio Terminal (ATSAMD51)
        ------------------------
        IN: UART Comandos (desde ESP32)
            I2C Detecciones (desde Camara)
        OUT: Pantalla LCD (HUD local)
             UART Telemetria (hacia ESP32)
             Senales Logicas (PWM y Direccion)"]
    end

    subgraph SENSOR ["4. Vision Artificial"]
        CAM["Grove Vision AI V2
        ------------------------
        IN: Imagen Optica (Lente OV5647)
            Comandos I2C (AT+INVOKE)
        OUT: Metadatos I2C (Cajas, ID, Score)"]
    end

    subgraph POWER_STAGE ["5. Driver de Potencia"]
        DRV["Driver TB6612FNG
        ------------------------
        IN: PWM y Direccion (3.3V logica)
            Potencia VM (6V de pilas)
        OUT: Voltaje conmutado a Motores"]
    end

    subgraph ACTUATORS ["6. Traccion Mecanica"]
        MOT["Motores TT 2WD + Ruedas
        ------------------------
        IN: Voltaje bipolar del Driver
        OUT: Par motor y Desplazamiento"]
    end

    %% Relaciones de flujo e interconexiones
    PHONE <-->|"Wi-Fi (HTTP / JSON)"| ESP
    ESP <-->|"Serial1 UART (115200 bps)"| WIO
    WIO <-->|"Bus I2C (100 kHz)"| CAM
    WIO -->|"PWM (D0, D1) + DIR (BCM)"| DRV
    DRV -->|"Salidas H-Bridge (A01/A02, B01/B02)"| MOT
```

---

## 2. Matriz de Entradas y Salidas (I/O) por Módulo

| Módulo | Entradas (Inputs) | Salidas (Outputs) | Función en la Arquitectura |
| :--- | :--- | :--- | :--- |
| **Smartphone** | - Página web interactiva<br>- Radar de visión<br>- Telemetría en tiempo real | - Eventos táctiles del usuario (joystick virtual, botones de modo) | Interfaz hombre-máquina (HMI) inalámbrica. |
| **Seeed XIAO ESP32-C3** | - Peticiones HTTP del navegador<br>- Tramas de telemetría UART (`TLM:...`) | - Página web (HTML5/CSS/JS)<br>- Respuestas JSON (`/telemetry`)<br>- Comandos UART (`JOY:x,y`, `CMD:AUTO`) | Coprocesador de comunicaciones inalámbricas (Wi-Fi AP a 2.4 GHz). |
| **Grove Vision AI V2** | - Luz ambiental / fotones (lente OV5647 CSI)<br>- Comandos AT de inferencia por I2C (`AT+INVOKE`) | - Metadatos de inferencia por I2C (cajas delimitadoras `x, y, w, h`, `ID` de clase, confianza `score`) | Periférico inteligente de visión artificial embebida (acelerador NPU/DSP). |
| **Seeed Wio Terminal** | - Comandos seriales desde ESP32 (`Serial1`)<br>- Paquetes de detección SSCMA por I2C (`Wire`) | - Renderizado gráfico en pantalla TFT LCD integrada<br>- Telemetría periódica hacia ESP32<br>- Señales de control de motor: PWM (`D0, D1`) y dirección (`BCM4, BCM17, BCM5, BCM6`) | **Controlador maestro (Cerebro):** coordina la FSM, mezcla de tracción con trim y toma de decisiones. |
| **Driver TB6612FNG** | - Señales de control lógico (3.3V) desde el Wio<br>- Potencia pura de 6V DC desde las 4 pilas AA (`VM`) | - Corriente y voltaje conmutados por modulación PWM y puente H | Etapa de amplificación y control de potencia motriz. |
| **Motores TT 2WD** | - Voltaje aplicado a las terminales | - Velocidad angular (RPM) y par de tracción en las ruedas | Actuadores físicos del robot móvil. |

---

## 3. Flujos de Control y Cierre de Lazos

### A. Lazo Abierto: Teleoperación Manual (Joystick)
1. El usuario desplaza el joystick táctil en la pantalla de su smartphone.
2. El navegador envía la posición normalizada mediante peticiones HTTP asíncronas (`/drive?x=...&y=...`) al ESP32-C3.
3. El ESP32-C3 traduce las peticiones y las transmite de inmediato por UART (`JOY:x,y\n`) al Wio Terminal.
4. El algoritmo de control del Wio Terminal calcula el mezclado diferencial de velocidad, aplica los factores de compensación (**Trim**) y modula los pines PWM y de dirección hacia el driver TB6612FNG.
5. Los motores responden instantáneamente con movimiento proporcional.

### B. Lazo Cerrado: Misión Autónoma (Guiado por Marcador)
1. El usuario pulsa `🚀 INICIAR AUTÓNOMO` desde el celular.
2. El Wio Terminal entra en el estado `ESTADO_AUTONOMO_AVANZANDO` y pone en marcha los motores hacia adelante a velocidad base controlada.
3. El Wio interroga periódicamente al módulo Grove Vision AI V2 mediante I2C (`AI.invoke`).
4. La cámara detecta la presencia del marcador en la escena y devuelve las coordenadas y tamaño de la caja delimitadora (`w` y `h`).
5. El algoritmo calcula la distancia estimada en tiempo real:
   $$d \approx \frac{f \cdot W}{w}$$
6. **Condición de Parada:** Cuando el ancho de la caja supera el umbral de proximidad ($w \ge 75 \text{ px}$ o $d \le 25 \text{ cm}$):
   - El Wio corta la señal PWM inmediatamente a 0 (frenado en seco).
   - Transiciona al estado `LLEGADA A MARCADOR!`.
   - Muestra el ID y confirmación en la pantalla LCD integrada.
   - Envía el estado `LLEGADA` por serial al ESP32 para que el celular lo muestre en rojo de misión cumplida.
