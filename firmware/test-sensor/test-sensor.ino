/*
 * TEST DE UN SENSOR DE NIVEL XKC-Y25 — lo más simple posible
 * -----------------------------------------------------------
 * Prueba UN solo sensor. Empieza por el del pin 32 (principal alto) y,
 * cuando funcione, cambia el número de PIN_SENSOR y repite con los demás:
 *
 *   GPIO32 → principal alto      GPIO36 → auxiliar medio (VP)
 *   GPIO33 → principal bajo      GPIO39 → auxiliar bajo  (VN)
 *   GPIO34 → auxiliar medio-alto
 *
 * Cómo probar: abre el Monitor Serie a 115200 baudios. Debe decir
 * "SIN AGUA (0)". Pega la palma de la mano (o un vaso de agua) a la cara
 * de detección del sensor: debe cambiar a "AGUA (1)". El LED azul de la
 * placa también se enciende con agua, por si no ves el monitor.
 *
 * Recuerda: el amarillo del sensor NO va directo al pin — pasa por su
 * divisor de 2 resistencias de 10k en la protoboard (sección 4 de la guía).
 */

#define PIN_SENSOR 32   // ← cambia este número para probar otro sensor
#define PIN_LED    2    // LED azul de la placa

void setup() {
  Serial.begin(115200);
  pinMode(PIN_SENSOR, INPUT);
  pinMode(PIN_LED, OUTPUT);
  Serial.println();
  Serial.print("=== Probando sensor en GPIO");
  Serial.print(PIN_SENSOR);
  Serial.println(" ===");
  Serial.println("Acerca la mano o agua a la cara del sensor y observa.");
}

void loop() {
  int lectura = digitalRead(PIN_SENSOR);
  digitalWrite(PIN_LED, lectura ? HIGH : LOW);

  if (lectura == 1) {
    Serial.println("AGUA (1)");
  } else {
    Serial.println("SIN AGUA (0)");
  }
  delay(500);
}
