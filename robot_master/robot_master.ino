/*
 * ROBOT MASTER - Wio Terminal (ATSAMD51)
 * Proyecto: Auto Autónomo guiado por Marcadores Visuales / Botella
 *
 * Mejoras v2:
 *   1. Lógica de giro optimizada: soporte para giros pivote en el lugar (|x| alto, y bajo)
 *      y curvas diferenciales fluidas en marcha.
 *   2. Detección de visión mejorada: umbral de confianza ajustado a 35% y filtro
 *      de persistencia (evita pérdida de detección por parpadeo de 1 fotograma).
 *   3. Guiado autónomo centrado: el auto ajusta ligeramente el rumbo si el objetivo se desvía.
 */

#include <Arduino.h>
#include <Wire.h>
#include <Seeed_Arduino_SSCMA.h>
#include <TFT_eSPI.h>

// --- PINES DE MOTORES (TB6612FNG) ---
const int PIN_PWMA = D0;    // Pin 13 (Motor A - Izquierdo)
const int PIN_AIN1 = BCM4;  // Pin 7
const int PIN_AIN2 = BCM17; // Pin 11

const int PIN_PWMB = D1;    // Pin 15 (Motor B - Derecho)
const int PIN_BIN1 = BCM5;  // Pin 29
const int PIN_BIN2 = BCM6;  // Pin 31

// --- CALIBRACIÓN DE RECTA (TRIM) ---
const float TRIM_MOTOR_A = 1.00; // Motor Izquierdo
const float TRIM_MOTOR_B = 0.74; // Motor Derecho compensado

// --- OBJETOS GLOBALES ---
SSCMA AI;
TFT_eSPI tft;

// --- ESTADOS DEL ROBOT ---
enum EstadoRobot {
    ESTADO_STOP,
    ESTADO_MANUAL_JOYSTICK,
    ESTADO_AUTONOMO_AVANZANDO,
    ESTADO_AUTONOMO_LLEGADA
};

EstadoRobot estadoActual = ESTADO_STOP;
String estadoTexto = "MANUAL - STOP";

// Parámetros de visión
int lastTargetID = -1;
int lastScore = 0;
int lastBoxX = 0, lastBoxY = 0, lastBoxW = 0, lastBoxH = 0;
int lastInferMs = 0;
int estDistCm = 0;
unsigned long lastBoxSeenTime = 0;

// --- CALIBRACIÓN DE DISTANCIA Y PARADA ---
const int CALIB_K = 2800;            // Calibración botella a 40 cm = 70 px
const int UMBRAL_SCORE = 35;         // Confianza ajustada para mayor sensibilidad a distancia
const int DISTANCIA_STOP_CM = 24;    // Frenar a ~24 cm
const int UMBRAL_ANCHO_STOP = 115;   // Ancho de caja de parada
const int VELOCIDAD_BASE = 140;      // PWM base de avance

unsigned long lastTlmSend = 0;
unsigned long lastDisplayUpdate = 0;

// --- CONTROL DE MOTORES ---
void pararMotores() {
    analogWrite(PIN_PWMA, 0);
    analogWrite(PIN_PWMB, 0);
    digitalWrite(PIN_AIN1, LOW);
    digitalWrite(PIN_AIN2, LOW);
    digitalWrite(PIN_BIN1, LOW);
    digitalWrite(PIN_BIN2, LOW);
}

void moverRuedas(int pwmL, int pwmR) {
    pwmL = constrain((int)(pwmL * TRIM_MOTOR_A), 0, 255);
    pwmR = constrain((int)(pwmR * TRIM_MOTOR_B), 0, 255);

    digitalWrite(PIN_AIN1, HIGH);
    digitalWrite(PIN_AIN2, LOW);
    analogWrite(PIN_PWMA, pwmL);

    digitalWrite(PIN_BIN1, HIGH);
    digitalWrite(PIN_BIN2, LOW);
    analogWrite(PIN_PWMB, pwmR);
}

void moverAdelante(int pwm) {
    moverRuedas(pwm, pwm);
}

