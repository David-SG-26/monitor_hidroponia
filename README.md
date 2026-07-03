# Monitor hidroponía · ESP32 + Google Sheets + GitHub Pages

Sistema de monitorización y control de llenado para un cultivo hidropónico con
dos depósitos (principal 60 L, auxiliar 100 L), bomba de trasvase 12 V y
dashboard web.

```
ESP32 ──POST──▶ Google Apps Script ──▶ Google Sheets (hoja "Historico")
                        ▲
GitHub Pages (docs/) ──GET── estado + histórico (JSON / JSONP)
```

| Parte | Carpeta | Qué es |
|---|---|---|
| Backend | `backend/Code.gs` | Apps Script: recibe POST autenticados, guarda en Sheets, sirve GET |
| Frontend | `docs/index.html` | Dashboard estático para GitHub Pages |
| Firmware | `firmware/` | Proyecto PlatformIO para ESP32-WROOM-32 |
| Guía de montaje | `docs/conexiones.html` | Cableado paso a paso + código para Arduino IDE (se publica con Pages) |

## Contrato de datos (fijo — no cambiar sin avisar)

```json
{
  "token": "…",
  "nivel_ppal_alto": 0, "nivel_ppal_bajo": 1,
  "nivel_aux_alto": 1, "nivel_aux_medio": 1, "nivel_aux_bajo": 1,
  "temp_c": 21.4, "tds_ppm": 640, "ec_us": 1280,
  "bomba": 0, "alerta": "", "rssi": -61
}
```

- Niveles: `1` = agua presente, `0` = ausente
- `alerta`: `""` | `"marcha_seco"` | `"timeout_bomba"` | `"sensor_fallo"`
- El timestamp lo pone el backend; el ESP32 puede enviarlo opcionalmente en `ts` (epoch NTP, segundos)

## Puesta en marcha

### 1. Backend (Google Apps Script)

1. Crea un Google Sheet vacío y copia su ID (el tramo largo de la URL).
2. En el Sheet: **Extensiones → Apps Script**, pega `backend/Code.gs`.
3. Edita la función `setup()` con el ID del Sheet y un token secreto, y ejecútala una vez.
4. Prueba sin hardware: ejecuta `testPost()` (una lectura) y `testSeedHistory()`
   (24 h de datos sintéticos para el dashboard). Comprueba la hoja `Historico`.
5. **Implementar → Nueva implementación → Aplicación web**:
   - Ejecutar como: **yo**
   - Quién tiene acceso: **cualquier usuario**
6. Copia la URL que termina en `/exec`: la necesitan el frontend y el firmware.

> Si cambias el código después, hay que crear una **nueva versión** de la
> implementación para que la URL `/exec` sirva el código nuevo.

### 2. Frontend (GitHub Pages)

1. Edita `docs/index.html` y pon la URL `/exec` en la constante `API_URL` (al principio del `<script>`).
2. En GitHub: **Settings → Pages → Deploy from a branch → `main` / carpeta `/docs`**.
3. Abre `https://<usuario>.github.io/monitor_hidroponia/`.

Características: vasijas animadas de ambos depósitos, temperatura, **EC con
toggle a TDS**, estado de bomba con **última activación y duración**, banner
de alertas, gráficos históricos (de 6 h a 3 meses; en rangos largos el backend
agrega las muestras a ~480 puntos) con **franjas azules en los tramos con la
bomba en marcha**, indicador de señal WiFi por barras, tabla de datos e
iconografía SVG propia. Estado de conexión calculado por la antigüedad de la
última lectura: en línea / desactualizado (>5 min) / sin conexión (>15 min).

GitHub Pages es estático: la página solo **lee** del backend; los POST del
ESP32 van siempre al Apps Script.

### 3. Firmware (PlatformIO)

1. Edita `firmware/src/config.h`: WiFi, URL `/exec`, token (el mismo del backend).
2. Compila y flashea:
   ```bash
   cd firmware
   pio run -t upload
   pio device monitor        # log a 115200 baudios
   ```

