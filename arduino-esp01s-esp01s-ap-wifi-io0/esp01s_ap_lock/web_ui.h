#pragma once
#include <pgmspace.h>

/*
 * ==========================================================================
 * ESP-01S 门锁网页资源：用户端、管理员端、公共 CSS 与脚本。
 * HTML 不缓存；CSS/JS 通过版本化绝对路径独立缓存。
 * ==========================================================================
 */

const char PAGE_INDEX[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<title>智能门锁</title>
<link rel="icon" href="data:,">
<link rel="stylesheet" href="/style.css?v=20261003c">
</head>
<body>
<div class="wrap">
  <header class="topbar">
    <div class="brand">
      <div class="brand-mark">
        <svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round">
          <rect x="4" y="10.5" width="16" height="10.5" rx="2.5"></rect>
          <path id="shackle" d="M8 10.5V7.5a4 4 0 0 1 8 0v3"></path>
          <circle cx="12" cy="15.6" r="1.6"></circle>
        </svg>
      </div>
      <div>
        <div class="title">智能门锁</div>
        <div class="sub">ESP-01S · AP 网关</div>
      </div>
    </div>
    <div class="chips">
      <span class="chip" id="chipWifi">WiFi --</span>
      <span class="chip blue mono" id="chipTime">--:--</span>
    </div>
  </header>

  <section class="card lock-card">
    <div class="lock-state">
      <div class="lock-ring" id="lockRing">
        <svg width="34" height="34" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round">
          <rect x="4" y="10.5" width="16" height="10.5" rx="2.5"></rect>
          <path d="M8 10.5V7.5a4 4 0 0 1 8 0v3"></path>
          <circle cx="12" cy="15.6" r="1.6"></circle>
        </svg>
      </div>
      <div class="lock-name" id="lockName">已上锁</div>
    </div>

    <div class="display" id="display"><span class="ph">请输入 6 位开锁密码</span></div>
    <div class="msg" id="msg"></div>

    <div class="keypad" id="keypad">
      <button class="key" data-k="1">1</button>
      <button class="key" data-k="2">2</button>
      <button class="key" data-k="3">3</button>
      <button class="key" data-k="4">4</button>
      <button class="key" data-k="5">5</button>
      <button class="key" data-k="6">6</button>
      <button class="key" data-k="7">7</button>
      <button class="key" data-k="8">8</button>
      <button class="key" data-k="9">9</button>
      <button class="key fn" data-k="C">清空</button>
      <button class="key" data-k="0">0</button>
      <button class="key fn" data-k="B">退格</button>
    </div>

    <button class="btn-primary" id="btnUnlock">开 门</button>
  </section>

  <footer class="foot">
    <a href="/admin">管理员入口</a>
    <span class="tag-preview" id="previewTag" hidden>预览模式</span>
  </footer>
</div>
<div class="toast" id="toast"></div>
<script src="/app.js?v=20261003c" defer></script>
</body>
</html>
)rawliteral";