// Control proporcional mejorado para Joystick y giros
void controlarJoystick(int x, int y) {
    // x: -100 (izq) a +100 (der)
    // y: -100 (atrás) a +100 (adelante)
    if (x == 0 && y == 0) {
        pararMotores();
        estadoActual = ESTADO_STOP;
        estadoTexto = "MANUAL - STOP";
        return;
    }

    estadoActual = ESTADO_MANUAL_JOYSTICK;
    estadoTexto = "MANUAL: JOYSTICK";

    // CASO 1: Giro pivote en el lugar (Joystick hacia los lados sin mucho avance)
    if (abs(y) <= 20 && abs(x) > 20) {
        int turnPwm = map(abs(x), 20, 100, 130, 210);
        if (x > 0) {
            // Giro a la derecha (Motor A avanza, Motor B quieto)
            digitalWrite(PIN_AIN1, HIGH); digitalWrite(PIN_AIN2, LOW);
            analogWrite(PIN_PWMA, (int)(turnPwm * TRIM_MOTOR_A));
            analogWrite(PIN_PWMB, 0);
        } else {
            // Giro a la izquierda (Motor B avanza, Motor A quieto)
            digitalWrite(PIN_BIN1, HIGH); digitalWrite(PIN_BIN2, LOW);
            analogWrite(PIN_PWMA, 0);
            analogWrite(PIN_PWMB, (int)(turnPwm * TRIM_MOTOR_B));
        }
        return;
    }

    // CASO 2: Avance hacia adelante con curvas diferenciales
    if (y > 0) {
        int throttle = map(y, 0, 100, 115, 235);
        int pwmL = throttle;
        int pwmR = throttle;

        if (x > 0) {
            // Curva a la derecha: reduce rueda derecha, aumenta rueda izquierda
            int delta = map(x, 0, 100, 0, throttle - 50);
            pwmR = throttle - delta;
            pwmL = min(255, throttle + (x / 3));
        } else if (x < 0) {
            // Curva a la izquierda: reduce rueda izquierda, aumenta rueda derecha
            int delta = map(-x, 0, 100, 0, throttle - 50);
            pwmL = throttle - delta;
            pwmR = min(255, throttle + (-x / 3));
        }
        moverRuedas(pwmL, pwmR);
        return;
    }

    // CASO 3: Marcha atrás
    if (y < 0) {
        int throttle = map(-y, 0, 100, 110, 200);
        int pwmL = constrain((int)(throttle * TRIM_MOTOR_A), 0, 255);
        int pwmR = constrain((int)(throttle * TRIM_MOTOR_B), 0, 255);

        digitalWrite(PIN_AIN1, LOW);
        digitalWrite(PIN_AIN2, HIGH);
        analogWrite(PIN_PWMA, pwmL);

        digitalWrite(PIN_BIN1, LOW);
        digitalWrite(PIN_BIN2, HIGH);
        analogWrite(PIN_PWMB, pwmR);
    }
}

// --- ACTUALIZACIÓN DE PANTALLA LCD ---
void actualizarPantalla() {
    tft.fillRect(0, 45, 320, 195, TFT_BLACK);

    tft.setTextSize(2);
    if (estadoActual == ESTADO_AUTONOMO_AVANZANDO) {
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.drawString("MODO: AUTONOMO", 10, 50);
    } else if (estadoActual == ESTADO_AUTONOMO_LLEGADA) {
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawString("META ALCANZADA!", 10, 50);
    } else {
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.drawString(estadoTexto, 10, 50);
    }

    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    
    char buff[40];
    snprintf(buff, sizeof(buff), "ID Visto:  %s", (lastScore > 0 ? String(lastTargetID).c_str() : "Ninguno"));
    tft.drawString(buff, 10, 85);

    snprintf(buff, sizeof(buff), "Score:     %d %%", lastScore);
    tft.drawString(buff, 10, 115);

    snprintf(buff, sizeof(buff), "Caja:      %dx%d px", lastBoxW, lastBoxH);
    tft.drawString(buff, 10, 145);

    if (estDistCm > 0) {
        snprintf(buff, sizeof(buff), "Dist Est:  ~%d cm", estDistCm);
    } else {
        snprintf(buff, sizeof(buff), "Dist Est:  -- cm");
    }
    tft.drawString(buff, 10, 175);

    snprintf(buff, sizeof(buff), "Inferencia: %d ms", lastInferMs);
    tft.drawString(buff, 10, 205);
}

void setup() {
    Serial.begin(115200);
    Serial1.begin(115200);

    pinMode(PIN_PWMA, OUTPUT);
    pinMode(PIN_AIN1, OUTPUT);
    pinMode(PIN_AIN2, OUTPUT);
    pinMode(PIN_PWMB, OUTPUT);
    pinMode(PIN_BIN1, OUTPUT);
    pinMode(PIN_BIN2, OUTPUT);
    pararMotores();

    tft.begin();
    tft.setRotation(3);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.setTextSize(2);
    tft.drawString("=== AUTO AUTONOMO ===", 30, 15);
    tft.drawFastHLine(0, 40, 320, TFT_DARKGREY);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString("Iniciando Camara I2C...", 10, 80);

    Wire.begin();
    delay(5000);

    bool camOk = AI.begin(&Wire, -1, 0x62, 5, 100000);
    if (camOk) {
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.drawString("Vision AI: CONECTADA", 10, 110);
    } else {
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawString("Vision AI: FALLO!", 10, 110);
    }

    AI.write("AT+BREAK\r\n", 10);
    delay(300);
    AI.write("AT+MODEL=1\r\n", 12);
    delay(500);

    delay(1500);
    actualizarPantalla();
}

