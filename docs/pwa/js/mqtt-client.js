/** MQTT bridge — lgtherma/* topicy kompatibilní s Tab5 (net_mqtt_client.cpp). */

/** Retained zprávy ignorované jen krátce po připojení (kvůli blikání poruchy). */
const HYDRATE_SKIP_RETAINED = new Set([
  'tele/alarm',
  'tele/porucha',
  'tele/temp_set',
]);
const TELE_NA = '___';

const TELE_SUFFIXES = [
  'availability',
  'tele/temp_room',
  'tele/temp_outdoor',
  'tele/temp_inlet',
  'tele/temp_outlet',
  'tele/temp_set',
  'tele/reg_mode',
  'tele/power',
  'tele/pump',
  'tele/compressor',
  'tele/lin',
  'tele/defrost',
  'tele/elec_heat',
  'tele/alarm',
  'tele/porucha',
  'tele/watch',
];

export class MqttBridge {
  constructor(onState) {
    this.onState = onState;
    this.client = null;
    this.prefix = 'lgtherma';
    this.watchTimer = null;
    this.status = 'idle';
    this.error = '';
    this.hydrateUntil = 0;
    this.hydrateTimer = null;
    this.closing = false;
    this.userStopped = false;
  }

  sessionActive() {
    if (this.userStopped) {
      return false;
    }
    return this.status === 'connected' || this.status === 'connecting';
  }

  isConnected() {
    return this.client?.connected === true;
  }

  topic(suffix) {
    return `${this.prefix}/${suffix}`;
  }

  connect({ url, user, password, prefix }) {
    if (typeof mqtt === 'undefined') {
      this.setStatus('error', 'mqtt.js se nenačetlo');
      return;
    }
    this.disconnect(false);
    this.userStopped = false;
    this.prefix = (prefix || 'lgtherma').replace(/\/+$/, '');
    this.setStatus('connecting', '');

    const clientId = `LGThermaPWA_${Math.random().toString(16).slice(2, 10)}`;
    this.client = mqtt.connect(url, {
      username: user || undefined,
      password: password || undefined,
      clientId,
      reconnectPeriod: 5000,
      connectTimeout: 20_000,
      keepalive: 45,
      clean: true,
    });

    this.client.on('connect', () => {
      this.beginHydrate();
      this.setStatus('connected', '');
      this.subscribeAll();
      this.publishWatch(true);
      this.startWatchKeepalive();
    });

    this.client.on('reconnect', () => {
      this.setStatus('connecting', 'Obnovuji spojení…');
    });

    this.client.on('close', () => {
      this.stopWatchKeepalive();
      if (this.userStopped) {
        if (this.status !== 'error') {
          this.setStatus('idle', '');
        }
        this.onState({ type: 'mqtt', connected: false, reconnecting: false });
        return;
      }
      this.setStatus('connecting', 'Obnovuji spojení…');
      this.onState({ type: 'mqtt', connected: false, reconnecting: true });
    });

    this.client.on('error', (err) => {
      this.setStatus('error', err?.message || 'Chyba MQTT');
    });

    this.client.on('message', (topic, payload, packet) => {
      this.handleMessage(topic, payload.toString(), packet?.retain === true);
    });
  }

  disconnect(sendWatchOff = true) {
    this.userStopped = true;
    this.closing = true;
    this.stopWatchKeepalive();
    this.endHydrate();
    if (this.client) {
      const client = this.client;
      this.client = null;
      client.removeAllListeners();
      if (sendWatchOff && client.connected) {
        try {
          client.publish(this.topic('cmd/watch'), 'OFF', { qos: 0 });
        } catch {
          /* ignore */
        }
      }
      try {
        client.end(true);
      } catch {
        /* ignore */
      }
    }
    this.setStatus('idle', '');
    this.onState({ type: 'mqtt', connected: false, reconnecting: false });
    this.closing = false;
  }

  setStatus(status, error) {
    this.status = status;
    this.error = error || '';
    this.onState({ type: 'status', status: this.status, error: this.error });
  }

  beginHydrate() {
    this.endHydrate();
    this.hydrateUntil = Date.now() + 3500;
    this.hydrateTimer = setTimeout(() => {
      this.hydrateUntil = 0;
      this.hydrateTimer = null;
      if (this.client?.connected) {
        this.publishWatch(true);
      }
    }, 3600);
  }

