/** LG Therma PWA — ovládání přes MQTT (WSS). */

import { MqttBridge } from './mqtt-client.js';

const STORAGE_KEY = 'lgtherma-pwa-settings';
const MQTT_AUTO_KEY = 'lgtherma-pwa-mqtt-auto';

const DEFAULT_WSS =
  'wss://n9e16b3c.ala.eu-central-1.emqxsl.com:8084/mqtt';

const state = {
  mqttConnected: false,
  mqttStatus: 'idle',
  mqttError: '',
  tab5Online: false,
  teleFresh: false,
  watchActive: false,
  autoMode: true,
  setpoint: null,
  power: false,
  pump: false,
  compressor: false,
  defrost: false,
  elec: false,
  lin: false,
  alarm: false,
  poruchaText: '',
  faultVisible: false,
  faultText: '',
  temps: {
    room: null,
    outdoor: null,
    inlet: null,
    outlet: null,
  },
};

const $ = (sel) => document.querySelector(sel);
const mqtt = new MqttBridge(applyMqttEvent);

const FAULT_SHOW_MS = 2000;
const FAULT_HIDE_MS = 800;
let faultTimer = null;
let faultPending = null;

function resetTelemetryState() {
  state.tab5Online = false;
  state.teleFresh = false;
  state.watchActive = false;
  state.autoMode = true;
  state.setpoint = null;
  state.power = false;
  state.pump = false;
  state.compressor = false;
  state.defrost = false;
  state.elec = false;
  state.lin = false;
  state.alarm = false;
  state.poruchaText = '';
  state.faultVisible = false;
  state.faultText = '';
  state.temps = { room: null, outdoor: null, inlet: null, outlet: null };
  clearTimeout(faultTimer);
  faultPending = null;
}

function isLinOk() {
  if (state.lin) {
    return true;
  }
  if (!state.teleFresh) {
    return false;
  }
  return (
    state.temps.inlet != null ||
    state.temps.outdoor != null ||
    state.pump ||
    state.compressor
  );
}

function scheduleFaultBanner() {
  if (!state.tab5Online || !state.teleFresh) {
    state.faultVisible = false;
    state.faultText = '';
    return;
  }

  const text = state.poruchaText.trim();
  const wantShow = text.length > 0 || state.alarm;
  const nextText = text || (state.alarm ? 'Alarm hlášen přes MQTT' : '');

  if (wantShow && state.faultVisible && state.faultText !== nextText && nextText) {
    state.faultText = nextText;
    return;
  }

  if (faultPending && faultPending.wantShow === wantShow && faultPending.text === nextText) {
    return;
  }

  clearTimeout(faultTimer);
  faultPending = { wantShow, text: nextText };

  faultTimer = setTimeout(() => {
    faultPending = null;
    if (!state.tab5Online || !state.teleFresh) {
      state.faultVisible = false;
      state.faultText = '';
      renderFaultBanner();
      return;
    }
    state.faultVisible = wantShow;
    state.faultText = wantShow ? nextText : '';
    renderFaultBanner();
  }, wantShow ? FAULT_SHOW_MS : FAULT_HIDE_MS);
}

function renderFaultBanner() {
  const faultBanner = $('#fault-banner');
  if (!faultBanner) {
    return;
  }
  if (state.faultVisible && state.faultText) {
    faultBanner.classList.remove('hidden');
    $('#fault-text').textContent = state.faultText;
  } else {
    faultBanner.classList.add('hidden');
    $('#fault-text').textContent = '';
  }
}

function loadSettings() {
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    return raw ? JSON.parse(raw) : {};
  } catch {
    return {};
  }
}

function saveSettings(data) {
  const prev = loadSettings();
  localStorage.setItem(STORAGE_KEY, JSON.stringify({ ...prev, ...data }));
}

function formatTemp(v, decimals = 1) {
  if (v === null || v === undefined || Number.isNaN(v)) {
    return '—';
  }
  return decimals === 0 ? String(Math.round(v)) : v.toFixed(decimals);
}

function brokerConnected() {
  return mqtt.isConnected();
}

function displaySetpoint() {
  if (!brokerConnected()) {
    return '—';
  }
  return state.autoMode
    ? formatTemp(state.setpoint, 1)
    : formatTemp(state.setpoint, 0);
}

function requireMqtt() {
  if (mqtt.isConnected()) {
    return true;
  }
  flashStatus('Nejdřív připoj MQTT v Nastavení');
  return false;
}

