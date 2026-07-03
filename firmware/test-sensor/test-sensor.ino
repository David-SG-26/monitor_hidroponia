/*
 * TEST DE UN SENSOR XKC-Y25 (salida por voltaje ~5V)
 * --------------------------------------------------
 * CABLEADO CONFIRMADO de estos sensores:
 *   rojo/rosa -> +5V
 *   azul      -> GND
 *   negro     -> modo: AISLAR con cinta, NO conectar
 *   amarillo  -> señal
 *
 * La señal sale a ~5V y el GPIO del ESP32 admite 3.3V. Por eso el amarillo
 * pasa por UNA resistencia de 10k EN SERIE hasta el pin:
 *
 *   amarillo --[ 10k ]-- GPIO32
 *
 * No es un divisor (no lleva segunda resistencia a GND): como el pin es de
 * alta impedancia, no baja el nivel lógico (lee la señal completa), pero
 * limita la corriente en los diodos internos del ESP32 y protege el pin.
 *
 * CÓMO INTERPRETAR EL MONITOR SERIE (115200 baudios):
 *
 *   Sin palma = 0  y  con palma = 1   -> PERFECTO, lógica normal.
 *   Sin palma = 1  y  con palma = 0   -> invertido: no toques el hardware,
 *       se arregla en firmware con NIVEL_AGUA_PRESENTE 0.
 *   Siempre 0, pase lo que pase       -> revisa: azul a GND, negro aislado,
 *       y que las 2 patas de la resistencia estén en filas distintas.
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
  Serial.println("Cableado: rojo=+5V, azul=GND, negro=aislar, amarillo->10k->pin.");
  Serial.println("Apunta que lee SIN palma y CON palma.");
}

void loop() {
  int lectura = digitalRead(PIN_SENSOR);
  digitalWrite(PIN_LED, lectura ? HIGH : LOW);
  if (lectura == 1) Serial.println("AGUA (1)");
  else              Serial.println("SIN AGUA (0)");
  delay(500);
}
