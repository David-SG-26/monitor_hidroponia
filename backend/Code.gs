/**
 * Backend hidroponía — Google Apps Script
 * ----------------------------------------
 * Recibe POST autenticados del ESP32, guarda en la hoja "Historico" de un
 * Google Sheet y sirve estado/histórico vía GET (JSON o JSONP).
 *
 * DESPLIEGUE (resumen; detalle en el README del repo):
 *   1. Crear un Google Sheet vacío y copiar su ID (el tramo largo de la URL).
 *   2. Extensiones → Apps Script → pegar este fichero.
 *   3. Ejecutar una vez `setup()` para fijar SHEET_ID y TOKEN en Script Properties.
 *   4. Implementar → Nueva implementación → "Aplicación web":
 *        - Ejecutar como: yo
 *        - Acceso: cualquier usuario
 *   5. Copiar la URL /exec resultante: es la que usan el ESP32 y el frontend.
 *
 * CONTRATO DE DATOS (fijo, no cambiar sin avisar):
 *   token, nivel_ppal_alto, nivel_ppal_bajo,
 *   nivel_aux_alto, nivel_aux_medio, nivel_aux_bajo,
 *   temp_c, tds_ppm, ec_us, bomba, alerta, rssi
 *   - Sensores de nivel: 1 = agua presente, 0 = ausente
 *   - alerta: "" | "marcha_seco" | "timeout_bomba" | "sensor_fallo"
 *   - Timestamp lo pone el backend; override opcional vía campo `ts` (epoch, s)
 */

var SHEET_NAME = 'Historico';

var HEADERS = [
  'timestamp',
  'nivel_ppal_alto', 'nivel_ppal_bajo',
  'nivel_aux_alto', 'nivel_aux_medio', 'nivel_aux_bajo',
  'temp_c', 'tds_ppm', 'ec_us',
  'bomba', 'alerta', 'rssi'
];

/**
 * Ejecutar UNA VEZ a mano tras pegar el código: fija el ID de la hoja y el
 * token compartido con el ESP32. Cambia los dos valores antes de ejecutar.
 */
function setup() {
  var props = PropertiesService.getScriptProperties();
  props.setProperty('SHEET_ID', 'PON_AQUI_EL_ID_DEL_SHEET');
  props.setProperty('TOKEN', 'PON_AQUI_UN_TOKEN_SECRETO');
  Logger.log('Propiedades guardadas. Ejecuta testPost() para probar sin hardware.');
}

function getProp_(key) {
  var v = PropertiesService.getScriptProperties().getProperty(key);
  if (!v) throw new Error('Falta la propiedad "' + key + '". Ejecuta setup() primero.');
  return v;
}

function getSheet_() {
  var ss = SpreadsheetApp.openById(getProp_('SHEET_ID'));
  var sheet = ss.getSheetByName(SHEET_NAME);
  if (!sheet) {
    sheet = ss.insertSheet(SHEET_NAME);
    sheet.appendRow(HEADERS);
    sheet.setFrozenRows(1);
  }
  return sheet;
}

// ---------------------------------------------------------------- POST (ESP32)

function doPost(e) {
  try {
    var data = JSON.parse(e.postData.contents);

    if (data.token !== getProp_('TOKEN')) {
      return jsonOut_({ ok: false, error: 'token invalido' });
    }

    // Timestamp del backend; override opcional con `ts` (epoch en segundos, NTP)
    var ts = new Date();
    if (data.ts && isFinite(data.ts) && Number(data.ts) > 1e9) {
      ts = new Date(Number(data.ts) * 1000);
    }

    var row = [
      ts,
      num_(data.nivel_ppal_alto), num_(data.nivel_ppal_bajo),
      num_(data.nivel_aux_alto), num_(data.nivel_aux_medio), num_(data.nivel_aux_bajo),
      num_(data.temp_c), num_(data.tds_ppm), num_(data.ec_us),
      num_(data.bomba), String(data.alerta || ''), num_(data.rssi)
    ];

    // Lock para evitar filas corruptas si llegan dos POST a la vez
    var lock = LockService.getScriptLock();
    lock.waitLock(10000);
    try {
      getSheet_().appendRow(row);
    } finally {
      lock.releaseLock();
    }

    return jsonOut_({ ok: true });
  } catch (err) {
    return jsonOut_({ ok: false, error: String(err) });
  }
}

function num_(v) {
  // null/undefined/'' se guardan como celda vacía (Number(null) sería 0)
  if (v === null || v === undefined || v === '') return '';
  var n = Number(v);
  return isFinite(n) ? n : '';
}