function applyMqttEvent(ev) {
  if (ev.type === 'status') {
    state.mqttStatus = ev.status;
    state.mqttError = ev.error || '';
    state.mqttConnected = ev.status === 'connected';
    if (ev.status === 'connecting') {
      state.teleFresh = false;
      state.tab5Online = false;
      state.setpoint = null;
    }
    render();
    return;
  }

  if (ev.type === 'mqtt') {
    state.mqttConnected = ev.connected;
    if (!ev.connected && !ev.reconnecting) {
      resetTelemetryState();
    }
    render();
    return;
  }

  if (ev.type === 'tele') {
    state.teleFresh = true;
    const p = ev.patch;
    if (p.temps) {
      state.temps = { ...state.temps, ...p.temps };
    }
    if (p.setpoint !== undefined) {
      state.setpoint = p.setpoint;
    }
    if (p.autoMode !== undefined) {
      state.autoMode = p.autoMode;
    }
    if (p.power !== undefined) {
      state.power = p.power;
    }
    if (p.pump !== undefined) {
      state.pump = p.pump;
    }
    if (p.compressor !== undefined) {
      state.compressor = p.compressor;
    }
    if (p.lin !== undefined) {
      state.lin = p.lin;
    }
    if (p.defrost !== undefined) {
      state.defrost = p.defrost;
    }
    if (p.elec !== undefined) {
      state.elec = p.elec;
    }
    if (p.alarm !== undefined) {
      state.alarm = p.alarm;
    }
    if (p.poruchaText !== undefined) {
      state.poruchaText = p.poruchaText;
    }
    if (p.watchActive !== undefined) {
      state.watchActive = p.watchActive;
    }
    if (p.tab5Online !== undefined) {
      state.tab5Online = p.tab5Online;
      if (!p.tab5Online) {
        state.teleFresh = false;
        state.setpoint = null;
        state.temps = { room: null, outdoor: null, inlet: null, outlet: null };
        state.poruchaText = '';
        state.alarm = false;
        state.faultVisible = false;
        state.faultText = '';
        clearTimeout(faultTimer);
        faultPending = null;
      }
    }
    scheduleFaultBanner();
    render();
  }
}

function adjustSetpoint(delta) {
  if (!requireMqtt()) {
    return;
  }
  mqtt.publishCmd('setpoint', delta > 0 ? '+' : '-');
  flashStatus(`→ setpoint ${delta > 0 ? '+' : '-'}`);
}

function setPower(on) {
  if (!requireMqtt()) {
    return;
  }
  mqtt.publishCmd('power', on ? 'ON' : 'OFF');
  flashStatus(`→ power ${on ? 'ON' : 'OFF'}`);
}

function flashStatus(msg) {
  const el = $('#sig-mqtt');
  if (!el) {
    return;
  }
  el.classList.add('sig-flash');
  el.setAttribute('aria-label', msg);
  setTimeout(() => {
    el.classList.remove('sig-flash');
    render();
  }, 1200);
}

function mqttSigMeta() {
  if (state.mqttStatus === 'connecting') {
    return { state: 'warn', label: 'MQTT: připojování', pulse: true };
  }
  if (state.mqttStatus === 'error') {
    return { state: 'error', label: `MQTT: ${state.mqttError || 'chyba'}`, pulse: false };
  }
  if (state.mqttConnected && state.tab5Online) {
    return { state: 'ok', label: 'MQTT: připojeno', pulse: false };
  }
  if (state.mqttConnected) {
    return { state: 'warn', label: 'MQTT: čekám na Tab5', pulse: false };
  }
  return { state: 'off', label: 'MQTT: odpojeno', pulse: false };
}

function linSigMeta() {
  if (!state.mqttConnected && state.mqttStatus !== 'connecting') {
    return { state: 'off', label: 'LIN: odpojeno', pulse: false };
  }
  if (isLinOk()) {
    return { state: 'ok', label: 'LIN: OK', pulse: false };
  }
  if (state.teleFresh) {
    return { state: 'warn', label: 'LIN: bez spojení', pulse: false };
  }
  return { state: 'off', label: 'LIN: čekám na data', pulse: false };
}

function applySig(el, meta, extraClass = '') {
  if (!el) {
    return;
  }
  const extra = extraClass ? ` ${extraClass}` : '';
  el.className = `sig${extra} sig-${meta.state}${meta.pulse ? ' sig-pulse' : ''}`;
  el.setAttribute('aria-label', meta.label);
  el.setAttribute('title', meta.label);
}

const STAT_LABELS = {
  power: 'Zapnuto',
  pump: 'Čerpadlo',
  compressor: 'Kompresor',
  defrost: 'Odmrazování',
  elec: 'El. topení',
};

