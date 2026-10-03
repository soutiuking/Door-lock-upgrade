/*
 * ============================================================================
 *  ESP-01S AP 网关门锁固件（Arduino / ESP8266 核心）
 * ============================================================================
 *
 *  功能概览
 *   · 自建 SoftAP + 内置网页（PROGMEM 内嵌，无需文件系统）
 *   · 用户端：输入 6 位密码开门；管理员端：10 组密码 / 密钥 / 时段 / 调试
 *   · 低功耗：平时每 5 分钟开 1 分钟 WiFi；常开时段内常态开启；调试模式常开
 *            模式由 DEBUG_MODE 决定（0 正常 / 1 调试），上电即生效
 *   · 时间同步：外部设备连上 AP 时同步（浏览器推送本地时间 / 上行路由 NTP）
 *   · Flash 均匀磨损：64 槽位日志式轮转写入（见 config_store.h）
 *
 *  引脚定义（需求一）
 *   · IO0  = 舵机供电开关：置高供电 / 置低断电
 *            ⚠ IO0 只能作为 MOSFET/三极管的控制脚，绝不可直接带动舵机！
 *            ⚠ IO0 是启动模式脚：上电瞬间必须为高（ESP-01S 板载 10K 上拉已
 *              满足），固件起来后才拉低断电，属正常运行行为。
 *   · IO1  = 舵机 PWM 信号（50Hz）
 *            ⚠ IO1 即 TXD0。使用串口打印会干扰舵机信号，因此默认
 *              ENABLE_SERIAL = 0；若需串口调试，请把 PIN_SERVO_SIG 改为 3(RX)
 *              或改用其它空闲脚，并把 ENABLE_SERIAL 置 1。
 *
 *  默认参数
 *   · AP 名称  LockGate-<芯片ID>      密码 88888888    地址 192.168.4.1
 *   · 管理员密钥 888888（登录后请立即修改）
 *   · WiFi 常开时段 08:00 - 22:00；运行模式 DEBUG_MODE = 1（调试，WiFi 常开）
 *
 *  仅需标准 ESP8266 Arduino Core（含 Servo / ESP8266WebServer），无第三方库。
 * ============================================================================
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Servo.h>
#include <time.h>
#include <sys/time.h>                   // settimeofday / struct timeval
#include <coredecls.h>

#include "config_store.h"
#include "web_ui.h"

/* ------------------------- 引脚与常量 ------------------------- */
#define PIN_SERVO_PWR   0       // IO0：舵机供电开关（外接 MOS 管）
#define PIN_SERVO_SIG   1       // IO1：舵机 PWM 信号
#define ENABLE_SERIAL   0       // 使用 IO1 输出 PWM 时必须为 0

#define DEBUG_MODE      1       // 0 = 正常模式（5 分钟开 1 分钟省电）；1 = 调试模式（WiFi 常开）

#define AP_PASSWORD     "88888888"                       // SoftAP 密码（≥8 位）
#define AP_CHANNEL      1
#define TZ_OFFSET_SEC   (8 * 3600)                       // 东八区

#define WIFI_DUTY_PERIOD_MS  (5UL * 60UL * 1000UL)       // 占空比周期：5 分钟
#define WIFI_DUTY_ON_MS      (1UL * 60UL * 1000UL)       // 其中开启：1 分钟
#define SERVO_POWER_SETTLE_MS 350                        // Wait for MG996R supply to stabilize
#define SERVO_MIN_US          600                        // MG996R minimum pulse width
#define SERVO_MAX_US          2400                       // MG996R maximum pulse width
#define SERVO_PWR_ACTIVE_LEVEL HIGH                      // Change to LOW for an active-low power switch
#define TOKEN_VALID_MS       (30UL * 60UL * 1000UL)      // 后台会话 30 分钟
#define WIFI_POLICY_TICK_MS  250                         // 策略检查节流，优先保证 HTTP 响应

/* ------------------------- 全局对象 ------------------------- */
ESP8266WebServer server(80);
Servo          servo;
ParamStore     store;
LockConfig     cfg;

String apSsid = "LockGate";


/* 后台会话 */
String   adminToken;
uint32_t tokenExpire = 0;

/* WiFi 策略状态 */
bool        wifiOn = false;
const char* wifiReason = "duty";
bool        prevInDuty = false;
uint32_t    dutyStart = 0;
bool        dutyValid = false;                    // dutyStart 是否已初始化（millis() 可能为 0，不能拿它判空）
uint8_t     lastStations = 0;
bool        sntpActive = false;
uint32_t    lastNetCheck = 0;
uint32_t    lastWifiPolicyTick = 0;

