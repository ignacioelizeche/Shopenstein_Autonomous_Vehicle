# Plan del proyecto: auto autónomo guiado por marcadores visuales

Wio Terminal + Grove Vision AI V2 + driver de motores + chasis 2WD. Documento de trabajo con objetivo final, arquitectura, fases, pruebas y riesgos.

## 1. Objetivo

**Objetivo final (presentación final):** un auto que se mueve por un espacio **sin necesidad de un grafo dibujado en el piso**. El auto identifica marcadores colocados en paredes o en el suelo, estima hacia dónde están y a qué distancia, y los usa para decidir qué hacer: avanzar hacia ellos, detenerse, girar o seguir al siguiente marcador.

**Objetivo del avance (midterm):** demostrar una parte que funcione de punta a punta: el auto avanza con velocidad controlada, la cámara reconoce un marcador entrenado con tus propias fotos, y el Wio reacciona (se detiene y muestra el ID del marcador en su pantalla).

**Idea central del diseño:** los marcadores no forman un mapa, son **instrucciones y puntos de referencia**. Cada marcador tiene un ID, y una tabla en el código dice qué hacer al llegar a cada uno. La "ruta" es una secuencia de marcadores, no un dibujo en el piso.

## 2. Alcance

**Incluye**

- Reconocimiento de marcadores con la cámara a bordo (Grove Vision AI V2).
- Estimación aproximada de ángulo (bearing) y distancia a cada marcador.
- Navegación reactiva: buscar, acercarse, ejecutar la acción asociada y continuar.
- Frenado ante obstáculos con el detector de distancia.
- Pantalla del Wio como interfaz de depuración (ID visto, estado, distancia estimada).

**No incluye (a menos que sobre tiempo)**

- Mapas métricos ni SLAM.
- Localización en coordenadas reales en centímetros.
- Trayectorias complejas o velocidades altas.

**Nota sobre "dónde está el auto y dónde están las cosas":** con una sola cámara a bordo, la posición del auto es **relativa a los marcadores** ("estoy a unos 30 cm del marcador 3, y lo veo ligeramente a la izquierda"). No son coordenadas globales. Si más adelante se quisieran coordenadas globales, se necesitaría odometría con encoders o una cámara fija desde arriba, que es otro diseño.

## 3. Arquitectura del sistema

**Hardware**

| Componente | Función |
| --- | --- |
| Wio Terminal | Cerebro: máquina de estados, control de motores, pantalla |
| Grove Vision AI V2 | Detecta marcadores con un modelo cargado en el módulo y entrega clase y caja delimitadora |
| Driver TB6612FNG (o reemplazo) | Maneja los dos motores con PWM y dirección |
| 2 motores TT amarillos + chasis 2WD | Tracción diferencial, rueda loca delante |
| Detector de distancia | Frenado ante obstáculos al frente |
| 4 pilas AA | Alimentan solo los motores (pin VM del driver) |
| Power bank USB-C | Alimenta el Wio por separado, para evitar reinicios por los picos de los motores |
| Encoders (opcional) | Sensores de horquilla para los discos ranurados que ya tienes |

**Flujo de datos**

1. La cámara captura una imagen y el modelo devuelve las detecciones (clase y caja).
2. El Wio lee esas detecciones por I2C (a confirmar con la documentación de Seeed).
3. De la caja se calcula el ángulo y la distancia aproximada al marcador.
4. La máquina de estados decide la acción.
5. El control convierte la acción en velocidades de rueda izquierda y derecha, y las aplica por PWM.

**Alternativa si la cámara se queda corta:** una webcam USB conectada a un PC corre OpenCV con marcadores ArUco o AprilTag, y el PC manda comandos al Wio por serial. Es más potente y más preciso, pero el auto deja de ser autónomo a bordo. Se deja como plan B.

## 4. Hardware y conexiones

### 4.1 Alimentación

- Pilas AA solo a **VM** del driver. Nada más.
- Power bank al USB-C del Wio. El Wio entrega 3.3 V al VCC del driver y al sensor de distancia.
- **GND común** entre Wio, driver, sensores y negativo de las pilas.
- Recomendado: interruptor en el positivo de las pilas.