const char PAGE_ADMIN[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<title>智能门锁 · 管理员</title>
<link rel="icon" href="data:,">
<link rel="stylesheet" href="/style.css?v=20261003c">
</head>
<body>
<div class="wrap admin-wrap">
  <header class="topbar">
    <div class="brand">
      <div class="brand-mark">
        <svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round">
          <path d="M12 3l7.5 3.2v5.1c0 4.6-3.1 8.5-7.5 9.7-4.4-1.2-7.5-5.1-7.5-9.7V6.2L12 3z"></path>
          <path d="M9.2 12.2l1.9 1.9 3.7-3.9"></path>
        </svg>
      </div>
      <div>
        <div class="title">管理后台</div>
        <div class="sub">密码 / 网络时间 / 门锁 / 安全</div>
      </div>
    </div>
    <div class="chips">
      <span class="chip" id="chipWifi">WiFi --</span>
      <span class="chip blue mono" id="chipTime">--:--</span>
      <span class="chip" id="chipSession" hidden>已登录</span>
    </div>
  </header>

  <!-- 登录 -->
  <div class="card login-card" id="adminLogin">
    <h2>管理员登录</h2>
    <p class="hint">请输入管理员密钥以进入后台（默认 888888，登录后请及时修改）</p>
    <div class="field">
      <input class="input" id="inKey" type="password" inputmode="numeric" maxlength="20" placeholder="管理员密钥">
    </div>
    <button class="btn-primary" id="btnLogin" style="height:50px;font-size:16px;letter-spacing:3px">进入后台</button>
    <p class="hint" style="margin-top:16px"><a href="/" style="color:var(--mut)">← 返回用户端</a></p>
  </div>

  <!-- 后台面板 -->
  <div id="adminPanel" hidden>
    <nav class="tabs">
      <button class="tab active" data-tab="codes">密码管理</button>
      <button class="tab" data-tab="net">网络与时间</button>
      <button class="tab" data-tab="lock">门锁控制</button>
      <button class="tab" data-tab="safe">安全设置</button>
    </nav>

    <!-- 密码管理 -->
    <section class="tabpane active" id="tab-codes">
      <div class="card">
        <div class="card-hd">
          <h3>开锁密码</h3>
          <span class="hint">最多 10 组；每组可单独保存，留空即删除</span>
        </div>
        <div class="code-grid" id="codeList"></div>
      </div>
    </section>

    <!-- 网络与时间 -->
    <section class="tabpane" id="tab-net">
      <div class="card">
        <div class="card-hd"><h3>系统时间</h3><span class="hint">设备连接 WiFi 时自动同步；也可手动推送本机时间</span></div>
        <div class="stat-grid">
          <div class="stat"><div class="k">当前时间</div><div class="v mono" id="stTime">--</div></div>
          <div class="stat"><div class="k">同步状态</div><div class="v small" id="stSync">未同步</div></div>
          <div class="stat"><div class="k">在线设备</div><div class="v" id="stSta">0</div></div>
          <div class="stat"><div class="k">AP 名称</div><div class="v small" id="stAp">--</div></div>
        </div>
        <div style="margin-top:14px;display:flex;gap:10px;flex-wrap:wrap">
          <button class="btn small" id="btnSyncNow">立即同步时间</button>
          <button class="btn small ghost" id="btnLogout">退出登录</button>
        </div>
      </div>

      <div class="card">
        <div class="card-hd"><h3>WiFi 常开时段</h3><span class="hint">该时段内 WiFi 常态开启；其余时间每 5 分钟开启 1 分钟</span></div>
        <div class="field-row">
          <div class="field"><label>开始时间</label><input class="input" type="time" id="winStart" value="08:00"></div>
          <div class="field"><label>结束时间</label><input class="input" type="time" id="winEnd" value="22:00"></div>
        </div>
        <div class="row-line">
          <div><div class="lab">调试模式</div><div class="desc">开启后 WiFi 常态开启，忽略时段与省电占空比</div></div>
          <label class="switch"><input type="checkbox" id="dbgMode"><span></span></label>
        </div>
        <div class="field" style="margin-top:14px">
          <label>上行 WiFi（可选，用于 NTP 时间同步）</label>
          <div class="field-row">
            <input class="input" id="upSsid" maxlength="32" placeholder="路由器 SSID" style="flex:2">
            <input class="input" id="upPass" type="password" maxlength="64" placeholder="密码（留空则不修改）" style="flex:2">
          </div>
        </div>
        <button class="btn" id="btnSaveNet">保存网络与时段设置</button>
      </div>
    </section>

    <!-- 门锁控制 -->
    <section class="tabpane" id="tab-lock">
      <div class="card">
        <div class="card-hd"><h3>开锁动作</h3><span class="hint">IO0 置高供电 → IO1 输出 PWM → 归位 → IO0 断电</span></div>
        <div class="stat-grid" style="margin-bottom:14px">
          <div class="stat"><div class="k">门锁状态</div><div class="v small" id="stLock">--</div></div>
          <div class="stat"><div class="k">供电 (IO0)</div><div class="v small" id="stPwr">--</div></div>
        </div>
        <div style="display:flex;gap:10px;flex-wrap:wrap">
          <button class="btn" id="btnTestUnlock">执行开门动作</button>
          <button class="btn" id="btnPwrOn">IO0 置高（供电）</button>
          <button class="btn danger" id="btnPwrOff">IO0 置低（断电）</button>
        </div>
      </div>

      <div class="card">
        <div class="card-hd"><h3>舵机角度测试</h3><span class="hint">需先给舵机上电，再输出角度</span></div>
        <div class="field">
          <label>角度：<span class="mono" id="angleVal">90</span>°</label>
          <input class="slider" type="range" id="angle" min="0" max="180" value="90">
        </div>
        <button class="btn" id="btnAngle">输出角度到舵机</button>
      </div>

      <div class="card">
        <div class="card-hd"><h3>动作参数</h3><span class="hint">影响开锁动作的 PWM 角度与保持时间</span></div>
        <div class="field-row">
          <div class="field"><label>锁定角度 (°)</label><input class="input" type="number" id="lockAngle" min="0" max="180" value="0"></div>
          <div class="field"><label>开锁角度 (°)</label><input class="input" type="number" id="unlockAngle" min="0" max="180" value="90"></div>
        </div>
        <div class="field"><label>开锁保持时间 (毫秒)</label><input class="input" type="number" id="holdMs" min="500" max="10000" step="100" value="2500"></div>
        <button class="btn" id="btnSaveAngles">保存动作参数</button>
      </div>
    </section>

    <!-- 安全设置 -->
    <section class="tabpane" id="tab-safe">
      <div class="card">
        <div class="card-hd"><h3>修改管理员密钥</h3><span class="hint">用于进入本管理后台</span></div>
        <div class="field"><label>当前密钥</label><input class="input" id="oldKey" type="password" maxlength="20" placeholder="当前管理员密钥"></div>
        <div class="field-row">
          <div class="field"><label>新密钥</label><input class="input" id="newKey" type="password" maxlength="20" placeholder="4~20 位"></div>
          <div class="field"><label>确认新密钥</label><input class="input" id="newKey2" type="password" maxlength="20" placeholder="重复输入"></div>
        </div>
        <button class="btn" id="btnChangeKey">确认修改</button>
      </div>

      <div class="card">
        <div class="card-hd"><h3>存储健康（Flash 均匀磨损）</h3><span class="hint">日志式轮转写入：64 槽位 / 4 扇区，写满一轮才擦除一次</span></div>
        <div class="stat-grid">
          <div class="stat"><div class="k">记录序号</div><div class="v mono" id="stSeq">--</div></div>
          <div class="stat"><div class="k">累计写入</div><div class="v mono" id="stWr">--</div></div>
          <div class="stat"><div class="k">累计擦除</div><div class="v mono" id="stEr">--</div></div>
          <div class="stat"><div class="k">理论寿命提升</div><div class="v small">32× （单扇区轮转）</div></div>
        </div>
      </div>
    </section>

    <div class="statusbar" id="adminStatus">状态加载中…</div>
  </div>
</div>
<div class="toast" id="toast"></div>
<script src="/app.js?v=20261003c" defer></script>
</body>
</html>
)rawliteral";

const char PAGE_CSS[] PROGMEM = R"rawliteral(
/* 智能门锁 · ESP-01S AP 网关 —— 用户端 / 管理员端 共用样式 */
* { box-sizing: border-box; margin: 0; padding: 0; -webkit-tap-highlight-color: transparent; }
[hidden] { display: none !important; }

:root {
  --bg: #0B1220;
  --panel: #121B2E;
  --panel-2: #0F172A;
  --line: #1E2A44;
  --ink: #E6EDF7;
  --mut: #8FA1BF;
  --acc: #34D399;
  --acc-deep: #059669;
  --blue: #38BDF8;
  --warn: #FBBF24;
  --danger: #F87171;
  --radius: 16px;
}