/* 舵机状态机 */
enum SrvState : uint8_t {
  SRV_OFF, SRV_POWER, SRV_UNLOCK, SRV_HOLD, SRV_LOCK, SRV_SETTLE,
  SRV_MANUAL_POWER, SRV_MANUAL
};
SrvState srvState = SRV_OFF;
uint32_t srvT = 0;
bool     pwrOn = false;
uint16_t testAngle = 90;

/* ============================================================================
 *  配置载入 / 保存
 * ============================================================================ */
static void sanitizeConfig() {
  if (cfg.version != LOCK_CONFIG_VERSION) cfg.version = LOCK_CONFIG_VERSION;
  if (cfg.adminKey[0] == '\0') { strncpy(cfg.adminKey, "888888", sizeof(cfg.adminKey) - 1); }
  cfg.adminKey[sizeof(cfg.adminKey) - 1] = '\0';
  if (cfg.lockAngle > 180)   cfg.lockAngle = 0;
  if (cfg.unlockAngle > 180) cfg.unlockAngle = 90;
  if (cfg.unlockHoldMs < 500 || cfg.unlockHoldMs > 10000) cfg.unlockHoldMs = 2500;
  if (cfg.winStart > 1439) cfg.winStart = 8 * 60;
  if (cfg.winEnd > 1439)   cfg.winEnd = 22 * 60;
  cfg.uplinkSsid[sizeof(cfg.uplinkSsid) - 1] = '\0';
  cfg.uplinkPass[sizeof(cfg.uplinkPass) - 1] = '\0';
  testAngle = cfg.unlockAngle;
}

static void loadConfig() {
  bool loaded = false;
  if (store.ok()) loaded = store.load(&cfg, sizeof(cfg));
  if (!loaded) {
    memset(&cfg, 0, sizeof(cfg));
    cfg.version = LOCK_CONFIG_VERSION;
    strncpy(cfg.adminKey, "888888", sizeof(cfg.adminKey) - 1);
    cfg.winStart = 8 * 60;
    cfg.winEnd = 22 * 60;
    cfg.debugMode = 0;
    cfg.lockAngle = 0;
    cfg.unlockAngle = 90;
    cfg.unlockHoldMs = 2500;
    sanitizeConfig();
    if (store.ok()) store.save(&cfg, sizeof(cfg));   // 首次落盘
  } else {
    sanitizeConfig();
    // Preserve settings created by the previous firmware layout, then move
    // them into the safe configuration area before future writes.
    if (store.loadedFromLegacy()) store.save(&cfg, sizeof(cfg));
  }
  cfg.debugMode = DEBUG_MODE;                 // 上电模式由 DEBUG_MODE 决定（页面上的切换仅本次运行有效）
}

/* Save explicit configuration changes immediately and verify them in ParamStore. */
static bool saveConfigNow() {
  if (!store.ok()) return false;
  return store.save(&cfg, sizeof(cfg));
}

/* ============================================================================
 *  舵机控制（需求一：IO0 供电开关 + IO1 PWM）
 * ============================================================================ */
static void servoPower(bool on) {
  pwrOn = on;
  digitalWrite(PIN_SERVO_PWR, on ? SERVO_PWR_ACTIVE_LEVEL : !SERVO_PWR_ACTIVE_LEVEL);
}

static void servoAttachSignal() {
  if (!servo.attached()) servo.attach(PIN_SERVO_SIG, SERVO_MIN_US, SERVO_MAX_US);
}

static void servoDetachSignal() {
  if (servo.attached()) servo.detach();
  // Release signal to prevent back-powering an unpowered servo through its signal wire.
  pinMode(PIN_SERVO_SIG, INPUT);
}

static uint16_t servoAngleToUs(uint16_t angle) {
  if (angle > 180) angle = 180;
  return (uint16_t)(SERVO_MIN_US +
         ((uint32_t)(SERVO_MAX_US - SERVO_MIN_US) * angle) / 180UL);
}

static void servoWriteAngle(uint16_t angle) {
  servoAttachSignal();
  servo.writeMicroseconds(servoAngleToUs(angle));
}

static const char* lockStateStr() {
  switch (srvState) {
    case SRV_OFF:    return "off";
    case SRV_POWER:  return "powering";
    case SRV_UNLOCK: return "unlocking";
    case SRV_HOLD:   return "unlocking";
    case SRV_LOCK:   return "locking";
    case SRV_SETTLE:      return "locking";
    case SRV_MANUAL_POWER: return "powering";
    case SRV_MANUAL:       return "manual";
  }
  return "off";
}

/* 触发一次完整开锁动作；忙则忽略 */
static bool requestUnlock() {
  if (srvState != SRV_OFF) return false;
  servoPower(true);
  srvState = SRV_POWER;
  srvT = millis();
  return true;
}

