/* 智能门锁 · ESP-01S —— 用户端 / 管理员端共用脚本 */
(function () {
  'use strict';

  var $ = function (id) { return document.getElementById(id); };
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

  function api(path, body) {
    var opt = body === undefined ? {} : {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body)
    };
    return fetch(path, opt).then(function (r) {
      if (!r.ok) throw new Error('http ' + r.status);
      return r.json();
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

  function poll(statusCb) {
    api('/api/status').then(function (s) {
      if (previewMode) {
        previewMode = false;
        var tag = $('previewTag');
        if (tag) tag.hidden = true;
      }
      s.offline = 0;
      statusCb(renderStatus(s));
    }).catch(function () {
      if (isLocal) {
        // 本地预览：模拟一套状态
        statusCb(renderStatus({
          wifiOn: 1, reason: 'debug', wifiNext: 0, stations: 1,
          time: Math.floor(Date.now() / 1000), synced: 1, inWindow: 1, debug: 1,
          lock: 'off', pwr: 0, apSsid: 'LockGate-DEMO', seq: 0, writes: 0, erases: 0,
          offline: 0
        }));
      } else {
        // 真机：请求失败 = 与设备断开（WiFi 休眠或已离开热点）
        statusCb(renderStatus({
          wifiOn: 0, reason: 'off', wifiNext: 0, stations: 0,
          time: 0, synced: 0, inWindow: 0, debug: 0,
          lock: 'off', pwr: 0, apSsid: '', seq: 0, writes: 0, erases: 0,
          offline: 1
        }));
      }
    });
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

    setInterval(function () {
      poll(function (s) { setLock(s.lock); });
    }, 2000);
    poll(function (s) { setLock(s.lock); });
    render();
  }

  /* ================= 管理员端 ================= */
  function initAdmin() {
    var token = sessionStorage.getItem('admToken') || '';
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
      if (!k) { toast('请输入管理员密钥', true); return; }
      api('/api/admin/login', { key: k }).then(function (r) {
        if (r.ok) {
          token = r.token;
          sessionStorage.setItem('admToken', token);
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
          '<input class="input code-in" data-i="' + i + '" inputmode="numeric" maxlength="6" placeholder="6 位数字" value="' + v + '">' +
          '<span class="state' + (v ? ' has' : '') + '">' + (v ? '启用' : '空') + '</span>' +
          '<button class="btn small danger code-clr" data-i="' + i + '">清空</button>';
        box.appendChild(el);
      }
    }
    $('codeList').addEventListener('click', function (e) {
      if (e.target.classList && e.target.classList.contains('code-clr')) {
        var idx = +e.target.getAttribute('data-i');
        codes[idx] = '';
        renderCodes();
      }
    });

    $('btnSaveCodes').addEventListener('click', function () {
      var ins = document.querySelectorAll('.code-in');
      var out = [];
      for (var i = 0; i < ins.length; i++) {
        var v = ins[i].value.trim();
        if (v === '') { out.push(''); continue; }
        if (!isSix(v)) { toast('第 ' + (i + 1) + ' 组必须是 6 位数字', true); return; }
        out.push(v);
      }
      api('/api/saveCodes', auth({ codes: out })).then(function (r) {
        if (r.ok) { codes = r.codes; renderCodes(); toast('密码已保存'); }
        else toast(r.msg || '保存失败', true);
      }).catch(function () {
        if (isLocal) { codes = out; renderCodes(); toast('预览模式：密码已在页面暂存'); }
        else netErr();
      });
    });

    /* --- 加载配置 --- */
    function loadConfig() {
      api('/api/config', auth()).then(function (r) {
        if (!r.ok) { if (r.needLogin) { token = ''; sessionStorage.removeItem('admToken'); show('login'); } return; }
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
      sessionStorage.removeItem('admToken');
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
          sessionStorage.setItem('admToken', token);
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
      api('/api/servo', auth({ cmd: 'power_on' })).then(function (r) { toast(r.ok ? 'IO0 已置高' : '失败'); })
        .catch(function () { netErr('预览模式：无后端'); });
    });
    $('btnPwrOff').addEventListener('click', function () {
      api('/api/servo', auth({ cmd: 'power_off' })).then(function (r) { toast(r.ok ? 'IO0 已置低' : '失败'); })
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

    /* --- 状态轮询 --- */
    setInterval(function () {
      poll(function (s) {
        if (s.synced) {
          var d = new Date(s.time * 1000);
          $('stTime').textContent = d.getFullYear() + '-' + pad(d.getMonth() + 1) + '-' + pad(d.getDate()) +
            ' ' + pad(d.getHours()) + ':' + pad(d.getMinutes()) + ':' + pad(d.getSeconds());
        } else $('stTime').textContent = '--';
        $('stSync').textContent = s.synced ? (s.debug ? '已同步（调试常开）' : '已同步') : '未同步';
        $('stSta').textContent = s.stations;
        $('stAp').textContent = s.apSsid || '--';
        $('stLock').textContent = LOCK[s.lock] || s.lock;
        $('stPwr').textContent = s.pwr ? '高 · 供电中' : '低 · 断电';
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
      });
    }, 2000);
    poll(function () {});
  }

  /* ---------------- 入口 ---------------- */
  document.addEventListener('DOMContentLoaded', function () {
    if (document.getElementById('btnUnlock')) initUser();
    else if (document.getElementById('adminLogin')) initAdmin();
  });
})();