function renderLeds() {
  const map = {
    power: state.power,
    pump: state.pump,
    compressor: state.compressor,
    defrost: state.defrost,
    elec: state.elec,
  };
  for (const [key, on] of Object.entries(map)) {
    const el = $(`#stat-${key}`);
    if (!el) {
      continue;
    }
    el.classList.toggle('stat-on', on);
    const name = STAT_LABELS[key] || key;
    const label = `${name}: ${on ? 'zapnuto' : 'vypnuto'}`;
    el.setAttribute('aria-label', label);
    el.setAttribute('title', name);
  }
}

function mqttSessionActive() {
  return mqtt.sessionActive();
}

function render() {
  applySig($('#sig-mqtt'), mqttSigMeta());
  applySig($('#sig-lin'), linSigMeta(), 'sig-label');

  renderFaultBanner();

  const title = $('#sp-title');
  if (state.autoMode) {
    title.textContent = 'Nastavení pokojové teploty';
    title.className = 'sp-title sp-room';
  } else {
    title.textContent = 'Nastavení teploty vody';
    title.className = 'sp-title sp-water';
  }

  const spVal = $('#sp-value');
  spVal.textContent = displaySetpoint();

  $('#water-sp').classList.add('hidden');

  const live = brokerConnected();
  $('#temp-room').textContent = formatTemp(live ? state.temps.room : null, 1);
  $('#temp-outdoor').textContent = formatTemp(live ? state.temps.outdoor : null, 1);
  $('#temp-inlet').textContent = formatTemp(live ? state.temps.inlet : null, 0);
  $('#temp-outlet').textContent = formatTemp(live ? state.temps.outlet : null, 0);

  renderLeds();

  const sessionActive = mqttSessionActive();
  $('#btn-connect').disabled = sessionActive;
  $('#btn-disconnect').disabled = !sessionActive;

  const statusEl = $('#cfg-status');
  if (statusEl) {
    if (state.mqttStatus === 'error') {
      statusEl.textContent = state.mqttError;
      statusEl.className = 'cfg-status cfg-status-error';
    } else if (state.mqttConnected) {
      statusEl.textContent = state.tab5Online
        ? 'Připojeno — Tab5 online'
        : 'Připojeno — čekám na lgtherma/availability';
      statusEl.className = 'cfg-status cfg-status-ok';
    } else if (state.mqttStatus === 'connecting') {
      statusEl.textContent = state.mqttError || 'Připojování…';
      statusEl.className = 'cfg-status';
    } else {
      statusEl.textContent = '';
      statusEl.className = 'cfg-status';
    }
  }
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
  $('#btn-start').addEventListener('click', () => setPower(true));
  $('#btn-stop').addEventListener('click', () => setPower(false));
}

function bindSettings() {
  const saved = loadSettings();
  $('#cfg-host').value = saved.host || DEFAULT_WSS;
  $('#cfg-user').value = saved.user || '';
  $('#cfg-pass').value = saved.password || '';
  $('#cfg-prefix').value = saved.prefix || 'lgtherma';

  $('#btn-connect').addEventListener('click', () => {
    const host = $('#cfg-host').value.trim();
    const user = $('#cfg-user').value.trim();
    const password = $('#cfg-pass').value;
    const prefix = $('#cfg-prefix').value.trim() || 'lgtherma';
    if (!host) {
      state.mqttStatus = 'error';
      state.mqttError = 'Zadej URL brokeru (WSS)';
      render();
      return;
    }
    saveSettings({ host, user, password, prefix });
    localStorage.setItem(MQTT_AUTO_KEY, 'true');
    mqtt.connect({ url: host, user, password, prefix });
  });

  $('#btn-disconnect').addEventListener('click', () => {
    localStorage.setItem(MQTT_AUTO_KEY, 'false');
    mqtt.disconnect(true);
  });
}

function tryAutoConnect() {
  if (localStorage.getItem(MQTT_AUTO_KEY) === 'false') {
    return;
  }
  const saved = loadSettings();
  if (saved.host && saved.user && saved.password) {
    mqtt.connect({
      url: saved.host,
      user: saved.user,
      password: saved.password,
      prefix: saved.prefix || 'lgtherma',
    });
  }
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
  window.addEventListener('beforeunload', () => {
    if (mqtt.isConnected()) {
      mqtt.disconnect(true);
    }
  });
  registerSw();
  tryAutoConnect();
}

document.addEventListener('DOMContentLoaded', init);
