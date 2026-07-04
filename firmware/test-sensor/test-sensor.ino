/*
 * TEST DE UN SENSOR XKC-Y25 (salida por voltaje ~5V)
 * --------------------------------------------------
 * CABLEADO CONFIRMADO de estos sensores:
 *   rojo/rosa -> +5V
 *   azul      -> GND
 *   negro     -> modo: AISLAR con cinta, NO conectar
 *   amarillo  -> señal (~4,94V, pasa por el divisor de abajo)
 *
 * La señal activa mide ~4,94V y el GPIO del ESP32 admite 3.3V. Por eso el
 * amarillo pasa por un DIVISOR de 3 resistencias de 10k:
 *
 *   amarillo --[ R1 10k ]--+-- GPIO32   (nodo de salida ~3,29V)
 *                          |
 *                       [ R2 10k ]
 *                          |
 *                       [ R3 10k ]
 *                          |
 *                         GND
 *
 * R1 (arriba) = 10k, R2+R3 (abajo) = 20k -> Vout = 4,94 x 20/30 = 3,29V.
 *
 * CÓMO INTERPRETAR EL MONITOR SERIE (115200 baudios):
 *
 *   Sin palma = 0  y  con palma = 1   -> PERFECTO, lógica normal.
 *   Sin palma = 1  y  con palma = 0   -> invertido: no toques el hardware,
 *       se arregla en firmware con NIVEL_AGUA_PRESENTE 0.
 *   Siempre 0, pase lo que pase       -> revisa: azul a GND, negro aislado,
 *       y que el nodo entre R1 y R2 (fila 2) sea el que va al GPIO.
 *
 * Pines para repetir la prueba con los demás sensores: 32, 33, 34, 36, 39.
 */

#define PIN_SENSOR 32   // ← cambia este número para probar otro sensor
#define PIN_LED    2    // LED azul de la placa

void setup() {
  Serial.begin(115200);
  pinMode(PIN_SENSOR, INPUT);
  pinMode(PIN_LED, OUTPUT);
  Serial.println();
  Serial.print("=== Prueba del sensor en GPIO");
  Serial.print(PIN_SENSOR);
  Serial.println(" ===");
  Serial.println("Cableado: rojo=+5V, azul=GND, negro=aislar, amarillo->divisor 3x10k->pin.");
  Serial.println("Apunta que lee SIN palma y CON palma.");
}

void loop() {
  int lectura = digitalRead(PIN_SENSOR);
  digitalWrite(PIN_LED, lectura ? HIGH : LOW);
  if (lectura == 1) Serial.println("AGUA (1)");
  else              Serial.println("SIN AGUA (0)");
  delay(500);
}