static void servoTick() {
  uint32_t now = millis();
  switch (srvState) {
    case SRV_POWER:                                  // 上电稳定 → 输出开锁角
      if (now - srvT >= SERVO_POWER_SETTLE_MS) {
        servoWriteAngle(cfg.unlockAngle);
        srvState = SRV_UNLOCK;
        srvT = now;
      }
      break;
    case SRV_UNLOCK:                                 // 转到位
      if (now - srvT >= 400) { srvState = SRV_HOLD; srvT = now; }
      break;
    case SRV_HOLD:                                   // 保持开锁 → 回锁定角
      if (now - srvT >= cfg.unlockHoldMs) {
        servoWriteAngle(cfg.lockAngle);
        srvState = SRV_LOCK;
        srvT = now;
      }
      break;
    case SRV_LOCK:
      if (now - srvT >= 450) { servoDetachSignal(); srvState = SRV_SETTLE; srvT = now; }
      break;
    case SRV_SETTLE:                                 // 停稳 → 断电
      if (now - srvT >= 150) { servoPower(false); srvState = SRV_OFF; }
      break;
    case SRV_MANUAL_POWER:                            // Wait for stable power before manual PWM output
      if (now - srvT >= SERVO_POWER_SETTLE_MS) {
        servoWriteAngle(testAngle);
        srvState = SRV_MANUAL;
      }
      break;
    default:
      break;
  }
}

/* ============================================================================
 *  时间同步（需求三）
 * ============================================================================ */
static bool timeValid() { return time(nullptr) > 1600000000L; }   // 2020 年后

/* 有上行路由时走 NTP */
static void startTimeSync() {
  if (!cfg.uplinkSsid[0]) return;
  if (WiFi.status() != WL_CONNECTED) return;
  configTime(TZ_OFFSET_SEC, 0, "ntp.aliyun.com", "pool.ntp.org", "time.windows.com");
  sntpActive = true;
}

/* ============================================================================
 *  WiFi 策略（需求三、四）：调试模式 > 常开时段 > 有客户端 > 5 分钟开 1 分钟
 * ============================================================================ */
static bool inWindow(time_t t) {
  struct tm lt;
  localtime_r(&t, &lt);
  int m = lt.tm_hour * 60 + lt.tm_min;
  uint16_t s = cfg.winStart, e = cfg.winEnd;
  if (s == e) return false;                  // 起止相同 = 关闭时段
  if (s < e)  return m >= s && m < e;
  return m >= s || m < e;                    // 跨午夜时段
}

static void wifiStart() {
  WiFi.persistent(false);                    // 不把配置写进 Flash（额外磨损）
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.mode(cfg.uplinkSsid[0] ? WIFI_AP_STA : WIFI_AP);
  WiFi.softAP(apSsid.c_str(), AP_PASSWORD, AP_CHANNEL, 0, 4);
  if (cfg.uplinkSsid[0]) WiFi.begin(cfg.uplinkSsid, cfg.uplinkPass);
  server.begin();
  wifiOn = true;
}

static void wifiStop() {
  server.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  wifiOn = false;
}

/* 外界设备接入 AP → 触发时间同步 */
static void onClientJoined() {
  if (sntpActive) configTime(TZ_OFFSET_SEC, 0, "ntp.aliyun.com", "pool.ntp.org", "time.windows.com");
  // 无 NTP 时，新接入设备打开网页会通过 /api/sync 推送其本地时间完成同步
}

static void updateWifi() {
  uint32_t nowMs = millis();
  if ((uint32_t)(nowMs - lastWifiPolicyTick) < WIFI_POLICY_TICK_MS) return;
  lastWifiPolicyTick = nowMs;

  // Keep WiFi awake while a station is connected so admin requests are not interrupted.
  uint8_t stationCount = wifiOn ? WiFi.softAPgetStationNum() : 0;

  bool want = false;
  const char* reason = "duty";

  if (cfg.debugMode) {
    want = true; reason = "debug";                                    // 需求四：调试模式常开
  } else if (!timeValid()) {
    want = true; reason = "notime";                                   // 时钟未同步：默认按开启时段处理，等网页推送时间
  } else if (inWindow(time(nullptr))) {
    want = true; reason = "window";                                   // 需求三：时段内常开
  } else if (wifiOn && stationCount > 0) {
    want = true; reason = "client";                                   // 有设备在线时保持开启
  } else {
    reason = "duty";
    uint32_t e = dutyValid ? (millis() - dutyStart) : 0;              // 未进入过占空比 → 从周期起点算
    e %= WIFI_DUTY_PERIOD_MS;
    want = (e < WIFI_DUTY_ON_MS);                                     // 需求四：5 分钟开 1 分钟
  }

  wifiReason = reason;

  // 离开/进入占空比模式时重置周期，保证一进入就先开 1 分钟
  // （注意用完整字符串比较：reason 还可能是 "debug"，它同样以 'd' 开头）
  bool inDuty = (strcmp(reason, "duty") == 0);
  if (inDuty && !prevInDuty) { dutyStart = millis(); dutyValid = true; }
  prevInDuty = inDuty;

  if (want && !wifiOn) wifiStart();
  else if (!want && wifiOn) wifiStop();

  // 客户端数量上升 → 时间同步
  if (wifiOn) {
    if (stationCount > lastStations) onClientJoined();
    lastStations = stationCount;
  } else {
    lastStations = 0;
  }

  // 定期尝试建立上行连接并启动 NTP
  if (millis() - lastNetCheck > 2000) {
    lastNetCheck = millis();
    if (!sntpActive && cfg.uplinkSsid[0] && wifiOn) startTimeSync();
  }
}

