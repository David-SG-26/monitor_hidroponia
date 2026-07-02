/**
 * Firmware hidroponía — ESP32-WROOM-32
 * ------------------------------------
 * - Lee 5 sensores de nivel XKC-Y25 (2 depósito principal, 3 auxiliar)
 * - Controla la bomba por relé con protecciones:
 *     · nunca bombear si el auxiliar-bajo indica vacío (marcha en seco)
 *     · timeout máximo de bomba
 *     · bloqueo si los sensores dan lecturas incoherentes
 * - Lee temperatura (DS18B20, pull-up interno) y TDS/EC (sonda energizada
 *   solo durante la lectura, ADC1)
 * - Envía JSON al backend Apps Script según el contrato de datos fijo
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <time.h>
#include "driver/gpio.h"
#include "config.h"

OneWire oneWire(PIN_ONEWIRE);
DallasTemperature ds18b20(&oneWire);

// ---------------------------------------------------------------- estado

struct Niveles {
  bool ppalAlto, ppalBajo;
  bool auxAlto, auxMedio, auxBajo;
};

enum class Alerta { NINGUNA, MARCHA_SECO, TIMEOUT_BOMBA, SENSOR_FALLO };

static bool bombaOn = false;
static unsigned long bombaStartMs = 0;
static unsigned long bombaStopMs = 0;
static Alerta alertaActiva = Alerta::NINGUNA;
static bool timeoutLatch = false;   // se limpia al llegar el agua a nivel alto o al reiniciar

static float ultimaTempC = NAN;
static float ultimoTds = NAN;
static float ultimoEc = NAN;
static unsigned long ultimoEnvioMs = 0;
static bool envioInmediato = false; // forzar envío al cambiar bomba/alerta

// ---------------------------------------------------------------- niveles

// Lectura con voto por mayoría para filtrar rebotes/ruido
static bool leerNivel(uint8_t pin) {
  int votos = 0;
  for (int i = 0; i < LEVEL_SAMPLES; i++) {
    if (digitalRead(pin) == NIVEL_AGUA_PRESENTE) votos++;
    delay(LEVEL_SAMPLE_GAP_MS);
  }
  return votos > LEVEL_SAMPLES / 2;
}

static Niveles leerNiveles() {
  Niveles n;
  n.ppalAlto = leerNivel(PIN_PPAL_ALTO);
  n.ppalBajo = leerNivel(PIN_PPAL_BAJO);
  n.auxAlto  = leerNivel(PIN_AUX_ALTO);
  n.auxMedio = leerNivel(PIN_AUX_MEDIO);
  n.auxBajo  = leerNivel(PIN_AUX_BAJO);
  return n;
}

// Coherencia física: no puede haber agua arriba sin haberla abajo
static bool nivelesIncoherentes(const Niveles& n) {
  if (n.ppalAlto && !n.ppalBajo) return true;
  if (n.auxAlto && !n.auxMedio) return true;
  if (n.auxMedio && !n.auxBajo) return true;
  return false;
}

// ----------------------------------------------------------------- bomba

static void setBomba(bool on) {
  if (on == bombaOn) return;
  bombaOn = on;
#if RELE_ACTIVO_BAJO
  digitalWrite(PIN_RELE, on ? LOW : HIGH);
#else
  digitalWrite(PIN_RELE, on ? HIGH : LOW);
#endif
  if (on) bombaStartMs = millis();
  else    bombaStopMs = millis();
  envioInmediato = true;
  Serial.printf("[BOMBA] %s\n", on ? "ON" : "OFF");
}

static void setAlerta(Alerta a) {
  if (a == alertaActiva) return;
  alertaActiva = a;
  envioInmediato = true;
}

static void controlBomba(const Niveles& n) {
  // 0) Sensores incoherentes: parar y bloquear hasta que la lectura sea sana
  if (nivelesIncoherentes(n)) {
    setBomba(false);
    setAlerta(Alerta::SENSOR_FALLO);
    return;
  }
  if (alertaActiva == Alerta::SENSOR_FALLO) setAlerta(Alerta::NINGUNA);

  // 1) Protección marcha en seco: prioridad absoluta sobre todo lo demás
  if (!n.auxBajo) {
    if (bombaOn) {
      setBomba(false);
      setAlerta(Alerta::MARCHA_SECO);
    }
    return; // con el auxiliar vacío la bomba jamás arranca
  }
  // Hay agua en el auxiliar: la alerta de marcha en seco deja de aplicar
  if (alertaActiva == Alerta::MARCHA_SECO) setAlerta(Alerta::NINGUNA);

  if (bombaOn) {
    // 2) Parar al alcanzar nivel alto del principal
    if (n.ppalAlto) {
      setBomba(false);
      return;
    }
    // 3) Timeout de seguridad: si no llega al nivel alto en el tiempo máximo,
    //    parar y quedar bloqueada hasta reinicio (posible fuga o bomba dañada)
    if (millis() - bombaStartMs > PUMP_TIMEOUT_MS) {
      setBomba(false);
      timeoutLatch = true;
      setAlerta(Alerta::TIMEOUT_BOMBA);
    }
    return;
  }

  // El latch de timeout se limpia solo si el principal llega a alto por otra vía
  if (timeoutLatch) {
    if (n.ppalAlto) {
      timeoutLatch = false;
      setAlerta(Alerta::NINGUNA);
    }
    return;
  }

  // 4) Arranque: principal por debajo del nivel bajo, con reposo anti-ciclado
  bool reposoOk = (bombaStopMs == 0) || (millis() - bombaStopMs > PUMP_MIN_OFF_MS);
  if (!n.ppalBajo && reposoOk) {
    setBomba(true);
  }
}

// --------------------------------------------------------------- sensores

static float leerTemperatura() {
  ds18b20.requestTemperatures();
  float t = ds18b20.getTempCByIndex(0);
  if (t == DEVICE_DISCONNECTED_C || t < -40 || t > 85) return NAN;
  return t;
}

static int compararUint16(const void* a, const void* b) {
  return (int)(*(const uint16_t*)a) - (int)(*(const uint16_t*)b);
}

// Energiza la sonda solo durante la lectura (evitar electrólisis).
// Devuelve EC en µS/cm compensada a 25 °C; TDS = EC × EC_TO_TDS.
static void leerTdsEc(float tempC, float& ecOut, float& tdsOut) {
  digitalWrite(PIN_TDS_POWER, HIGH);
  delay(TDS_WARMUP_MS);

  uint16_t muestras[TDS_SAMPLES];
  for (int i = 0; i < TDS_SAMPLES; i++) {
    muestras[i] = analogRead(PIN_TDS);
    delay(20);
  }
  digitalWrite(PIN_TDS_POWER, LOW);

  qsort(muestras, TDS_SAMPLES, sizeof(uint16_t), compararUint16);
  float adc = muestras[TDS_SAMPLES / 2]; // mediana

  float v = adc * TDS_VREF / 4095.0f;
  // Compensación de temperatura (2 %/°C respecto a 25 °C); sin sonda de
  // temperatura válida se asume 25 °C
  float coefComp = isnan(tempC) ? 1.0f : (1.0f + 0.02f * (tempC - 25.0f));
  float vComp = v / coefComp;

  // Polinomio del TDS Meter V1.0 (DFRobot): resultado ≈ EC en µS/cm
  float ec = (133.42f * vComp * vComp * vComp
              - 255.86f * vComp * vComp
              + 857.39f * vComp) * TDS_KFACTOR;
  if (ec < 0) ec = 0;

  ecOut = ec;
  tdsOut = ec * EC_TO_TDS;
}

// ------------------------------------------------------------------- red

static const char* alertaTexto() {
  switch (alertaActiva) {
    case Alerta::MARCHA_SECO:   return "marcha_seco";
    case Alerta::TIMEOUT_BOMBA: return "timeout_bomba";
    case Alerta::SENSOR_FALLO:  return "sensor_fallo";
    default:                    return "";
  }
}

static void asegurarWifi() {
  if (WiFi.status() == WL_CONNECTED) return;
  Serial.println("[WIFI] Reconectando...");
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  for (int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++) delay(500);
  Serial.printf("[WIFI] %s\n", WiFi.status() == WL_CONNECTED ? "conectado" : "sin conexion");
}

static bool enviarDatos(const Niveles& n) {
  if (WiFi.status() != WL_CONNECTED) return false;

  // JSON según el contrato de datos (fijo, no cambiar sin avisar)
  String json = "{";
  json += "\"token\":\"" API_TOKEN "\",";
  json += "\"nivel_ppal_alto\":" + String(n.ppalAlto ? 1 : 0) + ",";
  json += "\"nivel_ppal_bajo\":" + String(n.ppalBajo ? 1 : 0) + ",";
  json += "\"nivel_aux_alto\":" + String(n.auxAlto ? 1 : 0) + ",";
  json += "\"nivel_aux_medio\":" + String(n.auxMedio ? 1 : 0) + ",";
  json += "\"nivel_aux_bajo\":" + String(n.auxBajo ? 1 : 0) + ",";
  json += "\"temp_c\":" + (isnan(ultimaTempC) ? String("null") : String(ultimaTempC, 1)) + ",";
  json += "\"tds_ppm\":" + (isnan(ultimoTds) ? String("null") : String(ultimoTds, 0)) + ",";
  json += "\"ec_us\":" + (isnan(ultimoEc) ? String("null") : String(ultimoEc, 0)) + ",";
  json += "\"bomba\":" + String(bombaOn ? 1 : 0) + ",";
  json += "\"alerta\":\"" + String(alertaTexto()) + "\",";
  json += "\"rssi\":" + String(WiFi.RSSI());

  // Timestamp NTP opcional (el backend pone el suyo si no llega)
  time_t ahora = time(nullptr);
  if (ahora > 1600000000) {
    json += ",\"ts\":" + String((unsigned long)ahora);
  }
  json += "}";

  // HTTPS sin validar CA: Apps Script redirige entre dominios de Google y
  // fijar certificados obligaría a reflashear cuando roten. El token del
  // payload es la autenticación real.
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.begin(client, API_URL);
  http.addHeader("Content-Type", "application/json");
  // Apps Script responde con redirección 302: hay que seguirla
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  http.setTimeout(15000);

  int code = http.POST(json);
  Serial.printf("[HTTP] POST -> %d\n", code);
  http.end();
  return code >= 200 && code < 400;
}

// ------------------------------------------------------------------ setup

void setup() {
  Serial.begin(115200);
  Serial.println("\n[BOOT] Monitor hidroponia");

  // Relé primero y en reposo: la bomba no debe dar tirones al arrancar
  pinMode(PIN_RELE, OUTPUT);
#if RELE_ACTIVO_BAJO
  digitalWrite(PIN_RELE, HIGH);
#else
  digitalWrite(PIN_RELE, LOW);
#endif

  pinMode(PIN_PPAL_ALTO, INPUT);
  pinMode(PIN_PPAL_BAJO, INPUT);
  // GPIO34/36/39 son solo-entrada y sin pull-up interno: si el multímetro
  // confirma salida en colector abierto, añadir pull-up externo (README)
  pinMode(PIN_AUX_ALTO, INPUT);
  pinMode(PIN_AUX_MEDIO, INPUT);
  pinMode(PIN_AUX_BAJO, INPUT);

  pinMode(PIN_TDS_POWER, OUTPUT);
  digitalWrite(PIN_TDS_POWER, LOW); // sonda TDS sin alimentar por defecto

  analogReadResolution(12);
  analogSetPinAttenuation(PIN_TDS, ADC_11db); // rango completo 0-3.3 V

  // DS18B20 con pull-up interno (~45 kΩ): válido para 1 sensor y cable corto.
  // El registro de pull-up del ESP32 persiste aunque OneWire cambie el modo.
  gpio_pullup_en((gpio_num_t)PIN_ONEWIRE);
  ds18b20.begin();
  Serial.printf("[TEMP] Sensores DS18B20 detectados: %d\n", ds18b20.getDeviceCount());

  WiFi.mode(WIFI_STA);
  asegurarWifi();
  configTime(0, 0, NTP_SERVER); // epoch UTC; el frontend muestra hora local
}

// ------------------------------------------------------------------- loop

void loop() {
  asegurarWifi();

  Niveles n = leerNiveles();
  controlBomba(n);

  unsigned long ahora = millis();
  bool tocaEnviar = envioInmediato || (ahora - ultimoEnvioMs >= SEND_INTERVAL_MS) || ultimoEnvioMs == 0;

  if (tocaEnviar) {
    ultimaTempC = leerTemperatura();
    leerTdsEc(ultimaTempC, ultimoEc, ultimoTds);

    Serial.printf("[DATOS] ppal(A%u B%u) aux(A%u M%u B%u) T=%.1fC EC=%.0fuS TDS=%.0fppm bomba=%u alerta=%s\n",
                  n.ppalAlto, n.ppalBajo, n.auxAlto, n.auxMedio, n.auxBajo,
                  ultimaTempC, ultimoEc, ultimoTds, bombaOn, alertaTexto());

    if (enviarDatos(n)) {
      ultimoEnvioMs = ahora;
      envioInmediato = false;
    } else if (envioInmediato) {
      // No bloquear el control de bomba reintentando en bucle: se reintenta
      // en el siguiente ciclo normal
      envioInmediato = false;
      ultimoEnvioMs = ahora;
    }
  }

  // Ciclo de control corto para reaccionar rápido a los niveles;
  // leerNiveles ya invierte ~250 ms en el muestreo
  delay(750);
}
