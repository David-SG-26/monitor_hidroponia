/*
 * TEST DE UN SENSOR XKC-Y25-V (salida por voltaje, con cable de modo)
 * -------------------------------------------------------------------
 * IMPORTANTE — el cable AZUL (modo) NO es opcional en la version -V:
 *   - Azul al POSITIVO 5V (misma columna que el rojo) -> salida normal:
 *       amarillo saca nivel ALTO (~5V) cuando hay agua.
 *   - Azul a GND (negro)  -> logica invertida.
 *   - Azul al aire        -> salida indefinida: leera siempre 0 aunque
 *                            el LED rojo del sensor si reaccione.
 *
 * CONEXION para esta prueba:
 *   rojo  -> +5V
 *   negro -> GND
 *   azul  -> +5V   (mismo rail que el rojo)  <-- clave
 *   amarillo (senal) -> divisor 2x10k -> punto medio a GPIO32
 *
 * CÓMO INTERPRETAR EL MONITOR SERIE (115200 baudios):
 *
 *   Sin palma = 0  y  con palma = 1   -> PERFECTO, logica normal.
 *   Sin palma = 1  y  con palma = 0   -> invertido: no toques el
 *       hardware, se arregla en firmware con NIVEL_AGUA_PRESENTE 0.
 *   Siempre 0, pase lo que pase       -> el azul sigue sin conectar,
 *       o el amarillo no llega al pin (revisa el punto medio del divisor).
 *   Parpadea 0/1 sin tocar            -> el divisor deja ~2,5V (limite
 *       alto del ESP32); avisa para cambiar la proporcion de resistencias.
 *
 * Pines para repetir la prueba con los demas sensores: 32, 33, 34, 36, 39.
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
  Serial.println("Recuerda: cable AZUL conectado a +5V.");
  Serial.println("Apunta que lee SIN palma y CON palma.");
}

void loop() {
  int lectura = digitalRead(PIN_SENSOR);
  digitalWrite(PIN_LED, lectura ? HIGH : LOW);
  if (lectura == 1) Serial.println("AGUA (1)");
  else              Serial.println("SIN AGUA (0)");
  delay(500);
}