static uint32_t wifiNextSec() {
  if (cfg.debugMode) return 0;
  if (!timeValid()) return 0;                      // 时钟未同步：常开，无倒计时
  if (inWindow(time(nullptr))) return 0;
  if (wifiOn && lastStations > 0) return 0;
  uint32_t e = dutyValid ? (millis() - dutyStart) : 0;
  e %= WIFI_DUTY_PERIOD_MS;
  if (e < WIFI_DUTY_ON_MS) return (WIFI_DUTY_ON_MS - e) / 1000UL;
  return (WIFI_DUTY_PERIOD_MS - e) / 1000UL;
}

/* ============================================================================
 *  JSON 辅助
 * ============================================================================ */
static String jsonStr(const String& b, const String& key) {
  String q = "\"" + key + "\"";
  int i = b.indexOf(q);
  if (i < 0) return "";
  i = b.indexOf(':', i + q.length());
  if (i < 0) return "";
  i++;
  while (i < (int)b.length() && (b[i] == ' ' || b[i] == '\t')) i++;
  if (i >= (int)b.length()) return "";
  if (b[i] == '"') {
    i++;
    String out;
    while (i < (int)b.length() && b[i] != '"') {
      if (b[i] == '\\' && i + 1 < (int)b.length()) {
        char c = b[++i];
        switch (c) {
          case 'n': out += '\n'; break;
          case 't': out += '\t'; break;
          default:  out += c;   break;      // \" \\ / 等
        }
        i++;
        continue;
      }
      out += b[i++];
    }
    return out;
  }
  int s = i;
  while (i < (int)b.length() && (isDigit(b[i]) || b[i] == '-' || b[i] == '+')) i++;
  return b.substring(s, i);
}

static long jsonNum(const String& b, const String& key) {
  String s = jsonStr(b, key);
  return s.length() ? s.toInt() : 0;
}

static void sendJson(int code, const String& s) {
  server.sendHeader(F("Cache-Control"), F("no-store"));
  server.sendHeader(F("X-Content-Type-Options"), F("nosniff"));
  server.send(code, "application/json; charset=utf-8", s);
}

static void sendOk() { sendJson(200, F("{\"ok\":1}")); }
static void sendFail(const String& msg) {
  String s = F("{\"ok\":0,\"msg\":\"");
  s += msg; s += F("\"}");
  sendJson(200, s);
}

static bool checkAuth(const String& b) {
  String t = jsonStr(b, "token");
  if (adminToken.length() == 0 || t != adminToken) return false;
  if ((int32_t)(tokenExpire - millis()) <= 0) return false;
  tokenExpire = millis() + TOKEN_VALID_MS;      // 滑动续期
  return true;
}

static String makeToken() {
  char buf[33];
  uint32_t mix = ESP.getCycleCount() ^ micros();      // 熵混合
  for (int i = 0; i < 16; i++) {
    uint32_t r;
#ifdef RANDOM_REG32
    r = RANDOM_REG32 ^ mix;
#else
    r = mix ^ (uint32_t)millis() * 2654435761UL;
#endif
    mix = r * 1664525UL + 1013904223UL;
    snprintf(buf + i * 2, 3, "%02x", (unsigned)((r >> ((i & 3) * 8)) & 0xFF));
  }
  buf[32] = '\0';
  return String(buf);
}

/* ============================================================================
 *  API 处理
 * ============================================================================ */