void procesarComandosSerial() {
    while (Serial1.available()) {
        String line = Serial1.readStringUntil('\n');
        line.trim();

        if (line.startsWith("JOY:")) {
            line = line.substring(4);
            int comma = line.indexOf(',');
            if (comma != -1) {
                int jx = line.substring(0, comma).toInt();
                int jy = line.substring(comma + 1).toInt();
                controlarJoystick(jx, jy);
            }
        } else if (line.startsWith("CMD:")) {
            String act = line.substring(4);
            if (act == "STOP") {
                estadoActual = ESTADO_STOP;
                estadoTexto = "DETENIDO";
                pararMotores();
            } else if (act == "AUTO") {
                estadoActual = ESTADO_AUTONOMO_AVANZANDO;
                estadoTexto = "AUTONOMO";
                moverAdelante(VELOCIDAD_BASE);
            }
            actualizarPantalla();
        }
    }
}

void loop() {
    procesarComandosSerial();

    int ret = AI.invoke(1, true, false);
    if (ret == 0) {
        lastInferMs = AI.perf().inference;
        if (AI.boxes().size() > 0) {
            lastTargetID = AI.boxes()[0].target;
            lastScore = AI.boxes()[0].score;
            lastBoxX = AI.boxes()[0].x;
            lastBoxY = AI.boxes()[0].y;
            lastBoxW = AI.boxes()[0].w;
            lastBoxH = AI.boxes()[0].h;
            lastBoxSeenTime = millis();

            if (lastBoxW > 5) {
                estDistCm = constrain(CALIB_K / lastBoxW, 10, 200);
            }
        } else {
            // Filtro de persistencia: si no se detecta por más de 350ms, limpiar datos
            if (millis() - lastBoxSeenTime > 350) {
                lastScore = 0;
                lastBoxW = 0;
                lastBoxH = 0;
                estDistCm = 0;
            }
        }
    }

    // Lógica autónoma con centrado suave
    if (estadoActual == ESTADO_AUTONOMO_AVANZANDO) {
        if (lastScore >= UMBRAL_SCORE && estDistCm > 0) {
            // Condición de parada (meta alcanzada)
            if (estDistCm <= DISTANCIA_STOP_CM || lastBoxW >= UMBRAL_ANCHO_STOP) {
                pararMotores();
                estadoActual = ESTADO_AUTONOMO_LLEGADA;
                estadoTexto = "LLEGADA A META!";
                actualizarPantalla();
            } else {
                // Centrado en marcha: mantener el objetivo en el centro óptico (x ~ 120)
                int centroCaja = lastBoxX + (lastBoxW / 2);
                if (centroCaja < 90) {
                    // Objetivo a la izquierda: curva suave a la izquierda
                    moverRuedas(VELOCIDAD_BASE - 30, VELOCIDAD_BASE + 25);
                } else if (centroCaja > 150) {
                    // Objetivo a la derecha: curva suave a la derecha
                    moverRuedas(VELOCIDAD_BASE + 25, VELOCIDAD_BASE - 30);
                } else {
                    // Objetivo centrado: recto
                    moverAdelante(VELOCIDAD_BASE);
                }
            }
        } else {
            // Si momentáneamente no ve el objetivo, avanza recto suave
            moverAdelante(VELOCIDAD_BASE - 15);
        }
    }

    // Telemetría hacia ESP32
    if (millis() - lastTlmSend > 200) {
        lastTlmSend = millis();
        Serial1.print("TLM:");
        Serial1.print(lastTargetID); Serial1.print(",");
        Serial1.print(lastScore); Serial1.print(",");
        Serial1.print(lastBoxX); Serial1.print(",");
        Serial1.print(lastBoxY); Serial1.print(",");
        Serial1.print(lastBoxW); Serial1.print(",");
        Serial1.print(lastBoxH); Serial1.print(",");
        Serial1.print(lastInferMs); Serial1.print(",");
        Serial1.print(estDistCm); Serial1.print(",");
        Serial1.println(estadoTexto);
    }

    if (millis() - lastDisplayUpdate > 400) {
        lastDisplayUpdate = millis();
        actualizarPantalla();
    }
}