html, body { height: 100%; }
body {
  font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", "PingFang SC", "Microsoft YaHei", system-ui, sans-serif;
  background:
    radial-gradient(1100px 560px at 85% -12%, rgba(56, 189, 248, .16) 0%, transparent 60%),
    radial-gradient(900px 520px at -12% 112%, rgba(52, 211, 153, .14) 0%, transparent 58%),
    var(--bg);
  color: var(--ink);
  min-height: 100%;
  -webkit-font-smoothing: antialiased;
}

.wrap { max-width: 440px; margin: 0 auto; padding: 26px 20px 44px; }
.wrap.admin-wrap { max-width: 960px; }

/* ---------- 顶栏 ---------- */
.topbar { display: flex; align-items: center; justify-content: space-between; gap: 12px; margin-bottom: 22px; }
.brand { display: flex; align-items: center; gap: 12px; min-width: 0; }
.brand-mark {
  width: 42px; height: 42px; border-radius: 13px; flex: none;
  background: linear-gradient(140deg, rgba(52, 211, 153, .22), rgba(56, 189, 248, .16));
  border: 1px solid rgba(52, 211, 153, .38);
  display: grid; place-items: center;
  color: var(--acc);
}
.brand .title { font-size: 17px; font-weight: 700; letter-spacing: .5px; }
.brand .sub { font-size: 11px; color: var(--mut); margin-top: 2px; letter-spacing: .4px; }