static String statusJson() {
  time_t t = time(nullptr);
  bool synced = timeValid();
  String s;
  s.reserve(320);
  s = F("{\"wifiOn\":");
  s += wifiOn ? 1 : 0;
  s += F(",\"reason\":\""); s += wifiReason; s += '"';
  s += F(",\"wifiNext\":"); s += wifiNextSec();
  s += F(",\"stations\":"); s += wifiOn ? lastStations : 0;
  s += F(",\"time\":"); s += (uint32_t)t;
  s += F(",\"synced\":"); s += synced ? 1 : 0;
  s += F(",\"inWindow\":"); s += synced ? (inWindow(t) ? 1 : 0) : 1;   // 未同步时默认视为在开启时段
  s += F(",\"debug\":"); s += cfg.debugMode ? 1 : 0;
  s += F(",\"lock\":\""); s += lockStateStr(); s += '"';
  s += F(",\"pwr\":"); s += pwrOn ? 1 : 0;
  s += F(",\"apSsid\":\""); s += apSsid; s += '"';
  s += F(",\"seq\":"); s += store.seq();
  s += F(",\"writes\":"); s += store.writes();
  s += F(",\"erases\":"); s += store.erases();
  s += F("}");
  return s;
}

static void handleStatus() { sendJson(200, statusJson()); }

/* 时间推送：网页加载时上报浏览器本地时间（无外网时的兜底同步源） */
static void handleSync() {
  String b = server.arg("plain");
  if (!sntpActive) {
    long ep = jsonNum(b, "epoch");
    if (ep > 1600000000L) {
      struct timeval tv = { (time_t)ep, 0 };
      settimeofday(&tv, nullptr);
    }
  }
  sendOk();
}

static bool isSixDigits(const String& s) {
  if (s.length() != 6) return false;
  for (int i = 0; i < 6; i++) if (!isDigit(s[i])) return false;
  return true;
}

static void handleUnlock() {
  String b = server.arg("plain");
  String code = jsonStr(b, "code");
  if (!isSixDigits(code)) { sendFail("请输入 6 位数字密码"); return; }

  uint32_t v = strtoul(code.c_str(), nullptr, 10) + 1;   // 存储时 +1，0 保留为空槽
  bool hit = false;
  for (int i = 0; i < LOCK_MAX_CODES; i++) {
    if (cfg.codes[i] != 0 && cfg.codes[i] == v) { hit = true; break; }
  }
  if (!hit) { sendFail("密码错误"); return; }
  if (!requestUnlock()) { sendFail("门锁动作进行中，请稍候"); return; }
  sendJson(200, F("{\"ok\":1,\"msg\":\"密码正确，正在开锁\"}"));
}

static void handleLogin() {
  String b = server.arg("plain");
  String key = jsonStr(b, "key");
  if (key.length() < 4 || key != String(cfg.adminKey)) { sendFail("管理员密钥错误"); return; }
  adminToken = makeToken();
  tokenExpire = millis() + TOKEN_VALID_MS;
  String s = F("{\"ok\":1,\"token\":\""); s += adminToken; s += F("\"}");
  sendJson(200, s);
}

static void handleConfig() {
  String b = server.arg("plain");
  if (!checkAuth(b)) {
    sendJson(200, F("{\"ok\":0,\"needLogin\":1,\"msg\":\"未登录或会话过期\"}"));
    return;
  }

  String codes = "[";
  for (int i = 0; i < LOCK_MAX_CODES; i++) {
    if (i) codes += ',';
    if (cfg.codes[i]) {
      char t[8];
      snprintf(t, sizeof(t), "%06lu", (unsigned long)(cfg.codes[i] - 1));
      codes += '"'; codes += t; codes += '"';
    } else {
      codes += "\"\"";
    }
  }
  codes += ']';

  char hhmm1[6], hhmm2[6];
  snprintf(hhmm1, sizeof(hhmm1), "%02d:%02d", cfg.winStart / 60, cfg.winStart % 60);
  snprintf(hhmm2, sizeof(hhmm2), "%02d:%02d", cfg.winEnd / 60, cfg.winEnd % 60);

  String s = F("{\"ok\":1,\"codes\":");
  s += codes;
  s += F(",\"winStart\":\""); s += hhmm1; s += '"';
  s += F(",\"winEnd\":\""); s += hhmm2; s += '"';
  s += F(",\"debug\":"); s += cfg.debugMode ? 1 : 0;
  s += F(",\"uplinkSsid\":\""); s += cfg.uplinkSsid; s += '"';
  s += F(",\"uplinkHasPass\":"); s += cfg.uplinkPass[0] ? 1 : 0;
  s += F(",\"lockAngle\":"); s += cfg.lockAngle;
  s += F(",\"unlockAngle\":"); s += cfg.unlockAngle;
  s += F(",\"holdMs\":"); s += cfg.unlockHoldMs;
  s += F("}");
  sendJson(200, s);
}

