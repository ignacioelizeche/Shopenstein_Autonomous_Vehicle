# Diagrama de Conexiones y Pinout del Sistema

Memoria técnica de cableado para el proyecto **Auto Autónomo Guiado por Marcadores Visuales** (Midterm - *Embedded System Design*).

---

## 1. Arquitectura General y Distribución de Potencia

El sistema utiliza una arquitectura de **alimentación separada con tierra común** para proteger los microcontroladores de los ruidos inductivos y caídas de tensión provocadas por los motores DC.

```mermaid
flowchart TD
    subgraph Fuentes ["Fuentes de Alimentacion"]
        PB["Power Bank USB-C (5V)"]
        BAT["Portapilas 4x AA (~6V DC)"]
    end

    subgraph Cerebro ["Controlador Maestro"]
        WIO["Seeed Wio Terminal (ATSAMD51)<br/>- Pantalla LCD (grafo de nodos)<br/>- FSM y Logica de Navegacion"]
    end

    subgraph Vision ["Sensor de Vision Artificial"]
        CAM["Grove Vision AI V2 (Himax HX6538)<br/>Camara OV5647 CSI"]
    end

    subgraph Comunicaciones ["Coprocesador Wi-Fi"]
        ESP["Seeed XIAO ESP32-C3<br/>- Access Point 'Robot-Autonomo'<br/>- Dashboard Web, Joystick, Ruta<br/>- Medidor de latencia Wi-Fi / UART"]
    end

    subgraph Potencia ["Etapa de Potencia y Traccion"]
        DRV["Driver TB6612FNG"]
        MOT_IZQ["Motor TT Izquierdo (Canal A)"]
        MOT_DER["Motor TT Derecho (Canal B)"]
    end

    subgraph Usuario ["Teleoperacion"]
        PHONE["Smartphone / Navegador Web<br/>http://192.168.4.1"]
    end

    %% Conexiones de Alimentacion
    PB -->|Cable USB-C| WIO
    WIO -->|"5V (Pin 2) y GND (Pin 6)"| ESP
    WIO -->|"3.3V (Pin 1) a VCC y STBY"| DRV
    BAT -->|"Cable Rojo (+6V) a VM"| DRV
    BAT -->|"Cable Negro a GND"| DRV

    %% Conexiones de Datos
    WIO <-->|"Grove I2C (0x62, 100 kHz)"| CAM
    ESP <-->|"Serial1 (UART 115200)"| WIO
    PHONE <-->|"Wi-Fi 2.4 GHz (HTTP/Web)"| ESP

    %% Conexiones de Motores
    DRV -->|"Salidas A01 / A02"| MOT_IZQ
    DRV -->|"Salidas B01 / B02"| MOT_DER
    WIO -->|"Control PWM y Direccion"| DRV
```

---

## 2. Diagrama Detallado de Conexiones Pin a Pin

```mermaid
flowchart LR
    subgraph WIO_HEADER ["Wio Terminal (Conector 40 Pines)"]
        W_P1["Pin 1: 3.3V"]
        W_P2["Pin 2: 5V"]
        W_P6["Pin 6: GND"]
        W_P7["Pin 7: BCM4"]
        W_P8["Pin 8: BCM14 (TX1)"]
        W_P10["Pin 10: BCM15 (RX1)"]
        W_P11["Pin 11: BCM17"]
        W_P13["Pin 13: D0 (PWMA)"]
        W_P15["Pin 15: D1 (PWMB)"]
        W_P29["Pin 29: BCM5"]
        W_P31["Pin 31: BCM6"]
        W_GROVE["Puerto Grove Derecho (I2C)"]
    end

    subgraph DRIVER ["Driver TB6612FNG"]
        D_VM["VM (Motor Power)"]
        D_VCC["VCC (Logic 3.3V)"]
        D_STBY["STBY (Standby)"]
        D_GND["GND (Tierra)"]
        D_PWMA["PWMA"]
        D_AIN1["AIN1"]
        D_AIN2["AIN2"]
        D_PWMB["PWMB"]
        D_BIN1["BIN1"]
        D_BIN2["BIN2"]
        D_A01["A01"]
        D_A02["A02"]
        D_B01["B01"]
        D_B02["B02"]
    end

    subgraph ESP32 ["Seeed XIAO ESP32-C3"]
        E_VUSB["VUSB (5V)"]
        E_GND["GND"]
        E_D6["D6 (TX)"]
        E_D7["D7 (RX)"]
    end

    subgraph CAM_MOD ["Grove Vision AI V2"]
        C_GROVE["Puerto Grove I2C"]
    end

    subgraph BATT ["Portapilas (4x AA)"]
        B_POS["Cable Rojo (+6V)"]
        B_NEG["Cable Negro (GND)"]
    end

    subgraph MOTORS ["Motores 2WD"]
        M_A["Motor A (Izquierdo)"]
        M_B["Motor B (Derecho)"]
    end

    %% Cableado Wio a Driver
    W_P1 --> D_VCC
    W_P1 --> D_STBY
    W_P6 --> D_GND
    W_P13 --> D_PWMA
    W_P7 --> D_AIN1
    W_P11 --> D_AIN2
    W_P15 --> D_PWMB
    W_P29 --> D_BIN1
    W_P31 --> D_BIN2

    %% Pilas a Driver
    B_POS --> D_VM
    B_NEG --> D_GND

    %% Driver a Motores
    D_A01 --> M_A
    D_A02 --> M_A
    D_B01 --> M_B
    D_B02 --> M_B

    %% Wio a ESP32
    W_P2 --> E_VUSB
    W_P6 --> E_GND
    W_P8 --> E_D7
    W_P10 --> E_D6

    %% Wio a Camara
    W_GROVE <==>|"Cable Grove 4 hilos"| C_GROVE
```