## Hardware y cableado

Alimentación: 12 V → fusible → interruptor → (buck a 5 V → VIN del ESP32) +
(relé/bomba 12 V). **Masa común en todo el sistema.** Diodo HER208 en flyback
en paralelo con la bomba.

| Función | Pin ESP32 |
|---|---|
| Nivel principal alto (para llenado) | GPIO32 |
| Nivel principal bajo (arranca llenado) | GPIO33 |
| Nivel auxiliar medio-alto | GPIO34 |
| Nivel auxiliar medio | GPIO36 (VP) |
| Nivel auxiliar bajo (anti marcha en seco) | GPIO39 (VN) |
| TDS/EC analógico | GPIO35 (**ADC1** — ADC2 no funciona con WiFi) |
| **Alimentación conmutada sonda TDS** | **GPIO25 (añadido, ver nota)** |
| Temperatura DS18B20 | GPIO4 (pull-up interno) |
| Relé bomba | GPIO26 |

XKC-Y25 (5 uds.): rojo/rosa = +, negro = GND, amarillo = señal, azul = modo.
Confirmado: agua = `1`, sin agua = `0`.

### Notas de diseño del firmware

- **GPIO25 añadido a la tabla original**: la sonda TDS no debe estar
  energizada en continuo (electrólisis), así que su alimentación se conmuta
  con un GPIO y solo se activa ~0,8 s durante cada lectura. Si el módulo TDS
  consume más de lo que da un GPIO (~40 mA máx., usar 20 mA como límite
  prudente), alimentarlo a 3.3 V/5 V y conmutar con un transistor NPN/MOSFET
  de señal. Configurable en `config.h` (`PIN_TDS_POWER`).
- **DS18B20 con pull-up interno** (~45 kΩ) como primera opción por no haber
  resistencias de 4.7 kΩ: válido para un solo sensor con cable corto. Si las
  lecturas fallan (aparece `-127 °C`), hará falta pull-up externo.
- **Relé activo a nivel bajo** por defecto (`RELE_ACTIVO_BAJO 1` en
  `config.h`): la mayoría de módulos de relé de 1 canal disparan con IN=LOW.
  Verificar con el módulo real y ajustar.
- Lectura de niveles con **voto por mayoría** (5 muestras) para filtrar ruido.
- EC compensada por temperatura (2 %/°C respecto a 25 °C); TDS = EC × 0.5
  (escala ppm-500). Calibrar `TDS_KFACTOR` con solución patrón.

### Lógica de control de la bomba

1. Arranca cuando el principal baja del nivel bajo **y** el auxiliar-bajo tiene agua.
2. El sensor auxiliar-bajo **bloquea la bomba de forma absoluta** si indica
   vacío, pase lo que pase con el resto (protección de marcha en seco →
   alerta `marcha_seco`, se limpia sola cuando vuelve el agua).
3. Para al llegar al nivel principal alto.
4. Timeout de seguridad (5 min por defecto): si no alcanza el nivel alto, se
   para con alerta `timeout_bomba` y queda **bloqueada hasta reinicio** (o
   hasta que el principal llegue a alto por otra vía) — puede indicar fuga o
   bomba dañada.
5. Lecturas incoherentes (agua arriba sin agua abajo) → alerta `sensor_fallo`
   y bomba bloqueada mientras dure.
6. Reposo mínimo de 30 s entre arranques (anti-ciclado del relé).

## Pendiente de confirmar

- **Salida real de los XKC-Y25 con multímetro**: si es 3.3 V directo, el
  cableado actual vale tal cual. Si es colector abierto o saca >3.3 V,
  GPIO34/36/39 necesitan **pull-up externo o divisor resistivo** (esos pines
  no tienen pull-up interno). El nivel activo es configurable en `config.h`
  (`NIVEL_AGUA_PRESENTE`).
- Nivel de disparo del módulo de relé (activo alto/bajo) → `RELE_ACTIVO_BAJO`.
- Calibración del TDS con solución patrón → `TDS_KFACTOR`.