/* 解析 "codes":["123456","",...] 数组 */
static bool parseCodes(const String& b, uint32_t* out) {
  int i = b.indexOf("\"codes\"");
  if (i < 0) return false;
  i = b.indexOf('[', i);
  if (i < 0) return false;
  i++;
  int n = 0;
  while (n < LOCK_MAX_CODES && i < (int)b.length()) {
    while (i < (int)b.length() && (isSpace(b[i]) || b[i] == ',')) i++;
    if (i >= (int)b.length() || b[i] == ']') break;
    int s = i;
    if (b[i] == '"') {
      s = ++i;
      while (i < (int)b.length() && b[i] != '"') i++;
    } else {
      while (i < (int)b.length() && b[i] != ',' && b[i] != ']') i++;
    }
    String tok = b.substring(s, i);
    tok.trim();
    if (tok.length() == 0) { out[n++] = 0; continue; }
    if (!isSixDigits(tok)) return false;
    out[n++] = strtoul(tok.c_str(), nullptr, 10) + 1;   // 与解锁侧保持同一编码
    if (i < (int)b.length() && b[i] == '"') i++;
  }
  while (n < LOCK_MAX_CODES) out[n++] = 0;
  return true;
}

static void handleSaveCodes() {
  String b = server.arg("plain");
  if (!checkAuth(b)) { sendFail("Session expired"); return; }
  uint32_t parsed[LOCK_MAX_CODES];
  memset(parsed, 0, sizeof(parsed));
  if (!parseCodes(b, parsed)) { sendFail("Each code must contain exactly 6 digits"); return; }

  uint32_t previous[LOCK_MAX_CODES];
  memcpy(previous, cfg.codes, sizeof(previous));
  memcpy(cfg.codes, parsed, sizeof(cfg.codes));
  if (!saveConfigNow()) {
    memcpy(cfg.codes, previous, sizeof(cfg.codes));
    sendFail("Flash save failed");
    return;
  }

  String codes = "[";
  for (int i = 0; i < LOCK_MAX_CODES; i++) {
    if (i) codes += ',';
    if (cfg.codes[i]) {
      char t[8];
      snprintf(t, sizeof(t), "%06lu", (unsigned long)(cfg.codes[i] - 1));
      codes += '"'; codes += t; codes += '"';
    } else codes += "\"\"";
  }
  codes += ']';
  String out = F("{\"ok\":1,\"codes\":"); out += codes; out += F("}");
  sendJson(200, out);
}

static void handleSaveCode() {
  String b = server.arg("plain");
  if (!checkAuth(b)) { sendFail("Session expired"); return; }

  int index = (int)jsonNum(b, "index");
  String code = jsonStr(b, "code");
  if (index < 0 || index >= LOCK_MAX_CODES) { sendFail("Invalid code index"); return; }
  if (code.length() && !isSixDigits(code)) { sendFail("Code must contain exactly 6 digits"); return; }

  uint32_t previous = cfg.codes[index];
  cfg.codes[index] = code.length() ? strtoul(code.c_str(), nullptr, 10) + 1 : 0;
  if (!saveConfigNow()) {
    cfg.codes[index] = previous;
    sendFail("Flash save failed");
    return;
  }

  String out = F("{\"ok\":1,\"index\":");
  out += index;
  out += F(",\"code\":\"");
  out += code;
  out += F("\"}");
  sendJson(200, out);
}


static int toMin(const String& hm) {
  int c = hm.indexOf(':');
  if (c < 0) return -1;
  int h = hm.substring(0, c).toInt();
  int m = hm.substring(c + 1).toInt();
  if (h < 0 || h > 23 || m < 0 || m > 59) return -1;
  return h * 60 + m;
}

static void handleSaveNet() {
  String b = server.arg("plain");
  if (!checkAuth(b)) { sendFail("未登录或会话过期"); return; }

  int ws = toMin(jsonStr(b, "winStart"));
  int we = toMin(jsonStr(b, "winEnd"));
  if (ws < 0 || we < 0) { sendFail("时段格式应为 HH:MM"); return; }

  String ssid = jsonStr(b, "uplinkSsid");
  String pass = jsonStr(b, "uplinkPass");
  if (ssid.length() > 32) { sendFail("SSID 过长"); return; }
  if (pass.length() > 64) { sendFail("密码过长"); return; }

  LockConfig previous = cfg;
  cfg.winStart = (uint16_t)ws;
  cfg.winEnd = (uint16_t)we;
  cfg.debugMode = jsonNum(b, "debug") ? 1 : 0;

  memset(cfg.uplinkSsid, 0, sizeof(cfg.uplinkSsid));
  strncpy(cfg.uplinkSsid, ssid.c_str(), sizeof(cfg.uplinkSsid) - 1);
  if (pass.length()) {                          // 留空 = 保持原密码
    memset(cfg.uplinkPass, 0, sizeof(cfg.uplinkPass));
    strncpy(cfg.uplinkPass, pass.c_str(), sizeof(cfg.uplinkPass) - 1);
  } else if (!ssid.length()) {
    memset(cfg.uplinkPass, 0, sizeof(cfg.uplinkPass));
  }

  if (!saveConfigNow()) {
    cfg = previous;
    sendFail("Flash save failed; settings were not changed");
    return;
  }
  sntpActive = false;                           // Retry uplink and NTP with new settings.
  if (wifiOn) {                                 // 按新 SSID 切换 AP / AP+STA
    WiFi.mode(cfg.uplinkSsid[0] ? WIFI_AP_STA : WIFI_AP);
    if (cfg.uplinkSsid[0]) WiFi.begin(cfg.uplinkSsid, cfg.uplinkPass);
  }
  sendOk();
}