---

## 3. Tablas de Pinout y Asignación de Señales

### Tabla A: Wio Terminal $\leftrightarrow$ Driver TB6612FNG

| Pin Físico Wio | Nombre en Código | Pin en Driver TB6612FNG | Función |
| :---: | :---: | :---: | :--- |
| **Pin 1** | `3.3V` | **VCC** | Alimentación lógica del chip |
| **Pin 1** | `3.3V` (puenteado a VCC) | **STBY** | Desactiva modo Standby (habilita puente H) |
| **Pin 6 / 14**| `GND` | **GND** | Tierra lógica común |
| **Pin 13** | `D0` | **PWMA** | Velocidad PWM Motor Izquierdo |
| **Pin 7** | `BCM4` | **AIN1** | Sentido Motor Izquierdo (HIGH = adelante) |
| **Pin 11** | `BCM17`| **AIN2** | Sentido Motor Izquierdo (LOW = adelante) |
| **Pin 15** | `D1` | **PWMB** | Velocidad PWM Motor Derecho |
| **Pin 29** | `BCM5` | **BIN1** | Sentido Motor Derecho (HIGH = adelante) |
| **Pin 31** | `BCM6` | **BIN2** | Sentido Motor Derecho (LOW = adelante) |

---

### Tabla B: Wio Terminal $\leftrightarrow$ Seeed XIAO ESP32-C3

| Pin Físico Wio | Nombre Señal Wio | Pin en XIAO ESP32-C3 | Función |
| :---: | :---: | :---: | :--- |
| **Pin 2 o Pin 4** | `5V` (VBUS USB) | **VUSB** | Alimentación del ESP32 desde el Wio |
| **Pin 6, 9 o 14** | `GND` | **GND** | Tierra común de referencia |
| **Pin 10** | `BCM15` (RX de `Serial1`) | **D6 (TX)** | Recibe órdenes del celular (`JOY:x,y`, `CMD:AUTO`), el salto calculado (`NODE:X`, `PATH:...`) y la posición (`SET:X`) |
| **Pin 8** | `BCM14` (TX de `Serial1`) | **D7 (RX)** | Envía telemetría de visión (`TLM:id,score,dist...`), posición y dirección (`POS:actual,siguiente,dir`) y pide el siguiente salto (`REQ:NEXT`) |

---

### Tabla C: Alimentación de Potencia de Motores

| Origen | Destino en Driver TB6612FNG | Notas de Seguridad |
| :--- | :--- | :--- |
| **Cable Rojo (+6V de 4x AA)** | **VM** | **NUNCA** conectar a los pines de 3.3V o 5V del microcontrolador. |
| **Cable Negro (GND de 4x AA)** | **GND** | Debe estar en el mismo bus de GND que el Wio Terminal. |
| **Salidas A01 y A02** | Motor TT Izquierdo | Si gira al revés, invertir los dos cables del motor. |
| **Salidas B01 y B02** | Motor TT Derecho | Si gira al revés, invertir los dos cables del motor. |

---

### Tabla D: Cámara Grove Vision AI V2

| Conector en Wio | Conector en Vision AI V2 | Señales | Protocolo |
| :--- | :--- | :--- | :--- |
| **Puerto Grove Derecho (I2C)** | Puerto Grove I2C | `SDA`, `SCL`, `3.3V`, `GND` | I2C en dirección `0x62` a 100 kHz |