  endHydrate() {
    if (this.hydrateTimer) {
      clearTimeout(this.hydrateTimer);
      this.hydrateTimer = null;
    }
    this.hydrateUntil = 0;
  }

  shouldSkipRetained(rel) {
    return (
      Date.now() < this.hydrateUntil &&
      HYDRATE_SKIP_RETAINED.has(rel)
    );
  }

  subscribeAll() {
    if (!this.client?.connected) {
      return;
    }
    for (const suffix of TELE_SUFFIXES) {
      this.client.subscribe(this.topic(suffix), { qos: 0 });
    }
  }

  publishWatch(on) {
    if (!this.client?.connected) {
      return;
    }
    this.client.publish(this.topic('cmd/watch'), on ? 'ON' : 'OFF', { qos: 0 });
  }

  startWatchKeepalive() {
    this.stopWatchKeepalive();
    this.watchTimer = setInterval(() => {
      if (this.client?.connected) {
        this.publishWatch(true);
      }
    }, 120_000);
  }

  stopWatchKeepalive() {
    if (this.watchTimer) {
      clearInterval(this.watchTimer);
      this.watchTimer = null;
    }
  }

  publishCmd(suffix, payload) {
    if (!this.client?.connected) {
      return false;
    }
    this.client.publish(this.topic(`cmd/${suffix}`), String(payload), { qos: 0 });
    if (suffix !== 'watch') {
      this.publishWatch(true);
    }
    return true;
  }

  handleMessage(topic, raw, retained = false) {
    try {
      this.handleMessageInner(topic, raw, retained);
    } catch (err) {
      console.error('[MQTT] parse', topic, err);
    }
  }

  handleMessageInner(topic, raw, retained = false) {
    const base = `${this.prefix}/`;
    if (!topic.startsWith(base)) {
      return;
    }
    const rel = topic.slice(base.length);
    if (retained && this.shouldSkipRetained(rel)) {
      return;
    }
    const msg = raw.trim();
    const patch = {};

    switch (rel) {
      case 'availability':
        patch.tab5Online = msg.toLowerCase() === 'online';
        break;
      case 'tele/temp_room': {
        const v = parseTemp(msg);
        if (v !== null) {
          patch.temps = { room: v };
        }
        break;
      }
      case 'tele/temp_outdoor': {
        const v = parseTemp(msg);
        if (v !== null) {
          patch.temps = { outdoor: v };
        }
        break;
      }
      case 'tele/temp_inlet': {
        const v = parseTemp(msg);
        if (v !== null) {
          patch.temps = { inlet: Math.round(v) };
        }
        break;
      }
      case 'tele/temp_outlet': {
        const v = parseTemp(msg);
        if (v !== null) {
          patch.temps = { outlet: Math.round(v) };
        }
        break;
      }
      case 'tele/temp_set': {
        const v = parseTemp(msg);
        if (v !== null) {
          patch.setpoint = v;
        } else if (msg === TELE_NA || msg === '---' || msg.toLowerCase() === 'off') {
          patch.setpoint = null;
        }
        break;
      }
      case 'tele/reg_mode':
        patch.autoMode = msg.toLowerCase() === 'room';
        break;
      case 'tele/power':
        patch.power = parseOnOff(msg);
        break;
      case 'tele/pump':
        patch.pump = parseOnOff(msg);
        break;
      case 'tele/compressor':
        patch.compressor = parseOnOff(msg);
        break;
      case 'tele/lin':
        patch.lin = parseOnOff(msg);
        break;
      case 'tele/defrost':
        patch.defrost = parseOnOff(msg);
        break;
      case 'tele/elec_heat':
        patch.elec = parseOnOff(msg);
        break;
      case 'tele/alarm':
        patch.alarm = parseOnOff(msg);
        break;
      case 'tele/porucha':
        patch.poruchaText = msg;
        break;
      case 'tele/watch':
        patch.watchActive = parseOnOff(msg);
        break;
      default:
        return;
    }

    this.onState({ type: 'tele', patch });
  }
}

function parseOnOff(v) {
  const s = String(v).trim().toUpperCase();
  return s === 'ON' || s === '1' || s === 'TRUE' || s === 'START';
}

function parseTemp(v) {
  if (!v || v === TELE_NA || v === '---' || v === 'off') {
    return null;
  }
  const n = Number.parseFloat(v);
  return Number.isFinite(n) ? n : null;
}
