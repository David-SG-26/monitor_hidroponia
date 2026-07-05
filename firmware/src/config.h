#pragma once

// ============================== CREDENCIALES ==============================
// Rellenar antes de flashear. No subir credenciales reales a GitHub.
#define WIFI_SSID   "PON_AQUI_TU_SSID"
#define WIFI_PASS   "PON_AQUI_TU_PASSWORD"

// URL /exec de la implementación web del Apps Script (backend/Code.gs)
#define API_URL     "PON_AQUI_LA_URL_EXEC_DE_APPS_SCRIPT"
// Debe coincidir con la propiedad TOKEN fijada en setup() del Apps Script
#define API_TOKEN   "PON_AQUI_UN_TOKEN_SECRETO"

// ================================= PINES ==================================
// Asignación confirmada en resumen_para_fable5_hidroponia.md
#define PIN_PPAL_ALTO   32   // nivel principal alto (para llenado)
#define PIN_PPAL_BAJO   33   // nivel principal bajo (arranca llenado)
#define PIN_AUX_ALTO    34   // nivel auxiliar medio-alto  (solo entrada, SIN pull-up interno)
#define PIN_AUX_MEDIO   36   // nivel auxiliar medio (VP)  (solo entrada, SIN pull-up interno)
#define PIN_AUX_BAJO    39   // nivel auxiliar bajo (VN)   (solo entrada, SIN pull-up interno)
#define PIN_TDS         35   // TDS/EC analógico — ADC1 obligatorio (ADC2 no funciona con WiFi)
#define PIN_ONEWIRE     4    // DS18B20 (pull-up interno como primera opción)
#define PIN_RELE        26   // relé de la bomba

// PIN AÑADIDO (no estaba en la tabla original): alimentación conmutada de la
// sonda TDS para evitar electrólisis — se energiza solo durante la lectura.
// Si prefieres otro pin o alimentar el módulo de otra forma, cámbialo aquí.
#define PIN_TDS_POWER   25

// Nivel lógico del relé: la mayoría de módulos de relé de 1 canal son
// activos a nivel BAJO (IN=LOW → relé cerrado). Ajustar tras probar.
#define RELE_ACTIVO_BAJO 1

// Interruptor de software de la bomba:
//   1 = control automático normal
//   0 = modo solo monitorización: el relé NUNCA se activa (sin alertas de
//       bomba); útil para dejar los sensores en producción antes de conectar
//       el circuito de 12 V
#define BOMBA_HABILITADA 1

// Sensores XKC-Y25 (salida por voltaje) — confirmado en pruebas: agua = 1,
// sin agua = 0. Cableado real: rojo/rosa = +5V, azul = GND, amarillo = señal,
// negro = modo (se deja AISLADO, sin conectar). La señal activa mide ~4,94V
// (medido con multímetro), así que cada amarillo pasa por un DIVISOR de 3
// resistencias de 10k: R1 (10k) arriba y R2+R3 (20k) abajo a GND. El nodo entre
// R1 y R2 (~3,29V) va al GPIO. Si alguna unidad invierte la lógica, poner
// NIVEL_AGUA_PRESENTE 0.
#define NIVEL_AGUA_PRESENTE 1

// ================================ TIEMPOS =================================
#define SEND_INTERVAL_MS      60000UL    // envío al backend: cada 60 s
#define PUMP_TIMEOUT_MS       300000UL   // seguridad: máx. 5 min de bomba seguidos
#define PUMP_MIN_OFF_MS       30000UL    // reposo mínimo entre arranques (anti-ciclado)
#define LEVEL_SAMPLES         5          // muestras por sensor de nivel (voto por mayoría)
#define LEVEL_SAMPLE_GAP_MS   10

// Confirmación anti-falsos-positivos. Cada vuelta del loop dura ~1 s (incluye el
// muestreo de niveles), así que una condición debe MANTENERSE varias vueltas
// seguidas antes de actuar: evita reaccionar a olas, salpicaduras o ruido puntual
// del sensor, y evita el spam de POST por alertas que parpadean.
#define ARRANQUE_CONFIRMACIONES 3        // vueltas seguidas pidiendo agua antes de ARRANCAR la bomba (~3 s)
#define CONFIRMAR_CICLOS        3        // vueltas seguidas antes de declarar/limpiar "sensor_fallo" (~3 s)
#define TDS_WARMUP_MS         500        // estabilización de la sonda tras energizarla
#define TDS_SAMPLES           15         // muestras ADC (se usa la mediana)

// =============================== CALIBRACIÓN ==============================
#define TDS_VREF        3.3f    // tensión de referencia del ADC
#define TDS_KFACTOR     1.0f    // factor de calibración con solución patrón (ajustar)
#define EC_TO_TDS       0.5f    // escala ppm-500 (TDS = EC × 0.5)

// ================================== NTP ===================================
#define NTP_SERVER      "pool.ntp.org"
