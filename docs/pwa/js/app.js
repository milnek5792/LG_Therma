/** LG Therma PWA — klikatelný prototyp (demo data). */

const STORAGE_KEY = 'lgtherma-pwa-settings';

const state = {
  connected: false,
  autoMode: true,
  setpoint: 22.5,
  waterSp: 38,
  power: false,
  quiet: false,
  pump: false,
  compressor: false,
  defrost: false,
  elec: false,
  lin: true,
  fault: false,
  faultText: '',
  temps: {
    room: 21.3,
    outdoor: 8.2,
    inlet: 35,
    outlet: 42,
  },
  planTitle: 'TÝDENNÍ PLÁN AKTIVNÍ',
  planText: 'VT2: běžný režim',
};

const $ = (sel) => document.querySelector(sel);

function loadSettings() {
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    return raw ? JSON.parse(raw) : {};
  } catch {
    return {};
  }
}

function saveSettings(data) {
  localStorage.setItem(STORAGE_KEY, JSON.stringify(data));
}

function formatClock() {
  const d = new Date();
  const hh = String(d.getHours()).padStart(2, '0');
  const mm = String(d.getMinutes()).padStart(2, '0');
  return `${hh}:${mm}`;
}

function clampSetpoint(v) {
  if (state.autoMode) {
    return Math.min(24, Math.max(18, Math.round(v * 10) / 10));
  }
  return Math.min(55, Math.max(20, Math.round(v)));
}

function adjustSetpoint(delta) {
  const step = state.autoMode ? 0.5 : 1;
  state.setpoint = clampSetpoint(state.setpoint + delta * step);
  if (state.autoMode) {
    state.waterSp = Math.round(34 + (state.setpoint - 21) * 2);
  }
  render();
  flashDemoCmd(`setpoint → ${state.setpoint}`);
}

function setMode(auto) {
  state.autoMode = auto;
  if (auto) {
    state.setpoint = 22.5;
    state.waterSp = 38;
  } else {
    state.setpoint = 38;
  }
  render();
  flashDemoCmd(`mode → ${auto ? 'room' : 'water'}`);
}

function setPower(on) {
  state.power = on;
  state.pump = on;
  state.compressor = on && state.setpoint > 20;
  render();
  flashDemoCmd(`power → ${on ? 'ON' : 'OFF'}`);
}

function toggleQuiet() {
  state.quiet = !state.quiet;
  render();
  flashDemoCmd(`quiet → ${state.quiet ? 'ON' : 'OFF'}`);
}

function toggleFaultDemo() {
  state.fault = !state.fault;
  state.faultText = state.fault
    ? 'Ztráta spojení s venkovní jednotkou (LIN)'
    : '';
  render();
}

function flashDemoCmd(msg) {
  if (!state.connected) {
    return;
  }
  const pill = $('#mqtt-pill');
  if (!pill) {
    return;
  }
  const prev = pill.textContent;
  pill.textContent = `→ ${msg}`;
  pill.classList.add('pill-ok');
  setTimeout(() => {
    pill.textContent = prev;
  }, 1200);
}

function renderLeds() {
  const map = {
    power: state.power,
    pump: state.pump,
    compressor: state.compressor,
    defrost: state.defrost,
    elec: state.elec,
    quiet: state.quiet,
  };
  for (const [key, on] of Object.entries(map)) {
    const el = $(`#led-${key}`);
    if (el) {
      el.classList.toggle('led-on', on);
    }
  }
}