static void handleSaveAngles() {
  String b = server.arg("plain");
  if (!checkAuth(b)) { sendFail("未登录或会话过期"); return; }
  int la = jsonNum(b, "lockAngle");
  int ua = jsonNum(b, "unlockAngle");
  int h = jsonNum(b, "holdMs");
  if (la < 0 || la > 180 || ua < 0 || ua > 180) { sendFail("角度需在 0~180"); return; }
  if (h < 500 || h > 10000) { sendFail("保持时间需在 500~10000 ms"); return; }
  uint16_t oldLockAngle = cfg.lockAngle;
  uint16_t oldUnlockAngle = cfg.unlockAngle;
  uint16_t oldHoldMs = cfg.unlockHoldMs;
  cfg.lockAngle = la;
  cfg.unlockAngle = ua;
  cfg.unlockHoldMs = h;
  if (!saveConfigNow()) {
    cfg.lockAngle = oldLockAngle;
    cfg.unlockAngle = oldUnlockAngle;
    cfg.unlockHoldMs = oldHoldMs;
    sendFail("Flash save failed; motion settings were not changed");
    return;
  }
  sendOk();
}

static void handleChangeKey() {
  String b = server.arg("plain");
  if (!checkAuth(b)) { sendFail("未登录或会话过期"); return; }
  String oldK = jsonStr(b, "oldKey");
  String newK = jsonStr(b, "newKey");
  if (oldK != String(cfg.adminKey)) { sendFail("当前密钥不正确"); return; }
  if (newK.length() < 4 || newK.length() > 20) { sendFail("新密钥需 4~20 位"); return; }

  char previousKey[sizeof(cfg.adminKey)];
  memcpy(previousKey, cfg.adminKey, sizeof(previousKey));
  memset(cfg.adminKey, 0, sizeof(cfg.adminKey));
  strncpy(cfg.adminKey, newK.c_str(), sizeof(cfg.adminKey) - 1);
  if (!saveConfigNow()) {
    memcpy(cfg.adminKey, previousKey, sizeof(cfg.adminKey));
    sendFail("Flash save failed; admin key was not changed");
    return;
  }

  adminToken = makeToken();                     // 换密钥后刷新会话
  tokenExpire = millis() + TOKEN_VALID_MS;
  String s = F("{\"ok\":1,\"token\":\""); s += adminToken; s += F("\"}");
  sendJson(200, s);
}

static void handleServo() {
  String b = server.arg("plain");
  if (!checkAuth(b)) { sendFail("Session expired"); return; }
  String cmd = jsonStr(b, "cmd");

  if (cmd == "power_on") {
    if (srvState != SRV_OFF && srvState != SRV_MANUAL && srvState != SRV_MANUAL_POWER) {
      sendFail("Servo is busy"); return;
    }
    if (srvState == SRV_OFF) {
      servoPower(true);
      srvState = SRV_MANUAL_POWER;
      srvT = millis();
    }
    sendOk();
  } else if (cmd == "power_off") {
    if (srvState == SRV_MANUAL || srvState == SRV_MANUAL_POWER || srvState == SRV_OFF) {
      servoDetachSignal();
      servoPower(false);
      srvState = SRV_OFF;
      sendOk();
    } else {
      sendFail("Cannot power off during automatic motion");
    }
  } else if (cmd == "angle") {
    int a = jsonNum(b, "value");
    if (a < 0 || a > 180) { sendFail("Angle must be 0..180"); return; }
    if (srvState != SRV_MANUAL && srvState != SRV_MANUAL_POWER && srvState != SRV_OFF) {
      sendFail("Servo is busy with automatic motion"); return;
    }
    testAngle = (uint16_t)a;
    if (srvState == SRV_OFF) {
      servoPower(true);
      srvState = SRV_MANUAL_POWER;
      srvT = millis();
    } else if (srvState == SRV_MANUAL) {
      servoWriteAngle(testAngle);
    }
    sendOk();
  } else {
    sendFail("Unknown servo command");
  }
}


