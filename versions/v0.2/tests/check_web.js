const fs = require('fs');
const path = require('path');
const vm = require('vm');
const assert = require('assert');
const base = path.join(__dirname, '../arduino-esp01s-esp01s-ap-wifi-io0');
const read = f => fs.readFileSync(path.join(base, f), 'utf8');
const header = read('esp01s_ap_lock/web_ui.h');
const embedded = name => {
  const m = header.match(new RegExp('const char ' + name + '\\[\\] PROGMEM = R"rawliteral\\(([\\s\\S]*?)\\)rawliteral";'));
  assert(m, name + ' missing');
  return m[1];
};
for (const [label, js, html] of [
  ['standalone', read('app.js'), read('index.html') + read('admin.html')],
  ['embedded', embedded('PAGE_JS'), embedded('PAGE_INDEX') + embedded('PAGE_ADMIN')]
]) {
  new vm.Script(js, { filename: label + '.js' });
  assert(!/winStart|winEnd|dbgMode/.test(js + html));
  for (const match of js.matchAll(/\$\('([^']+)'\)/g)) {
    assert(html.includes('id="' + match[1] + '"'), label + ': missing #' + match[1]);
  }
  assert(js.includes("powered: '上电开启'"));
  console.log('PASS: ' + label + ' JS syntax, DOM references and removed schedule controls');
}
const ino = read('esp01s_ap_lock/esp01s_ap_lock.ino');
assert(!/WIFI_OFF|WIFI_DUTY|wifiStop|inWindow\(/.test(ino));
assert(/setupRoutes\(\);\s*wifiStart\(\);/.test(ino));
const oldRoot = path.join(base, '../../v0.1/arduino-esp01s-esp01s-ap-wifi-io0/esp01s_ap_lock');
assert.strictEqual(read('esp01s_ap_lock/config_store.h'), fs.readFileSync(path.join(oldRoot, 'config_store.h'), 'utf8'));
const old = fs.readFileSync(path.join(oldRoot, 'esp01s_ap_lock.ino'), 'utf8').replace(/\r\n/g, '\n');
const current = ino.replace(/\r\n/g, '\n');
for (const [start, end] of [
  ['static bool saveConfigNow()', '/* ============================================================================\n *  时间同步'],
  ['static String jsonStr(', 'static String statusJson()'],
  ['static void handleSync()', 'static void handleConfig()'],
  ['static bool parseCodes(', 'static int toMin('],
  ['static void handleSaveAngles()', 'void setup()']
]) {
  const oldEnd = old.indexOf(end, old.indexOf(start));
  // toMin was deleted; handleSaveNet now follows the unchanged code handlers.
  const newEndText = end === 'static int toMin(' ? 'static void handleSaveNet()' : end;
  const newEnd = current.indexOf(newEndText, current.indexOf(start));
  assert(oldEnd >= 0 && newEnd >= 0);
  const normalize = s => s.replace(/\n{3,}/g, '\n\n').trim();
  assert.strictEqual(normalize(current.slice(current.indexOf(start), newEnd)), normalize(old.slice(old.indexOf(start), oldEnd)), start);
}
console.log('PASS: WiFi always-on setup; storage, servo, password/auth and motion handlers unchanged');