.chips { display: flex; gap: 8px; flex-wrap: wrap; justify-content: flex-end; }
.chip {
  font-size: 11.5px; padding: 5px 10px; border-radius: 999px;
  background: rgba(148, 163, 184, .08);
  border: 1px solid var(--line); color: var(--mut);
  display: inline-flex; align-items: center; gap: 6px; white-space: nowrap;
}
.chip::before { content: ""; width: 6px; height: 6px; border-radius: 50%; background: var(--mut); }
.chip.on { color: #A7F3D0; border-color: rgba(52, 211, 153, .4); background: rgba(52, 211, 153, .10); }
.chip.on::before { background: var(--acc); box-shadow: 0 0 8px rgba(52, 211, 153, .9); }
.chip.blue { color: #BAE6FD; border-color: rgba(56, 189, 248, .4); background: rgba(56, 189, 248, .10); }
.chip.blue::before { background: var(--blue); }
.chip.amber { color: #FDE68A; border-color: rgba(251, 191, 36, .4); background: rgba(251, 191, 36, .10); }
.chip.amber::before { background: var(--warn); }

/* ---------- 卡片 ---------- */
.card {
  background: linear-gradient(180deg, rgba(18, 27, 46, .96), rgba(15, 22, 40, .96));
  border: 1px solid var(--line);
  border-radius: var(--radius);
  padding: 20px;
  box-shadow: 0 18px 40px rgba(2, 6, 23, .45);
}
.card + .card { margin-top: 16px; }
.card-hd { display: flex; align-items: center; gap: 10px; flex-wrap: wrap; margin-bottom: 14px; }
.card-hd h3 { font-size: 15px; font-weight: 700; }
.card-hd .hint { font-size: 12px; color: var(--mut); flex: 1; min-width: 140px; }

.hint { font-size: 12px; color: var(--mut); line-height: 1.7; }
.mono { font-variant-numeric: tabular-nums; font-family: "SFMono-Regular", Consolas, "Liberation Mono", monospace; }

/* ---------- 用户端 ---------- */
.lock-card { text-align: center; padding-top: 26px; }
.lock-state { display: flex; flex-direction: column; align-items: center; gap: 8px; margin-bottom: 20px; }
.lock-ring {
  width: 76px; height: 76px; border-radius: 50%;
  border: 1px solid rgba(52, 211, 153, .35);
  background: radial-gradient(circle at 50% 35%, rgba(52, 211, 153, .20), rgba(52, 211, 153, .03) 70%);
  display: grid; place-items: center; color: var(--acc);
  transition: all .3s ease;
}
.lock-ring.busy { border-color: rgba(251, 191, 36, .5); color: var(--warn); background: radial-gradient(circle at 50% 35%, rgba(251, 191, 36, .18), transparent 70%); animation: pulse 1s infinite; }
.lock-ring.open { border-color: rgba(56, 189, 248, .55); color: var(--blue); background: radial-gradient(circle at 50% 35%, rgba(56, 189, 248, .20), transparent 70%); }
@keyframes pulse { 50% { transform: scale(1.05); } }
.lock-name { font-size: 15px; font-weight: 600; letter-spacing: 2px; }

.display {
  min-height: 62px; border-radius: 14px;
  background: rgba(2, 6, 23, .55);
  border: 1px solid var(--line);
  display: flex; align-items: center; justify-content: center; gap: 10px;
  margin-bottom: 14px; padding: 8px 14px; overflow: hidden;
}
.display .ph { color: #5C6E8C; font-size: 14px; letter-spacing: 1px; }
.display .dot {
  width: 34px; height: 44px; border-radius: 9px;
  background: rgba(52, 211, 153, .10);
  border: 1px solid rgba(52, 211, 153, .30);
  color: var(--acc); font-size: 24px; font-weight: 700;
  display: grid; place-items: center;
  font-variant-numeric: tabular-nums;
}
.display.shake { animation: shake .38s; border-color: rgba(248, 113, 113, .6); }
@keyframes shake { 0%,100%{transform:translateX(0)} 25%{transform:translateX(-7px)} 50%{transform:translateX(6px)} 75%{transform:translateX(-4px)} }

.msg { min-height: 20px; font-size: 13px; color: var(--mut); margin-bottom: 12px; }
.msg.ok { color: var(--acc); }
.msg.err { color: var(--danger); }

.keypad { display: grid; grid-template-columns: repeat(3, 1fr); gap: 10px; margin-bottom: 16px; }
.key {
  height: 56px; border-radius: 13px; border: 1px solid var(--line);
  background: rgba(30, 42, 68, .35); color: var(--ink);
  font-size: 21px; font-weight: 600; cursor: pointer;
  transition: transform .08s ease, background .15s ease, border-color .15s ease;
  font-variant-numeric: tabular-nums;
}
.key:hover { background: rgba(56, 189, 248, .10); border-color: rgba(56, 189, 248, .35); }
.key:active { transform: scale(.95); }
.key.fn { font-size: 15px; color: var(--mut); }

.btn {
  border: 1px solid var(--line); background: rgba(30, 42, 68, .4);
  color: var(--ink); border-radius: 11px; padding: 10px 16px;
  font-size: 14px; cursor: pointer; transition: all .15s ease; font-weight: 600;
}
.btn:hover { border-color: rgba(56, 189, 248, .45); background: rgba(56, 189, 248, .10); }
.btn:disabled { opacity: .5; cursor: not-allowed; }
.btn.small { padding: 7px 12px; font-size: 12.5px; border-radius: 9px; }
.btn.danger:hover { border-color: rgba(248, 113, 113, .5); background: rgba(248, 113, 113, .12); color: #FCA5A5; }
.btn.ghost { background: transparent; }

.btn-primary {
  width: 100%; height: 58px; border: none; border-radius: 14px; cursor: pointer;
  background: linear-gradient(135deg, var(--acc) 0%, #10B981 55%, #059669 100%);
  color: #04140D; font-size: 18px; font-weight: 800; letter-spacing: 6px;
  box-shadow: 0 12px 28px rgba(16, 185, 129, .32);
  transition: transform .1s ease, filter .15s ease;
}
.btn-primary:hover { filter: brightness(1.06); }
.btn-primary:active { transform: translateY(1px) scale(.99); }
.btn-primary:disabled { filter: grayscale(.55) brightness(.7); cursor: not-allowed; box-shadow: none; }

.foot { display: flex; justify-content: space-between; align-items: center; margin-top: 18px; font-size: 12.5px; }
.foot a { color: var(--mut); text-decoration: none; border-bottom: 1px dashed rgba(143, 161, 191, .5); padding-bottom: 1px; }
.foot a:hover { color: var(--blue); border-color: var(--blue); }
.tag-preview { color: var(--warn); font-size: 11px; border: 1px dashed rgba(251, 191, 36, .5); padding: 3px 8px; border-radius: 999px; }

/* ---------- 管理员端 ---------- */
.login-card { max-width: 420px; margin: 8vh auto 0; text-align: center; }
.login-card h2 { font-size: 20px; margin-bottom: 6px; }
.login-card .hint { margin-bottom: 18px; }

.input {
  width: 100%; height: 46px; border-radius: 11px;
  border: 1px solid var(--line); background: rgba(2, 6, 23, .5);
  color: var(--ink); padding: 0 14px; font-size: 15px; outline: none;
  transition: border-color .15s ease, box-shadow .15s ease;
}
.input:focus { border-color: rgba(52, 211, 153, .6); box-shadow: 0 0 0 3px rgba(52, 211, 153, .14); }
.input::placeholder { color: #55658A; }
input[type="time"].input { font-variant-numeric: tabular-nums; }
.field { margin-bottom: 14px; text-align: left; }
.field label { display: block; font-size: 12.5px; color: var(--mut); margin-bottom: 7px; letter-spacing: .4px; }
.field-row { display: flex; gap: 12px; }
.field-row .field { flex: 1; }

.tabs { display: flex; gap: 6px; flex-wrap: wrap; background: rgba(15, 23, 42, .7); border: 1px solid var(--line); border-radius: 13px; padding: 6px; margin-bottom: 16px; }
.tab {
  flex: 1; min-width: 120px; border: none; background: transparent; color: var(--mut);
  font-size: 14px; font-weight: 600; padding: 10px 8px; border-radius: 9px; cursor: pointer;
  transition: all .15s ease;
}
.tab:hover { color: var(--ink); background: rgba(56, 189, 248, .08); }
.tab.active { background: linear-gradient(135deg, rgba(52, 211, 153, .22), rgba(56, 189, 248, .14)); color: #A7F3D0; box-shadow: inset 0 0 0 1px rgba(52, 211, 153, .4); }
.tabpane { display: none; }
.tabpane.active { display: block; animation: fade .22s ease; }
@keyframes fade { from { opacity: 0; transform: translateY(5px); } to { opacity: 1; transform: none; } }

.code-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
  gap: 10px;
}
.code-item {
  display: grid;
  grid-template-columns: 30px minmax(0, 1fr) 38px;
  grid-template-areas:
    "idx input state"
    ". actions actions";
  align-items: center;
  gap: 8px 10px;
  min-width: 0;
  background: rgba(30, 42, 68, .28); border: 1px solid var(--line);
  border-radius: 12px; padding: 10px 12px;
}
.code-item .idx {
  grid-area: idx;
  width: 26px; height: 26px; border-radius: 8px;
  background: rgba(56, 189, 248, .14); color: var(--blue);
  display: grid; place-items: center; font-size: 12px; font-weight: 700;
}
.code-item .code-in {
  grid-area: input;
  width: 100%; min-width: 0; height: 38px;
  font-size: 16px; letter-spacing: 2px; text-align: center;
  font-variant-numeric: tabular-nums;
}
.code-item .state {
  grid-area: state;
  width: 38px; font-size: 11px; text-align: center; color: var(--mut);
}
.code-item .state.has { color: var(--acc); }
.code-item .code-actions {
  grid-area: actions;
  display: flex; justify-content: flex-end; gap: 8px;
}
.code-item .code-actions .btn { min-width: 64px; }

/* 开关 */
.switch { position: relative; display: inline-block; width: 52px; height: 28px; flex: none; }
.switch input { opacity: 0; width: 0; height: 0; }
.switch span {
  position: absolute; inset: 0; cursor: pointer; border-radius: 999px;
  background: rgba(148, 163, 184, .22); border: 1px solid var(--line); transition: .2s;
}
.switch span::before {
  content: ""; position: absolute; width: 20px; height: 20px; left: 3px; top: 3px;
  border-radius: 50%; background: #CBD5E1; transition: .2s;
}
.switch input:checked + span { background: rgba(52, 211, 153, .35); border-color: rgba(52, 211, 153, .6); }
.switch input:checked + span::before { transform: translateX(24px); background: var(--acc); }

.row-line { display: flex; align-items: center; gap: 14px; justify-content: space-between; padding: 13px 0; border-bottom: 1px dashed rgba(30, 42, 68, .9); }
.row-line:last-child { border-bottom: none; }
.row-line .lab { font-size: 14px; font-weight: 600; }
.row-line .desc { font-size: 12px; color: var(--mut); margin-top: 4px; line-height: 1.6; }

.slider { width: 100%; accent-color: var(--acc); height: 26px; }
.stat-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 10px; }
.stat {
  background: rgba(30, 42, 68, .3); border: 1px solid var(--line);
  border-radius: 12px; padding: 12px 14px;
}
.stat .k { font-size: 11.5px; color: var(--mut); letter-spacing: .5px; }
.stat .v { font-size: 19px; font-weight: 700; margin-top: 5px; font-variant-numeric: tabular-nums; }
.stat .v.small { font-size: 14px; }

.statusbar {
  margin-top: 18px; display: flex; flex-wrap: wrap; gap: 8px; align-items: center;
  font-size: 12px; color: var(--mut);
  background: rgba(15, 23, 42, .65); border: 1px solid var(--line);
  border-radius: 12px; padding: 10px 14px;
}
.statusbar b { color: var(--ink); font-weight: 600; }

/* Toast */
.toast {
  position: fixed; left: 50%; bottom: 30px; transform: translateX(-50%) translateY(20px);
  background: rgba(8, 15, 30, .96); border: 1px solid rgba(52, 211, 153, .45);
  color: var(--ink); font-size: 14px; padding: 11px 20px; border-radius: 12px;
  opacity: 0; pointer-events: none; transition: all .25s ease; z-index: 99;
  box-shadow: 0 14px 34px rgba(0, 0, 0, .5); max-width: 88vw; text-align: center;
}
.toast.show { opacity: 1; transform: translateX(-50%) translateY(0); }
.toast.err { border-color: rgba(248, 113, 113, .55); }

@media (max-width: 560px) {
  .wrap { padding: 18px 14px 36px; }
  .display .dot { width: 30px; height: 40px; font-size: 21px; }
  .key { height: 52px; }
  .field-row { flex-direction: column; gap: 0; }
}
)rawliteral";

const char PAGE_JS[] PROGMEM = R"rawliteral(
/* 智能门锁 · ESP-01S —— 用户端 / 管理员端共用脚本 */
(function () {
  'use strict';

  var $ = function (id) { return document.getElementById(id); };

  // Keep the admin token in sessionStorage; closing the browser session clears it.
  function sessionGet(key) {
    try { return window.sessionStorage.getItem(key) || ''; } catch (e) { return ''; }
  }
  function sessionSet(key, value) {
    try { window.sessionStorage.setItem(key, value); } catch (e) {}
  }
  function sessionRemove(key) {
    try { window.sessionStorage.removeItem(key); } catch (e) {}
  }

  var previewMode = false;
  /* 本地预览（file:// 或 localhost）与真机（192.168.4.1）需要不同的降级策略 */
  var isLocal = location.protocol === 'file:' || location.hostname === '' ||
    /^(localhost|127\.0\.0\.1|\[::1\])$/.test(location.hostname);

  /* ---------------- 基础工具 ---------------- */
  function toast(msg, isErr) {
    var t = $('toast');
    if (!t) return;
    t.textContent = msg;
    t.className = 'toast show' + (isErr ? ' err' : '');
    clearTimeout(t._tm);
    t._tm = setTimeout(function () { t.className = 'toast'; }, 2400);
  }

  var REQUEST_TIMEOUT_MS = 5000;

  function api(path, body) {
    var opt = body === undefined ? { cache: 'no-store' } : {
      method: 'POST',
      cache: 'no-store',
      headers: { 'Content-Type': 'application/json', 'Accept': 'application/json' },
      body: JSON.stringify(body)
    };
    var controller = window.AbortController ? new AbortController() : null;
    if (controller) opt.signal = controller.signal;

    // Abort timed-out requests when supported; the timer also handles older browsers.
    return new Promise(function (resolve, reject) {
      var done = false;
      var timer = setTimeout(function () {
        if (done) return;
        done = true;
        if (controller) controller.abort();
        reject(new Error('request timeout'));
      }, REQUEST_TIMEOUT_MS);

      fetch(path, opt).then(function (r) {
        if (!r.ok) throw new Error('http ' + r.status);
        return r.json();
      }).then(function (data) {
        if (done) return;
        done = true;
        clearTimeout(timer);
        resolve(data);
      }, function (e) {
        if (done) return;
        done = true;
        clearTimeout(timer);
        reject(e);
      });
    }).catch(function (e) {
      if (isLocal) enterPreview();
      throw e;
    });
  }

  function enterPreview() {
    if (previewMode) return;
    previewMode = true;
    var tag = $('previewTag');
    if (tag) tag.hidden = false;
  }

  /* 接口失败的统一提示：本地预览给演示提示，真机提示连接中断 */
  function netErr(previewMsg) {
    if (isLocal) toast(previewMsg || '预览模式：无后端', true);
    else toast('与设备连接中断（WiFi 可能已休眠）', true);
  }

  function pad(n) { return (n < 10 ? '0' : '') + n; }
  function hhmm(min) { return pad(Math.floor(min / 60)) + ':' + pad(min % 60); }
  function toMin(s) {
    var p = String(s || '').split(':');
    return (parseInt(p[0], 10) || 0) * 60 + (parseInt(p[1], 10) || 0);
  }
  function isSix(s) { return /^\d{6}$/.test(String(s || '')); }

  var REASON = {
    duty: '定时唤醒',
    window: '常开时段',
    notime: '待同步时间',
    debug: '调试模式',
    client: '设备在线',
    off: '休眠中'
  };
  var LOCK = {
    off: '已上锁',
    powering: '舵机上电中…',
    unlocking: '开锁中…',
    locking: '复位中…',
    manual: '手动控制'
  };

  /* ---------------- 状态轮询（两端共用） ---------------- */
  function renderStatus(s) {
    var wifi = $('chipWifi'), tm = $('chipTime');
    if (wifi) {
      if (s.offline) {
        wifi.className = 'chip amber';
        wifi.textContent = '与设备断开';
      } else if (s.wifiOn) {
        wifi.className = 'chip on';
        var r = REASON[s.reason] || s.reason;
        wifi.textContent = 'WiFi 开 · ' + r + (s.wifiNext > 0 ? ' ' + s.wifiNext + 's' : '');
      } else {
        wifi.className = 'chip amber';
        wifi.textContent = 'WiFi 休眠 · ' + (s.wifiNext > 0 ? s.wifiNext + 's 后开启' : '等待唤醒');
      }
    }
    if (tm) {
      if (s.synced) {
        var d = new Date(s.time * 1000);
        tm.textContent = pad(d.getHours()) + ':' + pad(d.getMinutes()) + ':' + pad(d.getSeconds());
        tm.className = 'chip blue mono';
      } else {
        tm.textContent = s.offline ? '--:--' : '时间未同步';
        tm.className = 'chip amber';
      }
    }
    return s;
  }

  var statusRequestBusy = false;

  function poll(statusCb) {
    if (statusRequestBusy) return Promise.resolve();
    statusRequestBusy = true;
    return api('/api/status').then(function (s) {
      if (previewMode) {
        previewMode = false;
        var tag = $('previewTag');
        if (tag) tag.hidden = true;
      }
      s.offline = 0;
      statusCb(renderStatus(s));
    }).catch(function () {
      if (isLocal) {
        statusCb(renderStatus({
          wifiOn: 1, reason: 'debug', wifiNext: 0, stations: 1,
          time: Math.floor(Date.now() / 1000), synced: 1, inWindow: 1, debug: 1,
          lock: 'off', pwr: 0, apSsid: 'LockGate-DEMO', seq: 0, writes: 0, erases: 0,
          offline: 0
        }));
      } else {
        statusCb(renderStatus({
          wifiOn: 0, reason: 'off', wifiNext: 0, stations: 0,
          time: 0, synced: 0, inWindow: 0, debug: 0,
          lock: 'off', pwr: 0, apSsid: '', seq: 0, writes: 0, erases: 0,
          offline: 1
        }));
      }
    }).then(function () {
      statusRequestBusy = false;
    }, function () {
      statusRequestBusy = false;
    });
  }

  function startPolling(statusCb, intervalMs) {
    var timer = 0;
    function schedule(delay) {
      clearTimeout(timer);
      timer = setTimeout(run, delay);
    }
    function run() {
      if (document.hidden) { schedule(intervalMs); return; }
      poll(statusCb).then(function () { schedule(intervalMs); });
    }
    document.addEventListener('visibilitychange', function () {
      if (!document.hidden) schedule(50);
    });
    run();
  }

  /* ================= 用户端 ================= */
  function initUser() {
    var code = '';
    var busy = false;
    var lockState = 'off';

    function render() {
      var d = $('display');
      if (!code) d.innerHTML = '<span class="ph">请输入 6 位开锁密码</span>';
      else {
        var h = '';
        for (var i = 0; i < code.length; i++) h += '<span class="dot">' + code[i] + '</span>';
        d.innerHTML = h;
      }
    }
    function msg(text, cls) {
      var m = $('msg');
      m.textContent = text || '';
      m.className = 'msg' + (cls ? ' ' + cls : '');
    }
    function setLock(st) {
      lockState = st;
      var ring = $('lockRing'), name = $('lockName');
      ring.className = 'lock-ring' + (st === 'off' ? '' : st === 'manual' ? ' open' : ' busy');
      name.textContent = LOCK[st] || st;
    }

    $('keypad').addEventListener('click', function (e) {
      var k = e.target.getAttribute && e.target.getAttribute('data-k');
      if (!k || busy) return;
      if (k === 'C') code = '';
      else if (k === 'B') code = code.slice(0, -1);
      else if (code.length < 6) code += k;
      render();
      msg('');
    });

    $('btnUnlock').addEventListener('click', function () {
      if (busy) return;
      if (code.length !== 6) { msg('请输入完整的 6 位密码', 'err'); shake(); return; }
      busy = true;
      $('btnUnlock').disabled = true;
      msg('验证中…');
      api('/api/unlock', { code: code }).then(function (r) {
        if (r.ok) {
          setLock('unlocking');
          msg(r.msg || '密码正确，正在开锁', 'ok');
          code = ''; render();
          setTimeout(function () { setLock('off'); }, 3000);
        } else {
          msg(r.msg || '密码错误', 'err');
          shake();
        }
      }).catch(function () {
        if (!isLocal) {
          msg('与设备连接中断（WiFi 可能已休眠）', 'err');
          shake();
          return;
        }
        // 本地预览演示
        msg('预览模式：模拟开锁成功（无后端）', 'ok');
        setLock('unlocking');
        code = ''; render();
        setTimeout(function () { setLock('off'); }, 3000);
      }).then(function () {
        busy = false;
        $('btnUnlock').disabled = false;
      });
    });

    function shake() {
      var d = $('display');
      d.classList.remove('shake');
      void d.offsetWidth;
      d.classList.add('shake');
    }

    // 设备连上 WiFi 后上报本机时间，完成时间同步
    api('/api/sync', { epoch: Math.floor(Date.now() / 1000) }).catch(function () {});

    startPolling(function (s) { setLock(s.lock); }, 3000);
    render();
  }

  /* ================= 管理员端 ================= */
  function initAdmin() {
    var token = sessionGet('admToken');
    var codes = [];

    function show(panel) {
      $('adminLogin').hidden = panel !== 'login';
      $('adminPanel').hidden = panel !== 'panel';
      $('chipSession').hidden = panel !== 'panel';
    }

    function auth(body) {
      body = body || {};
      body.token = token;
      return body;
    }

    /* --- 登录 --- */
    function login() {
      var k = $('inKey').value.trim();
      var btn = $('btnLogin');
      if (!k) { toast('请输入管理员密钥', true); return; }
      if (btn.disabled) return;
      btn.disabled = true;
      btn.textContent = '登录中…';
      api('/api/admin/login', { key: k }).then(function (r) {
        if (r.ok) {
          token = r.token;
          sessionSet('admToken', token);
          show('panel');
          loadConfig();
          toast('登录成功');
        } else toast(r.msg || '密钥错误', true);
      }).catch(function () {
        if (!isLocal) { netErr(); return; }
        // 本地预览演示
        token = 'preview';
        show('panel');
        codes = ['123456', '654321', '', '', '', '', '', '', '', ''];
        renderCodes();
        toast('预览模式：已进入演示后台');
      }).then(function () {
        btn.disabled = false;
        btn.textContent = '进入后台';
      });
    }
    $('btnLogin').addEventListener('click', login);
    $('inKey').addEventListener('keydown', function (e) { if (e.key === 'Enter') login(); });

    /* --- Tab 切换 --- */
    var tabs = document.querySelectorAll('.tab');
    for (var i = 0; i < tabs.length; i++) {
      tabs[i].addEventListener('click', function () {
        for (var j = 0; j < tabs.length; j++) tabs[j].classList.remove('active');
        this.classList.add('active');
        var panes = document.querySelectorAll('.tabpane');
        for (var k = 0; k < panes.length; k++) panes[k].classList.remove('active');
        $('tab-' + this.getAttribute('data-tab')).classList.add('active');
      });
    }

    /* --- 密码列表 --- */
    function renderCodes() {
      var box = $('codeList');
      box.innerHTML = '';
      for (var i = 0; i < 10; i++) {
        var v = codes[i] || '';
        var el = document.createElement('div');
        el.className = 'code-item';
        el.innerHTML =
          '<span class="idx">' + (i + 1) + '</span>' +
          '<input class="input code-in" data-i="' + i + '" type="text" inputmode="numeric" pattern="[0-9]*" maxlength="6" autocomplete="off" placeholder="&#x516D;&#x4F4D;&#x6570;&#x5B57;" value="' + v + '">' +
          '<span class="state' + (v ? ' has' : '') + '">' + (v ? '&#x542F;&#x7528;' : '&#x7A7A;') + '</span>' +
          '<div class="code-actions">' +
            '<button class="btn small code-save" data-i="' + i + '">&#x4FDD;&#x5B58;</button>' +
            '<button class="btn small danger code-clr" data-i="' + i + '">&#x5220;&#x9664;</button>' +
          '</div>';
        box.appendChild(el);
      }
    }

    function saveCode(index, value) {
      var code = String(value || '').trim();
      if (code && !isSix(code)) { toast('第 ' + (index + 1) + ' 组必须是 6 位数字', true); return; }
      var btn = document.querySelector('.code-save[data-i="' + index + '"]');
      if (btn) { btn.disabled = true; btn.textContent = '保存中'; }
      api('/api/saveCode', auth({ index: index, code: code })).then(function (r) {
        if (r.ok) {
          codes[index] = r.code || '';
          renderCodes();
          toast(code ? '第 ' + (index + 1) + ' 组密码已保存' : '第 ' + (index + 1) + ' 组密码已删除');
        } else toast(r.msg || '保存失败', true);
      }).catch(function () {
        if (isLocal) {
          codes[index] = code;
          renderCodes();
          toast('预览模式：已暂存');
        } else netErr();
      }).then(function () {
        if (btn) { btn.disabled = false; btn.textContent = '保存'; }
      });
    }

    $('codeList').addEventListener('input', function (e) {
      if (!e.target.classList || !e.target.classList.contains('code-in')) return;
      e.target.value = e.target.value.replace(/\D/g, '').slice(0, 6);
    });

    $('codeList').addEventListener('click', function (e) {
      var target = e.target;
      if (!target.classList) return;
      var idx = +target.getAttribute('data-i');
      if (target.classList.contains('code-save')) {
        var input = document.querySelector('.code-in[data-i="' + idx + '"]');
        saveCode(idx, input ? input.value : '');
      } else if (target.classList.contains('code-clr')) {
        saveCode(idx, '');
      }
    });

    $('codeList').addEventListener('keydown', function (e) {
      if (e.key !== 'Enter' || !e.target.classList.contains('code-in')) return;
      e.preventDefault();
      saveCode(+e.target.getAttribute('data-i'), e.target.value);
    });

    /* --- load config --- */
    function loadConfig() {
      api('/api/config', auth()).then(function (r) {
        if (!r.ok) { if (r.needLogin) { token = ''; sessionRemove('admToken'); show('login'); } return; }
        codes = r.codes;
        renderCodes();
        $('winStart').value = r.winStart;
        $('winEnd').value = r.winEnd;
        $('dbgMode').checked = !!r.debug;
        $('upSsid').value = r.uplinkSsid || '';
        $('lockAngle').value = r.lockAngle;
        $('unlockAngle').value = r.unlockAngle;
        $('holdMs').value = r.holdMs;
      }).catch(function () { renderCodes(); });
    }

    /* --- 网络与时间 --- */
    $('btnSaveNet').addEventListener('click', function () {
      var p = $('upPass').value;
      var body = {
        winStart: $('winStart').value || '08:00',
        winEnd: $('winEnd').value || '22:00',
        debug: $('dbgMode').checked ? 1 : 0,
        uplinkSsid: $('upSsid').value.trim(),
        uplinkPass: p
      };
      api('/api/saveNet', auth(body)).then(function (r) {
        if (r.ok) { $('upPass').value = ''; toast('网络与时段设置已保存'); }
        else toast(r.msg || '保存失败', true);
      }).catch(function () { $('upPass').value = ''; netErr('预览模式：设置已在页面暂存'); });
    });

    $('btnSyncNow').addEventListener('click', function () {
      api('/api/sync', { epoch: Math.floor(Date.now() / 1000) }).then(function (r) {
        toast(r.ok ? '已推送本机时间并完成同步' : '同步失败');
      }).catch(function () { netErr('预览模式：无后端，无法同步'); });
    });

    $('btnLogout').addEventListener('click', function () {
      token = '';
      sessionRemove('admToken');
      show('login');
      toast('已退出登录');
    });

    /* --- 密钥修改 --- */
    $('btnChangeKey').addEventListener('click', function () {
      var o = $('oldKey').value, n = $('newKey').value, n2 = $('newKey2').value;
      if (!o || !n) { toast('请填写完整', true); return; }
      if (n.length < 4) { toast('新密钥至少 4 位', true); return; }
      if (n !== n2) { toast('两次输入的新密钥不一致', true); return; }
      api('/api/changeKey', auth({ oldKey: o, newKey: n })).then(function (r) {
        if (r.ok) {
          $('oldKey').value = $('newKey').value = $('newKey2').value = '';
          token = r.token;
          sessionSet('admToken', token);
          toast('管理员密钥已修改');
        } else toast(r.msg || '修改失败', true);
      }).catch(function () {
        $('oldKey').value = $('newKey').value = $('newKey2').value = '';
        if (isLocal) toast('预览模式：演示成功（未真正修改）');
        else netErr();
      });
    });

    /* --- 门锁控制 --- */
    $('btnTestUnlock').addEventListener('click', function () {
      api('/api/testUnlock', auth()).then(function (r) {
        toast(r.ok ? '已触发开锁动作' : (r.msg || '失败'));
      }).catch(function () { netErr('预览模式：无后端'); });
    });
    $('btnPwrOn').addEventListener('click', function () {
      api('/api/servo', auth({ cmd: 'power_on' })).then(function (r) { toast(r.ok ? '舵机电源已开启' : '失败'); })
        .catch(function () { netErr('预览模式：无后端'); });
    });
    $('btnPwrOff').addEventListener('click', function () {
      api('/api/servo', auth({ cmd: 'power_off' })).then(function (r) { toast(r.ok ? '舵机电源已关闭' : '失败'); })
        .catch(function () { netErr('预览模式：无后端'); });
    });
    $('angle').addEventListener('input', function () { $('angleVal').textContent = this.value; });
    $('btnAngle').addEventListener('click', function () {
      api('/api/servo', auth({ cmd: 'angle', value: +$('angle').value })).then(function (r) {
        toast(r.ok ? '已输出 ' + $('angle').value + '°' : '失败');
      }).catch(function () { netErr('预览模式：无后端'); });
    });
    $('btnSaveAngles').addEventListener('click', function () {
      var la = +$('lockAngle').value, ua = +$('unlockAngle').value, h = +$('holdMs').value;
      if (!(la >= 0 && la <= 180 && ua >= 0 && ua <= 180)) { toast('角度需在 0~180 之间', true); return; }
      if (!(h >= 500 && h <= 10000)) { toast('保持时间需在 500~10000 ms', true); return; }
      api('/api/saveAngles', auth({ lockAngle: la, unlockAngle: ua, holdMs: h })).then(function (r) {
        toast(r.ok ? '动作参数已保存' : '失败');
      }).catch(function () { netErr('预览模式：参数已在页面暂存'); });
    });

    if (token) {
      show('panel');
      loadConfig();
    } else {
      show('login');
    }

    /* --- 状态轮询 --- */
    startPolling(function (s) {
        if (s.synced) {
          var d = new Date(s.time * 1000);
          $('stTime').textContent = d.getFullYear() + '-' + pad(d.getMonth() + 1) + '-' + pad(d.getDate()) +
            ' ' + pad(d.getHours()) + ':' + pad(d.getMinutes()) + ':' + pad(d.getSeconds());
        } else $('stTime').textContent = '--';
        $('stSync').textContent = s.synced ? (s.debug ? '已同步（调试常开）' : '已同步') : '未同步';
        $('stSta').textContent = s.stations;
        $('stAp').textContent = s.apSsid || '--';
        $('stLock').textContent = LOCK[s.lock] || s.lock;
        $('stPwr').textContent = s.pwr ? '供电中' : '已断电';
        $('stSeq').textContent = s.seq;
        $('stWr').textContent = s.writes;
        $('stEr').textContent = s.erases;
        $('adminStatus').innerHTML =
          'WiFi：<b>' + (s.wifiOn ? '开启' : '休眠') + '</b>（' + (REASON[s.reason] || s.reason) + '）' +
          ' · 下次切换 <b>' + s.wifiNext + 's</b>' +
          ' · 在线 <b>' + s.stations + '</b>' +
          ' · 时段 <b>' + (s.inWindow ? '内' : '外') + '</b>' +
          ' · 调试 <b>' + (s.debug ? '开' : '关') + '</b>' +
          ' · AP <b>' + (s.apSsid || '--') + '</b>';
      }, 3000);
  }

  /* ---------------- 入口 ---------------- */
  document.addEventListener('DOMContentLoaded', function () {
    if (document.getElementById('btnUnlock')) initUser();
    else if (document.getElementById('adminLogin')) initAdmin();
  });
})();
)rawliteral";