// ------------------------------------------------------------- GET (frontend)

/**
 * GET params:
 *   action=status                → última lectura
 *   action=history&hours=24      → histórico de las últimas N horas (def. 24, máx. 168)
 *   callback=fn                  → envuelve la respuesta en JSONP (fallback CORS)
 */
function doGet(e) {
  var p = (e && e.parameter) || {};
  var action = p.action || 'status';
  var payload;

  try {
    if (action === 'history') {
      var hours = Math.min(Math.max(Number(p.hours) || 24, 1), 168);
      payload = { ok: true, rows: readHistory_(hours) };
    } else {
      payload = { ok: true, row: readLast_() };
    }
  } catch (err) {
    payload = { ok: false, error: String(err) };
  }

  if (p.callback) {
    return ContentService
      .createTextOutput(p.callback + '(' + JSON.stringify(payload) + ');')
      .setMimeType(ContentService.MimeType.JAVASCRIPT);
  }
  return jsonOut_(payload);
}

function readLast_() {
  var sheet = getSheet_();
  var last = sheet.getLastRow();
  if (last < 2) return null;
  var values = sheet.getRange(last, 1, 1, HEADERS.length).getValues()[0];
  return rowToObj_(values);
}

function readHistory_(hours) {
  var sheet = getSheet_();
  var last = sheet.getLastRow();
  if (last < 2) return [];

  var cutoff = Date.now() - hours * 3600 * 1000;
  // Leer como mucho las últimas 2000 filas para no agotar el tiempo de ejecución
  var maxRows = 2000;
  var start = Math.max(2, last - maxRows + 1);
  var values = sheet.getRange(start, 1, last - start + 1, HEADERS.length).getValues();

  var out = [];
  for (var i = 0; i < values.length; i++) {
    // La celda puede llegar como Date o como texto según cómo se escribiera:
    // aceptar ambos en vez de exigir instanceof Date
    var t = values[i][0];
    var time = (t instanceof Date) ? t.getTime() : new Date(t).getTime();
    if (isFinite(time) && time >= cutoff) {
      out.push(rowToObj_(values[i]));
    }
  }
  return out;
}

function rowToObj_(values) {
  var obj = {};
  for (var i = 0; i < HEADERS.length; i++) {
    var v = values[i];
    obj[HEADERS[i]] = (v instanceof Date) ? v.toISOString() : v;
  }
  return obj;
}

function jsonOut_(obj) {
  return ContentService
    .createTextOutput(JSON.stringify(obj))
    .setMimeType(ContentService.MimeType.JSON);
}

// ------------------------------------------------------- Pruebas sin hardware

/**
 * Simula un POST del ESP32 sin hardware. Ejecutar desde el editor de Apps
 * Script tras setup(); revisa la hoja "Historico" y el log.
 */
function testPost() {
  var fake = {
    postData: {
      contents: JSON.stringify({
        token: getProp_('TOKEN'),
        nivel_ppal_alto: 0,
        nivel_ppal_bajo: 1,
        nivel_aux_alto: 1,
        nivel_aux_medio: 1,
        nivel_aux_bajo: 1,
        temp_c: 21.4,
        tds_ppm: 640,
        ec_us: 1280,
        bomba: 0,
        alerta: '',
        rssi: -61
      })
    }
  };
  var res = doPost(fake);
  Logger.log('doPost → ' + res.getContent());
  Logger.log('status → ' + doGet({ parameter: { action: 'status' } }).getContent());
}

/** Genera 24 h de datos sintéticos para probar el dashboard sin ESP32. */
function testSeedHistory() {
  var sheet = getSheet_();
  var now = Date.now();
  var rows = [];
  for (var i = 96; i >= 0; i--) { // una lectura cada 15 min, 24 h
    var t = new Date(now - i * 15 * 60 * 1000);
    var temp = 20 + 3 * Math.sin(i / 12) + Math.random() * 0.4;
    var ec = 1250 + 60 * Math.sin(i / 20) + Math.random() * 20;
    rows.push([
      t, 0, 1, 1, 1, 1,
      Math.round(temp * 10) / 10,
      Math.round(ec / 2),
      Math.round(ec),
      (i % 30 === 0) ? 1 : 0,
      '',
      -55 - Math.round(Math.random() * 15)
    ]);
  }
  sheet.getRange(sheet.getLastRow() + 1, 1, rows.length, HEADERS.length).setValues(rows);
  Logger.log('Insertadas ' + rows.length + ' filas sintéticas.');
}
