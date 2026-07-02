# Proyecto hidroponía ESP32 — contexto para generación de código (desde 0 en GitHub)

## Qué hay que construir
Repositorio nuevo desde cero con tres partes:
1. **Backend**: Google Apps Script (`Code.gs`) que recibe POST autenticados del ESP32, guarda en Google Sheets (hoja `Historico`) y sirve estado/histórico vía GET. Incluir función de prueba sin hardware y fallback JSONP.
2. **Frontend**: página estática para GitHub Pages (`index.html`) con visualización tipo "vasija" animada de nivel para ambos depósitos, lecturas de temperatura y EC (con opción de alternar a TDS), estado de bomba, banner de alertas, gráfico histórico (Chart.js) y detección automática online/stale/offline según antigüedad de la lectura.
3. **Firmware ESP32** (Arduino/PlatformIO): lee sensores, controla bomba vía relé con protecciones de seguridad, y envía datos al backend según el contrato de datos.

Orden de construcción: backend y frontend primero (para fijar el contrato de datos), firmware después.

## Contrato de datos (JSON, fijo, no cambiar sin avisar)
```
token, nivel_ppal_alto, nivel_ppal_bajo,
nivel_aux_alto, nivel_aux_medio, nivel_aux_bajo,
temp_c, tds_ppm, ec_us, bomba, alerta, rssi
```
- Sensores de nivel: `1` = agua presente, `0` = ausente
- `alerta`: `""` (normal) | `"marcha_seco"` | `"timeout_bomba"` | `"sensor_fallo"`
- Timestamp lo pone el backend; admite override opcional vía campo `ts` (epoch NTP)

## Hardware
- **Microcontrolador**: ESP32-WROOM-32 (DevKit, USB-C, también jack DC)
- **Depósito principal**: 60L | **Depósito auxiliar**: 100L
- **Sensores de nivel**: 5x XKC-Y25 (4 cables: rojo/rosa=+, negro=GND, amarillo=señal, azul=modo). Confirmado en pruebas: agua=1, sin agua=0
- **Temperatura**: DS18B20 impermeable (1-Wire), pull-up interno del ESP32 como primera opción (no hay resistencias de 4.7kΩ disponibles, solo 120Ω)
- **TDS/EC**: sensor tipo TDS Meter V1.0, no alimentar en continuo (evitar electrólisis, activar solo durante la lectura)
- **Actuación de bomba**: **relé** (no MOSFET — decisión del usuario, solo se usan los componentes ya disponibles), bomba 12V DC 3-5W, diodo HER208 en flyback en paralelo a la bomba
- **Alimentación**: 12V → fusible → interruptor → divisor → (módulo buck a 5V/3.3V para ESP32) + (relé/bomba). Masa común en todo el sistema

## Asignación de pines (ESP32)
Reparto de sensores confirmado: **2 en el depósito principal** (alto = para de llenar, bajo = arranca llenado) y **3 en el auxiliar** (medio-alto, medio, bajo = bloquea la bomba para evitar marcha en seco). Usa los 5 sensores XKC-Y25 disponibles, sin repuesto libre.

| Función | Pin |
|---|---|
| Nivel principal alto (para llenado) | GPIO32 |
| Nivel principal bajo (arranca llenado) | GPIO33 |
| Nivel auxiliar medio-alto | GPIO34 |
| Nivel auxiliar medio | GPIO36 (VP) |
| Nivel auxiliar bajo (corta bomba, anti marcha en seco) | GPIO39 (VN) |
| TDS/EC (analógico) | GPIO35 — **debe ser ADC1**, ADC2 no funciona con WiFi activo |
| Temperatura DS18B20 | GPIO4 |
| Relé bomba (salida) | GPIO26 |
| Alimentación ESP32 | 5V → VIN |

## Lógica de control esperada
1. Leer niveles: principal (alto, bajo) y auxiliar (medio-alto, medio, bajo)
2. Si principal está bajo (no alto) Y auxiliar bajo tiene agua → activar bomba
3. Nunca activar bomba si el sensor auxiliar bajo indica vacío (protección marcha en seco), pase lo que pase con los sensores del principal
4. Apagar bomba al llegar a nivel principal alto o al cumplir timeout de seguridad → generar alerta si aplica
5. Leer temperatura y TDS/EC (activar sonda TDS solo durante la lectura)
6. Enviar datos al backend según el contrato
7. Registrar activaciones de bomba e histórico

## Restricciones técnicas clave a respetar en el código
- TDS en ADC1 obligatorio (WiFi + ADC2 son incompatibles)
- No mantener la sonda TDS energizada de forma continua
- Compensar EC por temperatura si es viable
- GitHub Pages es estático: no recibe POST, solo lee del backend (Apps Script + Sheets)

## Dashboard: EC/TDS
Mostrar **EC como valor principal**, con un selector/toggle en la UI para alternar a TDS (ambos valores ya vienen en el payload del ESP32, es solo cambio de vista, no de datos).

## Preguntas abiertas pendientes de confirmar con el usuario antes/durante el desarrollo
1. Resultado de verificación con multímetro: tensión de salida real de los sensores XKC-Y25 (3.3V directo / necesita pull-up / necesita divisor resistivo)