### 4.2 Asignación de pines del Wio

Los nombres de pin (D0 a D8, BCM4, BCM5, BCM6, BCM17) están verificados. Los números físicos del conector se deducen del trazado compatible con Raspberry Pi: confírmalos con el pinout impreso de Seeed antes de cablear.

| Señal | Pin del Wio | Pin físico |
| --- | --- | --- |
| PWMA | D0 | 13 |
| PWMB | D1 | 15 |
| AIN1 | BCM4 | 7 |
| AIN2 | BCM17 | 11 |
| BIN1 | BCM5 | 29 |
| BIN2 | BCM6 | 31 |
| STBY y VCC del driver | 3.3 V | 1 |
| GND | GND | 6 |

- **Cámara:** puerto Grove I2C del Wio.
- **Detector de distancia:** I2C (si es ToF) o un pin digital libre (si es ultrasónico).
- **Encoders (si se compran):** dos pines libres entre D2 y D8. Hay que verificar cuáles soportan interrupciones externas en el Wio.
- **Sensor de línea de 5 canales:** ya no es necesario en el diseño final. Libera los pines D2 a D6.

### 4.3 Problema pendiente: marcha atrás

El canal A del driver tiene un fallo en el lado de AIN2 (según las pruebas hechas, el motor A no gira bien al invertir el sentido). Consecuencias:

- **Para el midterm no importa:** basta con avanzar. Se fija AIN1 y BIN1 a 3.3 V, AIN2 y BIN2 a GND, y se controla la velocidad solo con PWM.
- **Para el final sí importa:** buscar, girar en el lugar y retroceder requieren que ambos motores giren en los dos sentidos. Hay que conseguir otro driver (cualquiera sirve: TB6612FNG, L298N, DRV8833, L9110S) o intentar reparar la soldadura del pin.
- Antes de comprar, probar el canal B en los dos sentidos. Quizá el fallo está solo en el canal A.

### 4.4 Montaje en el chasis

- Cámara al frente, a unos 10 a 12 cm de altura, con una inclinación ligera hacia abajo. Tiene que ver marcadores en paredes (a su altura) y en el suelo (más cerca).
- Distancia mínima de enfoque de la cámara: **verificar** en la documentación. Si no enfoca a menos de unos 15 cm, el auto debe detenerse antes de que el marcador quede demasiado cerca.
- Wio y driver fijados con tornillos o bridas. En la versión final, sin protoboard, porque los falsos contactos aparecen con la vibración.

## 5. Visión por computador

### 5.1 Diseño de los marcadores

Hay dos caminos. Se recomienda el primero.

**A. Marcadores como clases de un detector (recomendado, todo a bordo)**

- Cada marcador es un símbolo grande y de alto contraste, distinto de los demás: por ejemplo un círculo rojo, un cuadrado azul, un triángulo verde, una cruz amarilla, un círculo con cruz.
- El modelo de detección aprende una clase por marcador.
- Ventaja: corre dentro del módulo, sin PC.
- Límite práctico: entre 4 y 8 clases, para no necesitar un conjunto de datos enorme.

**B. Marcadores tipo ArUco o AprilTag (plan B, necesita PC)**

- Se decodifican con OpenCV y entregan un ID y la pose con precisión.
- No se espera que el Grove Vision AI V2 los decodifique de forma nativa. **Verificarlo** en la documentación de Seeed antes de descartarlos.

**Reglas de diseño comunes**

- Tamaño de 10 a 15 cm de lado, impreso en mate (el papel brillante refleja).
- Fondo blanco con borde negro grueso.
- Colocados a la altura de la cámara en las paredes, o planos en el suelo.
- Tamaño real conocido y **el mismo para todos**: es lo que permite estimar distancia.

### 5.2 Entrenamiento del modelo