function render() {
  $('#clock').textContent = formatClock();

  const mqttPill = $('#mqtt-pill');
  mqttPill.textContent = state.connected ? 'MQTT OK' : 'MQTT ---';
  mqttPill.className = `pill ${state.connected ? 'pill-ok' : 'pill-muted'}`;

  const linPill = $('#lin-pill');
  linPill.textContent = state.lin ? 'LIN OK' : 'LIN ---';
  linPill.className = `pill ${state.lin ? 'pill-ok' : 'pill-muted'}`;

  const faultBanner = $('#fault-banner');
  if (state.fault) {
    faultBanner.classList.remove('hidden');
    $('#fault-text').textContent = state.faultText;
  } else {
    faultBanner.classList.add('hidden');
  }

  const title = $('#sp-title');
  if (state.autoMode) {
    title.textContent = 'Nastavení pokojové teploty';
    title.className = 'sp-title sp-room';
  } else {
    title.textContent = 'Nastavení teploty vody';
    title.className = 'sp-title sp-water';
  }

  const spVal = $('#sp-value');
  spVal.textContent = state.autoMode
    ? state.setpoint.toFixed(1)
    : String(Math.round(state.setpoint));

  const waterEl = $('#water-sp');
  if (state.autoMode) {
    waterEl.classList.remove('hidden');
    waterEl.textContent = `Voda SP ${state.waterSp} °C`;
  } else {
    waterEl.classList.add('hidden');
  }

  $('#btn-mode-auto').classList.toggle('chip-active', state.autoMode);
  $('#btn-mode-water').classList.toggle('chip-active', !state.autoMode);
  $('#btn-mode-water').classList.toggle('chip-water', !state.autoMode);

  $('#plan-title').textContent = state.planTitle;
  $('#plan-text').textContent = state.planText;

  $('#temp-room').textContent = state.temps.room.toFixed(1);
  $('#temp-outdoor').textContent = state.temps.outdoor.toFixed(1);
  $('#temp-inlet').textContent = String(state.temps.inlet);
  $('#temp-outlet').textContent = String(state.temps.outlet);

  renderLeds();

  $('#btn-quiet').textContent = `Tichý režim: ${state.quiet ? 'ZAP' : 'VYP'}`;

  $('#btn-connect').disabled = state.connected;
  $('#btn-disconnect').disabled = !state.connected;
}

function showView(name) {
  $('#view-home').classList.toggle('view-active', name === 'home');
  $('#view-home').hidden = name !== 'home';
  $('#view-settings').classList.toggle('view-active', name === 'settings');
  $('#view-settings').hidden = name !== 'settings';

  document.querySelectorAll('.nav-btn').forEach((btn) => {
    btn.classList.toggle('nav-active', btn.dataset.view === name);
  });
}

function bindNav() {
  document.querySelectorAll('.nav-btn').forEach((btn) => {
    btn.addEventListener('click', () => showView(btn.dataset.view));
  });
}

function bindControls() {
  $('#btn-minus').addEventListener('click', () => adjustSetpoint(-1));
  $('#btn-plus').addEventListener('click', () => adjustSetpoint(1));
  $('#btn-mode-auto').addEventListener('click', () => setMode(true));
  $('#btn-mode-water').addEventListener('click', () => setMode(false));
  $('#btn-start').addEventListener('click', () => setPower(true));
  $('#btn-stop').addEventListener('click', () => setPower(false));
  $('#btn-quiet').addEventListener('click', toggleQuiet);

  // Dvojité klepnutí na horní lištu = demo porucha
  let tapCount = 0;
  let tapTimer = null;
  $('.topbar').addEventListener('click', () => {
    tapCount += 1;
    clearTimeout(tapTimer);
    tapTimer = setTimeout(() => {
      tapCount = 0;
    }, 400);
    if (tapCount >= 2) {
      tapCount = 0;
      toggleFaultDemo();
    }
  });
}

function bindSettings() {
  const saved = loadSettings();
  if (saved.host) {
    $('#cfg-host').value = saved.host;
  }
  if (saved.user) {
    $('#cfg-user').value = saved.user;
  }
  if (saved.prefix) {
    $('#cfg-prefix').value = saved.prefix;
  }

  $('#btn-connect').addEventListener('click', () => {
    saveSettings({
      host: $('#cfg-host').value.trim(),
      user: $('#cfg-user').value.trim(),
      prefix: $('#cfg-prefix').value.trim() || 'lgtherma',
    });
    state.connected = true;
    render();
    flashDemoCmd('watch → ON');
  });

  $('#btn-disconnect').addEventListener('click', () => {
    state.connected = false;
    render();
  });
}

function simulateTelemetry() {
  if (!state.connected || !state.power) {
    return;
  }
  const drift = (Math.random() - 0.5) * 0.2;
  state.temps.room = Math.round((state.temps.room + drift) * 10) / 10;
  if (state.autoMode) {
    state.temps.outlet = Math.round(state.waterSp + (Math.random() - 0.5) * 2);
    state.temps.inlet = state.temps.outlet - Math.round(4 + Math.random() * 4);
  }
  render();
}

function registerSw() {
  if (!('serviceWorker' in navigator)) {
    return;
  }
  navigator.serviceWorker.register('./sw.js').catch(() => {});
}

function init() {
  bindNav();
  bindControls();
  bindSettings();
  render();
  setInterval(() => {
    $('#clock').textContent = formatClock();
  }, 30_000);
  setInterval(simulateTelemetry, 5000);
  registerSw();
}

document.addEventListener('DOMContentLoaded', init);