static void handleTestUnlock() {
  String b = server.arg("plain");
  if (!checkAuth(b)) { sendFail("未登录或会话过期"); return; }
  if (srvState == SRV_MANUAL || srvState == SRV_MANUAL_POWER) {
    servoDetachSignal();
    servoPower(false);
    srvState = SRV_OFF;
  }
  if (!requestUnlock()) { sendFail("门锁动作进行中"); return; }
  sendOk();
}

/* ============================================================================
 *  静态页面
 * ============================================================================ */
/* HTML 不缓存；CSS/JS 使用版本化 URL 长缓存。
   用户端进入管理员端时只传输较小的 HTML，样式与脚本复用浏览器缓存。 */
static void handleIndex() {
  server.sendHeader(F("Cache-Control"), F("no-store"));
  server.send_P(200, "text/html; charset=utf-8", PAGE_INDEX);
}
static void handleAdmin() {
  server.sendHeader(F("Cache-Control"), F("no-store"));
  server.send_P(200, "text/html; charset=utf-8", PAGE_ADMIN);
}
static void handleStyle() {
  server.sendHeader(F("Cache-Control"), F("public, max-age=86400, immutable"));
  server.send_P(200, "text/css; charset=utf-8", PAGE_CSS);
}
static void handleScript() {
  server.sendHeader(F("Cache-Control"), F("public, max-age=86400, immutable"));
  server.send_P(200, "application/javascript; charset=utf-8", PAGE_JS);
}

static void handleNotFound() {
  String uri = server.uri();
  if (uri.startsWith("/api/")) {
    sendJson(404, F("{\"ok\":0,\"msg\":\"not found\"}"));
    return;
  }
  // 兼容手机系统联网检测/强制门户路径，统一回到用户首页。
  server.sendHeader(F("Location"), F("/"), true);
  server.sendHeader(F("Cache-Control"), F("no-store"));
  server.send(302, "text/plain", "");
}

static void setupRoutes() {
  server.on("/", HTTP_GET, handleIndex);
  server.on("/index.html", HTTP_GET, handleIndex);
  server.on("/admin", HTTP_GET, handleAdmin);
  server.on("/admin/", HTTP_GET, handleAdmin);
  server.on("/admin.html", HTTP_GET, handleAdmin);
  server.on("/style.css", HTTP_GET, handleStyle);
  server.on("/app.js", HTTP_GET, handleScript);

  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/sync", HTTP_POST, handleSync);
  server.on("/api/unlock", HTTP_POST, handleUnlock);
  server.on("/api/admin/login", HTTP_POST, handleLogin);
  server.on("/api/config", HTTP_POST, handleConfig);
  server.on("/api/saveCodes", HTTP_POST, handleSaveCodes);
  server.on("/api/saveCode", HTTP_POST, handleSaveCode);
  server.on("/api/saveNet", HTTP_POST, handleSaveNet);
  server.on("/api/saveAngles", HTTP_POST, handleSaveAngles);
  server.on("/api/changeKey", HTTP_POST, handleChangeKey);
  server.on("/api/servo", HTTP_POST, handleServo);
  server.on("/api/testUnlock", HTTP_POST, handleTestUnlock);

  server.onNotFound(handleNotFound);
}

/* ============================================================================
 *  setup / loop
 * ============================================================================ */
void setup() {
#if ENABLE_SERIAL
  Serial.begin(115200);
  delay(50);
  Serial.println();
  Serial.println(F("[lock] boot"));
#endif

  pinMode(PIN_SERVO_PWR, OUTPUT);
  digitalWrite(PIN_SERVO_PWR, !SERVO_PWR_ACTIVE_LEVEL);  // Servo power off by default
  pinMode(PIN_SERVO_SIG, INPUT);                         // No PWM output by default

  store.begin();
  loadConfig();

  apSsid = "LockGate-" + String(ESP.getChipId(), HEX);

  WiFi.persistent(false);
  WiFi.mode(WIFI_OFF);

  dutyStart = millis();
  dutyValid = false;
  prevInDuty = false;

  setupRoutes();

#if ENABLE_SERIAL
  Serial.printf("[lock] AP=%s store=%s seq=%lu\n",
                apSsid.c_str(), store.ok() ? "ok" : "FAIL",
                (unsigned long)store.seq());
#endif
}

void loop() {
  // 活跃连接优先，避免策略计算影响网页与 API 响应。
  if (wifiOn) server.handleClient();
  updateWifi();
  servoTick();
  yield();
}