1. Elegir 4 a 6 marcadores para empezar.
2. Tomar fotos **con la propia cámara del auto**, en el piso y con la luz del lugar donde se hará la demo.
3. Entre 100 y 200 imágenes por marcador, variando distancia (15 a 100 cm), ángulo, iluminación y fondo. Incluir también imágenes sin ningún marcador, para reducir falsos positivos.
4. Etiquetar con cajas delimitadoras.
5. Entrenar y desplegar el modelo en el módulo. El flujo habitual del Grove Vision AI V2 usa SenseCraft AI: **confirmar los pasos actuales en la wiki de Seeed**.
6. Validar con imágenes nuevas que no se usaron al entrenar.

### 5.3 Estimar ángulo y distancia

Para cada detección se conoce el centro horizontal de la caja (cx), el ancho de la caja en píxeles (w) y el ancho de la imagen.

- **Distancia aproximada:** d = f × W / w, donde W es el ancho real del marcador y f es la distancia focal en píxeles.
- **Calibración de f:** colocar un marcador a una distancia conocida D0 (por ejemplo 50 cm), medir su ancho en píxeles w0, y calcular f = w0 × D0 / W.
- **Ángulo (bearing):** diferencia entre cx y el centro de la imagen, convertida a grados con f (ángulo = atan((cx − cx0) / f)).
- **Precisión esperada:** de unos centímetros a decenas de centímetros, empeora con la distancia. Suficiente para acercarse y detenerse, no para mediciones finas.
- **Respaldo para la distancia:** el detector de distancia al frente da un valor directo cuando el marcador está justo delante.

## 6. Lógica de navegación

### 6.1 El libro de ruta

Una tabla en el código asocia cada marcador a una acción:

| Marcador | Distancia de parada | Acción al llegar | Siguiente marcador |
| --- | --- | --- | --- |
| 1 (círculo rojo) | 30 cm | Girar 90° a la derecha | 2 |
| 2 (cuadrado azul) | 30 cm | Seguir recto | 3 |
| 3 (triángulo verde) | 25 cm | Girar 90° a la izquierda | 4 |
| 4 (cruz amarilla) | 20 cm | Detenerse (fin) | — |

Los valores son ejemplos. La ruta es la secuencia de la última columna, y se puede cambiar editando la tabla, sin tocar el piso.

### 6.2 Máquina de estados

1. **BUSCAR:** no se ve el marcador objetivo. El auto gira lentamente hasta encontrarlo. Si da una vuelta completa sin verlo, se detiene y lo indica en la pantalla.
2. **ALINEAR:** se ve el marcador. El auto gira hasta centrarlo en la imagen (error de ángulo cercano a cero).
3. **ACERCARSE:** avanza hacia el marcador, corrigiendo el rumbo continuamente, con velocidad proporcional a la distancia restante.
4. **LLEGADA:** al alcanzar la distancia de parada, se detiene.
5. **EJECUTAR ACCIÓN:** gira, avanza o termina, según la tabla.
6. **SIGUIENTE:** pasa al siguiente marcador de la lista y vuelve a BUSCAR.
7. **OBSTÁCULO** (prioridad máxima): si el detector de distancia marca menos de unos 15 cm al frente, el auto frena, espera y reintenta. Si persiste, informa y se detiene.

### 6.3 Qué hacer si se pierde el marcador

- Si desaparece unos instantes (ruido de la cámara), el auto sigue avanzando 0.5 a 1 segundo con la última orden.
- Si no vuelve a aparecer, regresa a BUSCAR, girando hacia el último lado donde se vio.
- Si el marcador queda demasiado cerca para enfocarse, el auto da por cumplida la aproximación usando la última distancia estimada.

## 7. Control de movimiento

- **Tracción diferencial:** velocidad izquierda = base − corrección, velocidad derecha = base + corrección.
- **Corrección:** controlador proporcional (más adelante PD) sobre el error de ángulo hacia el marcador.
- **Velocidades bajas:** la cámara entrega pocas detecciones por segundo, y un auto rápido supera esa frecuencia y pierde el marcador. Se empieza lento y se sube poco a poco.
- **PWM mínimo:** los motores TT no se mueven con valores muy bajos. Medir en el piso real qué valor mínimo vence la fricción (suele rondar 120 a 150 sobre 255) y no bajar de ahí.
- **Giros:**
  - Sin encoders: se gira hasta que la cámara encuentre la referencia correspondiente (el marcador siguiente, o la línea del marcador). No se confía en el tiempo.
  - Con encoders: se mide cuánto giró cada rueda y se gira por ángulo, con la cámara como confirmación.
- **Encoders (recomendado):** comprar dos módulos de horquilla (tipo LM393) para los discos ranurados que ya vienen en el chasis. Permiten igualar la velocidad de las dos ruedas con un PID, hacer giros por ángulo y avanzar una distancia conocida. Contar las ranuras del disco y medir el diámetro de la rueda para calcular los centímetros por pulso.

## 8. Plan por fases

Las duraciones son orientativas. Ajústalas con la fecha real del midterm y de la presentación final.

### Fase 0 — Base de hardware

- Armar el chasis con motores, ruedas, rueda loca y portapilas.
- Dejar los dos motores funcionando **hacia adelante** con PWM (modo de avance fijo, descrito en la sección 4.3).
- Resolver o reemplazar el driver para tener marcha atrás.
- **Entregable:** el auto avanza, frena y hace curvas suaves sobre el piso.

### Fase 1 — Cámara en marcha

- Conectar el Grove Vision AI V2 al Wio por I2C.
- Cargar un modelo de ejemplo y leer sus detecciones desde el Wio.
- Mostrar en la pantalla del Wio la clase detectada y la posición de la caja.
- **Entregable:** detección en vivo mostrada en la pantalla del Wio.

### Fase 2 — Marcadores propios (cierra el midterm)

- Diseñar e imprimir 3 a 4 marcadores.
- Tomar el conjunto de datos con la cámara del auto, etiquetar, entrenar y cargar el modelo.
- Medir la tasa de aciertos y el rango de distancias útil.
- Demo integrada: el auto avanza, ve un marcador, se detiene y muestra el ID en la pantalla.
- **Entregable del midterm:** esa demo, más el libro de ruta como tabla en código.

### Fase 3 — Estimación de ángulo y distancia

- Calibrar la focal en píxeles con un marcador a distancia conocida.
- Comparar la distancia estimada con la medida real en varios puntos y registrar el error.
- Integrar el detector de distancia.
- **Entregable:** tabla de error de estimación y funciones de ángulo y distancia en el código.

### Fase 4 — Alinear y acercarse

- Implementar ALINEAR y ACERCARSE con control proporcional.
- Probar con un solo marcador a distintas distancias y ángulos iniciales.
- **Entregable:** el auto se acerca a un marcador y se detiene a la distancia pedida, de forma repetible.

### Fase 5 — Buscar y encadenar marcadores

- Implementar BUSCAR y la lógica de pérdida de marcador.
- Implementar el libro de ruta y la secuencia de 3 a 4 marcadores.
- Agregar los giros de la tabla (por referencia visual, o por ángulo si hay encoders).
- **Entregable:** el auto completa una ruta de 3 a 4 marcadores en el espacio de la demo.

### Fase 6 — Robustez y obstáculos

- Agregar frenado por obstáculo y recuperación.
- Probar con distintas luces y posiciones de partida.
- Repetir la ruta completa 10 veces y registrar cuántas salen bien.
- **Entregable:** tasa de éxito medida y lista de fallos conocidos.

### Fase 7 — Presentación final

- Elegir el escenario de la demo y ensayarlo.
- Preparar el video de respaldo por si la demo en vivo falla.
- **Entregable:** demo en vivo, video de respaldo y diapositivas.

## 9. Pruebas y métricas

| Qué se mide | Cómo | Meta orientativa |
| --- | --- | --- |
| Reconocimiento de marcadores | 20 imágenes nuevas por marcador a distintas distancias | Más de 90% de aciertos entre 20 y 80 cm |
| Error de distancia | Marcador a 20, 40, 60 y 80 cm, 5 mediciones cada uno | Menos de 15% de error a 60 cm |
| Error de ángulo | Marcador a ángulos conocidos | Menos de 5 grados |
| Frenado por obstáculo | Obstáculo a 15 cm del frente | Se detiene el 100% de las veces |
| Ruta completa | 10 intentos desde el mismo punto de partida | 7 o más completados sin intervención |
| Rendimiento de la cámara | Contar detecciones por segundo en marcha | Medirlo y fijar la velocidad máxima segura según eso |

Registrar cada resultado en una tabla. Las metas son puntos de partida: ajústalas a lo que den las pruebas reales.

## 10. Riesgos y qué hacer

| Riesgo | Efecto | Mitigación |
| --- | --- | --- |
| Pocas detecciones por segundo de la cámara | El auto pierde el marcador a velocidad media | Velocidad baja y avance corto con la última orden |
| Cambios de luz | El modelo falla en otro lugar | Entrenar con fotos del lugar real y con varias luces |
| Driver sin marcha atrás | No se puede buscar ni girar en el lugar | Conseguir otro driver o probar el canal B; plan de avance fijo para el midterm |
| Reinicios del Wio por los motores | El auto se detiene a mitad de la ruta | Fuentes separadas y GND común; condensador cerca del driver |
| Falsos contactos del protoboard | Fallos intermitentes | Pasar a cables firmes o soldar en la versión final |
| Marcador demasiado cerca para enfocar | Se pierde la referencia al final | Parar a una distancia mayor al mínimo de enfoque |
| Detecciones falsas | El auto va hacia algo que no es un marcador | Imágenes negativas en el entrenamiento y umbral de confianza alto |
| Giros imprecisos sin encoders | La ruta se desvía | Comprar encoders, o girar hasta ver la siguiente referencia |
| Poco tiempo antes del midterm | Se queda sin demo | Alcance mínimo: avance más detección de un marcador con parada |

## 11. Pendientes por verificar

- Que `analogWrite` dé PWM real en D0 y D1 (si un motor solo va a tope o parado, mover los pines).
- Los números físicos del conector del Wio, contra el pinout oficial de Seeed.
- El nombre exacto de los pines BCM en el paquete Arduino que se use (si no compila, pedir alternativas).
- Cómo se leen las detecciones del Grove Vision AI V2 desde el Wio (librería y protocolo actuales).
- Distancia mínima de enfoque, campo de visión y velocidad de detección de la cámara.
- Si el Grove Vision AI V2 soporta ArUco o AprilTag de forma nativa.
- Voltaje y consumo de los motores, para confirmar que 4 AA alcanzan (medir con multímetro bajo carga).
- Qué pines del Wio admiten interrupciones, para los encoders.

## 12. Guion de las presentaciones

**Midterm (avance)**

1. **Problema y visión:** un auto que se orienta solo con marcadores visuales, sin mapa dibujado en el piso.
2. **Arquitectura:** el diagrama de hardware y el flujo cámara a motores.
3. **Lo ya hecho:** auto armado, motores controlados, cámara reconociendo marcadores propios, libro de ruta en código.
4. **Demo en vivo:** el auto avanza, ve un marcador, se detiene y muestra el ID.
5. **Lo que falta:** estimación de distancia, aproximación, búsqueda, encadenamiento de marcadores, obstáculos.
6. **Riesgos y plan para atenderlos.**

**Final**

1. Recordar el objetivo y qué cambió desde el avance.
2. Demo en vivo de la ruta completa con 3 a 4 marcadores, sin ningún dibujo en el piso.
3. Resultados medidos: aciertos de reconocimiento, error de distancia y de ángulo, tasa de rutas completadas.
4. Limitaciones honestas: velocidad baja, dependencia de la luz, posición relativa y no global.
5. Trabajo futuro: encoders y odometría, más marcadores, ArUco con cámara de mayor capacidad, localización global.
6. Video de respaldo disponible por si la demo en vivo falla.
