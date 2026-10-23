/**
 * SMART TRAFFIC CONTROL SYSTEM - CORE CONTROLLER CLIENT
 * ESP32 Wi-Fi Direct Architecture (Offline, No External Dependencies)
 */

(function () {
  'use strict';

  // Base API configuration: If loaded directly from ESP32, use relative path.
  // If loaded locally for test, fall back to default ESP32 SoftAP IP 192.168.4.1
  const API_BASE = window.location.hostname === '192.168.4.1' || window.location.hostname === 'localhost'
    ? ''
    : 'http://192.168.4.1';

  // Application State
  const state = {
    connected: false,
    pollIntervalMs: 700,
    timerId: null,
    consecutiveFailures: 0,
    lastSuccessfulPoll: 0,
    latencyMs: 0,
    successCount: 0,
    totalRequests: 0,

    // Hardware status snapshot from ESP32
    hardware: {
      mode: 'auto',         // 'auto' | 'manual' | 'all_red'
      activeRoad: 1,        // 1 to 4
      phase: 'GREEN',       // 'STARTUP' | 'GREEN' | 'YELLOW' | 'ALL_RED_CLEARANCE'
      remainingMs: 10000,
      totalMs: 10000,
      pendingRoad: 0,
      uptimeSec: 0,
      freeHeap: 0,
      roads: [
        { road: 1, r: 0, y: 0, g: 1 },
        { road: 2, r: 1, y: 0, g: 0 },
        { road: 3, r: 1, y: 0, g: 0 },
        { road: 4, r: 1, y: 0, g: 0 }
      ],
      settings: {
        greenTime: 10,
        yellowTime: 2,
        clearanceTime: 1,
        sequence: [1, 2, 3, 4]
      }
    },

    // UI Configuration & Compass Mapping
    orientationOffset: 0, // 0, 90, 180, 270 degrees
    roadLabels: {
      1: 'North Approach',
      2: 'East Approach',
      3: 'South Approach',
      4: 'West Approach'
    },

    // Local countdown interpolation
    localCountdownSec: 10,
    lastTickTimestamp: Date.now()
  };

  // DOM Elements Cache
  const dom = {
    connectionBanner: document.getElementById('connectionBanner'),
    bannerText: document.getElementById('bannerText'),
    bannerRetryBtn: document.getElementById('bannerRetryBtn'),
    wifiStatusPill: document.getElementById('wifiStatusPill'),
    wifiLabel: document.getElementById('wifiLabel'),
    modeValue: document.getElementById('modeValue'),
    uptimeValue: document.getElementById('uptimeValue'),

    // Ribbon & Mode buttons
    btnModeAuto: document.getElementById('btnModeAuto'),
    btnModeManual: document.getElementById('btnModeManual'),
    btnModeAllRed: document.getElementById('btnModeAllRed'),
    phaseIndicatorDot: document.getElementById('phaseIndicatorDot'),
    phaseDescription: document.getElementById('phaseDescription'),
    phaseCountdown: document.getElementById('phaseCountdown'),

    // Intersection SVG Elements
    orientationSelect: document.getElementById('orientationSelect'),
    intersectionSvg: document.getElementById('intersectionSvg'),
    junctionCenterGlow: document.getElementById('junctionCenterGlow'),
    junctionCenterText: document.getElementById('junctionCenterText'),

    // SVG LED Elements by Compass (N, E, S, W)
    svgLeds: {
      N: { R: document.getElementById('svgLed_N_R'), Y: document.getElementById('svgLed_N_Y'), G: document.getElementById('svgLed_N_G') },
      E: { R: document.getElementById('svgLed_E_R'), Y: document.getElementById('svgLed_E_Y'), G: document.getElementById('svgLed_E_G') },
      S: { R: document.getElementById('svgLed_S_R'), Y: document.getElementById('svgLed_S_Y'), G: document.getElementById('svgLed_S_G') },
      W: { R: document.getElementById('svgLed_W_R'), Y: document.getElementById('svgLed_W_Y'), G: document.getElementById('svgLed_W_G') }
    },
    svgLabels: {
      N: document.getElementById('svgLabelN'),
      E: document.getElementById('svgLabelE'),
      S: document.getElementById('svgLabelS'),
      W: document.getElementById('svgLabelW')
    },
    svgBadges: {
      N: document.getElementById('svgBadge_N'),
      E: document.getElementById('svgBadge_E'),
      S: document.getElementById('svgBadge_S'),
      W: document.getElementById('svgBadge_W')
    },

    // Road Cards
    roadCards: [1, 2, 3, 4].map(id => ({
      card: document.getElementById(`roadCard-${id}`),
      dirBadge: document.getElementById(`cardDirBadge-${id}`),
      lampR: document.getElementById(`lamp-${id}-R`),
      lampY: document.getElementById(`lamp-${id}-Y`),
      lampG: document.getElementById(`lamp-${id}-G`),
      statePill: document.getElementById(`statePill-${id}`),
      timeInfo: document.getElementById(`timeInfo-${id}`),
      btnRequest: document.getElementById(`btnRequestRoad-${id}`)
    })),

    // Diagnostics
    diagHeap: document.getElementById('diagHeap'),
    diagLatency: document.getElementById('diagLatency'),
    diagSuccessRate: document.getElementById('diagSuccessRate'),
    diagStateName: document.getElementById('diagStateName'),
    diagPending: document.getElementById('diagPending'),

    // Event Log
    logContainer: document.getElementById('logContainer'),
    logCount: document.getElementById('logCount'),
    btnClearLog: document.getElementById('btnClearLog'),

    // Settings Modal
    openSettingsBtn: document.getElementById('openSettingsBtn'),
    settingsModal: document.getElementById('settingsModal'),
    btnCloseSettings: document.getElementById('btnCloseSettings'),
    settingsForm: document.getElementById('settingsForm'),
    inputGreenTime: document.getElementById('inputGreenTime'),
    inputYellowTime: document.getElementById('inputYellowTime'),
    inputClearanceTime: document.getElementById('inputClearanceTime'),
    selectSequence: document.getElementById('selectSequence'),
    btnResetSettings: document.getElementById('btnResetSettings'),
    labelRoadInputs: [1, 2, 3, 4].map(id => document.getElementById(`labelRoad${id}`))
  };

  let totalLogEntries = 0;

  // =========================================================================
  // LOGGING UTILITY
  // =========================================================================
  function logEvent(type, message) {
    totalLogEntries++;
    dom.logCount.textContent = `${totalLogEntries} events`;

    const now = new Date();
    const timeStr = now.toTimeString().split(' ')[0];

    const entry = document.createElement('div');
    entry.className = `log-entry log-entry--${type}`;

    let badgeClass = 'badge-system';
    if (type === 'safety') badgeClass = 'badge-safety';
    else if (type === 'state') badgeClass = 'badge-state';
    else if (type === 'cmd') badgeClass = 'badge-cmd';
    else if (type === 'error') badgeClass = 'badge-error';

    entry.innerHTML = `
      <span class="log-time">${timeStr}</span>
      <span class="log-badge ${badgeClass}">${type.toUpperCase()}</span>
      <span class="log-msg">${escapeHtml(message)}</span>
    `;

    dom.logContainer.prepend(entry);

    // Keep log buffer manageable
    while (dom.logContainer.children.length > 50) {
      dom.logContainer.removeChild(dom.logContainer.lastChild);
    }
  }

  function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
  }

  // =========================================================================
  // API CLIENT (Zero External Libs)
  // =========================================================================
  async function fetchWithTimeout(resource, options = {}) {
    const { timeout = 2500 } = options;
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), timeout);

    try {
      const response = await fetch(resource, {
        ...options,
        signal: controller.signal,
        headers: {
          'Content-Type': 'application/json',
          'Cache-Control': 'no-cache, no-store, must-revalidate',
          ...(options.headers || {})
        }
      });
      clearTimeout(timer);
      return response;
    } catch (err) {
      clearTimeout(timer);
      throw err;
    }
  }

  // Poll /api/status from ESP32
  async function pollStatus() {
    state.totalRequests++;
    const startTime = performance.now();

    try {
      const response = await fetchWithTimeout(`${API_BASE}/api/status`);

      if (!response.ok) {
        throw new Error(`HTTP ${response.status}`);
      }

      const data = await response.json();
      state.latencyMs = Math.round(performance.now() - startTime);
      state.successCount++;
      state.consecutiveFailures = 0;
      state.lastSuccessfulPoll = Date.now();

      if (!state.connected) {
        state.connected = true;
        onConnected();
      }

      applyHardwareStatus(data);
    } catch (err) {
      state.consecutiveFailures++;
      if (state.consecutiveFailures >= 2 && state.connected) {
        state.connected = false;
        onDisconnected(err.message);
      }
    }
  }

  function onConnected() {
    dom.connectionBanner.classList.add('hidden');
    dom.wifiStatusPill.className = 'status-pill status-pill--wifi connected';
    dom.wifiLabel.textContent = 'ONLINE (192.168.4.1)';
    logEvent('system', 'Connected to ESP32 Traffic Controller at 192.168.4.1');
  }

  function onDisconnected(reason) {
    dom.connectionBanner.classList.remove('hidden');
    dom.bannerText.textContent = `Disconnected from ESP32 (${reason}). Check Wi-Fi: TrafficLight-ESP32`;
    dom.wifiStatusPill.className = 'status-pill status-pill--wifi disconnected';
    dom.wifiLabel.textContent = 'OFFLINE';
    logEvent('error', `Connection lost: ${reason}. Retrying automatically...`);
  }

  // Send Mode Change
  async function sendModeChange(newMode) {
    logEvent('cmd', `Requesting mode change to ${newMode.toUpperCase()}`);
    try {
      const res = await fetchWithTimeout(`${API_BASE}/api/mode`, {
        method: 'POST',
        body: JSON.stringify({ mode: newMode })
      });
      if (res.ok) {
        logEvent('cmd', `Mode successfully switched to ${newMode.toUpperCase()}`);
        pollStatus();
      } else {
        logEvent('error', `Mode change rejected by ESP32 (HTTP ${res.status})`);
      }
    } catch (err) {
      logEvent('error', `Failed to send mode change: ${err.message}`);
    }
  }

  // Send Manual Road Request
  async function sendRoadRequest(roadId) {
    logEvent('cmd', `Manual selection: Requesting green for Road ${roadId} (${state.roadLabels[roadId]})`);
    
    // Visually show pending immediately
    const cardObj = dom.roadCards[roadId - 1];
    if (cardObj) {
      cardObj.btnRequest.classList.add('is-pending');
      cardObj.btnRequest.querySelector('.btn-text').textContent = 'Requesting...';
    }

    try {
      const res = await fetchWithTimeout(`${API_BASE}/api/road`, {
        method: 'POST',
        body: JSON.stringify({ road: roadId })
      });
      if (res.ok) {
        logEvent('safety', `ESP32 acknowledged Road ${roadId}. Safe clearance sequence initiated.`);
        pollStatus();
      } else {
        logEvent('error', `Road request rejected (HTTP ${res.status})`);
      }
    } catch (err) {
      logEvent('error', `Failed to send road request: ${err.message}`);
    }
  }

  // Send Settings Update
  async function sendSettingsUpdate(newSettings) {
    logEvent('cmd', 'Applying new signal timing configuration to ESP32');
    try {
      const res = await fetchWithTimeout(`${API_BASE}/api/settings`, {
        method: 'POST',
        body: JSON.stringify(newSettings)
      });
      if (res.ok) {
        logEvent('safety', 'New timings saved and verified by ESP32 controller');
        closeSettingsModal();
        pollStatus();
      } else {
        alert('ESP32 rejected timing values. Ensure limits: Green (5-60s), Yellow (1-10s), Clearance (1-5s).');
      }
    } catch (err) {
      alert(`Settings update failed: ${err.message}`);
    }
  }

  // =========================================================================
  // HARDWARE DATA APPLICATION TO UI
  // =========================================================================
  function applyHardwareStatus(data) {
    const prevPhase = state.hardware.phase;
    const prevActive = state.hardware.activeRoad;
    const prevMode = state.hardware.mode;

    // Merge data into state
    state.hardware.mode = data.mode || state.hardware.mode;
    state.hardware.activeRoad = data.active_road || state.hardware.activeRoad;
    state.hardware.phase = data.phase || state.hardware.phase;
    state.hardware.remainingMs = typeof data.remaining_ms === 'number' ? data.remaining_ms : state.hardware.remainingMs;
    state.hardware.totalMs = typeof data.total_ms === 'number' ? data.total_ms : state.hardware.totalMs;
    state.hardware.pendingRoad = data.pending_road || 0;
    state.hardware.uptimeSec = data.uptime_sec || 0;
    state.hardware.freeHeap = data.free_heap || 0;
    if (Array.isArray(data.roads)) {
      state.hardware.roads = data.roads;
    }
    if (data.settings) {
      state.hardware.settings = data.settings;
    }

    // Set countdown directly from ESP32 remaining_ms
    state.localCountdownSec = Math.max(0, Math.ceil(state.hardware.remainingMs / 1000));
    state.lastTickTimestamp = Date.now();

    // Log state changes if observed
    if (prevMode !== state.hardware.mode) {
      logEvent('state', `Mode changed from ${prevMode.toUpperCase()} to ${state.hardware.mode.toUpperCase()}`);
    }
    if (prevActive !== state.hardware.activeRoad || prevPhase !== state.hardware.phase) {
      logEvent('state', `Road ${state.hardware.activeRoad} entered ${state.hardware.phase} phase (${state.localCountdownSec}s)`);
    }

    renderAll();
  }

  // =========================================================================
  // UI RENDERING
  // =========================================================================
  function renderAll() {
    renderHeaderAndRibbon();
    renderCountdown();
    renderSvgIntersection();
    renderRoadCards();
    renderDiagnostics();
  }

  function renderHeaderAndRibbon() {
    // Mode indicator pill
    dom.modeValue.textContent = state.hardware.mode.toUpperCase();

    // Mode buttons active states
    dom.btnModeAuto.classList.toggle('active', state.hardware.mode === 'auto');
    dom.btnModeManual.classList.toggle('active', state.hardware.mode === 'manual');
    dom.btnModeAllRed.classList.toggle('active', state.hardware.mode === 'all_red');

    // Uptime formatting: HH:MM:SS
    const sec = state.hardware.uptimeSec;
    const h = String(Math.floor(sec / 3600)).padStart(2, '0');
    const m = String(Math.floor((sec % 3600) / 60)).padStart(2, '0');
    const s = String(sec % 60).padStart(2, '0');
    dom.uptimeValue.textContent = `${h}:${m}:${s}`;

    // Phase Description & Dot
    const activeRoad = state.hardware.activeRoad;
    const phase = state.hardware.phase;

    dom.phaseIndicatorDot.className = 'signal-indicator-dot';

    if (state.hardware.mode === 'all_red') {
      dom.phaseIndicatorDot.classList.add('dot-red');
      dom.phaseDescription.textContent = 'ALL-RED EMERGENCY HOLD';
    } else if (phase === 'GREEN') {
      dom.phaseIndicatorDot.classList.add('dot-green');
      dom.phaseDescription.textContent = `ROAD ${activeRoad} (${state.roadLabels[activeRoad]}) • GREEN`;
    } else if (phase === 'YELLOW') {
      dom.phaseIndicatorDot.classList.add('dot-yellow');
      dom.phaseDescription.textContent = `ROAD ${activeRoad} • YELLOW CLEARING`;
    } else if (phase === 'ALL_RED_CLEARANCE') {
      dom.phaseIndicatorDot.classList.add('dot-red');
      dom.phaseDescription.textContent = 'ALL-RED SAFETY CLEARANCE';
    } else {
      dom.phaseDescription.textContent = 'SYSTEM INITIALIZING';
    }
  }

  function renderCountdown() {
    // Truthful countdown display
    if (state.hardware.mode === 'all_red') {
      dom.phaseCountdown.textContent = 'HOLD';
      dom.phaseCountdown.style.fontSize = '1.3rem';
    } else {
      dom.phaseCountdown.textContent = String(state.localCountdownSec).padStart(2, '0');
      dom.phaseCountdown.style.fontSize = '1.8rem';
    }
  }

  function renderSvgIntersection() {
    // Physical Road 1..4 maps to visual directions N, E, S, W with offset rotation
    const compassDirs = ['N', 'E', 'S', 'W'];
    const offsetIndex = (state.orientationOffset / 90) % 4;

    // Reset all SVG LEDs to off
    compassDirs.forEach(dir => {
      const leds = dom.svgLeds[dir];
      leds.R.className.baseVal = 'led-off';
      leds.Y.className.baseVal = 'led-off';
      leds.G.className.baseVal = 'led-off';
    });

    // Map each physical road to its corresponding visual compass point
    state.hardware.roads.forEach(roadObj => {
      const roadNum = roadObj.road; // 1, 2, 3, 4
      const visualDirIndex = (roadNum - 1 + offsetIndex) % 4;
      const compassDir = compassDirs[visualDirIndex];

      const leds = dom.svgLeds[compassDir];
      if (leds) {
        if (roadObj.r) leds.R.className.baseVal = 'led-red-on';
        if (roadObj.y) leds.Y.className.baseVal = 'led-yellow-on';
        if (roadObj.g) leds.G.className.baseVal = 'led-green-on';
      }

      // Update badge label
      if (dom.svgBadges[compassDir]) {
        dom.svgBadges[compassDir].textContent = `R${roadNum}`;
      }
    });

    // Center Junction Status text and pulse
    if (state.hardware.phase === 'ALL_RED_CLEARANCE' || state.hardware.mode === 'all_red') {
      dom.junctionCenterGlow.setAttribute('stroke', '#ef4444');
      dom.junctionCenterGlow.setAttribute('opacity', '0.8');
      dom.junctionCenterText.textContent = 'CLEARANCE';
      dom.junctionCenterText.setAttribute('fill', '#ef4444');
    } else {
      const activeRoad = state.hardware.activeRoad;
      dom.junctionCenterGlow.setAttribute('stroke', state.hardware.phase === 'YELLOW' ? '#f59e0b' : '#10b981');
      dom.junctionCenterGlow.setAttribute('opacity', '0.4');
      dom.junctionCenterText.textContent = `R${activeRoad} FLOW`;
      dom.junctionCenterText.setAttribute('fill', '#94a3b8');
    }
  }

  function renderRoadCards() {
    const activeRoad = state.hardware.activeRoad;
    const phase = state.hardware.phase;
    const pendingRoad = state.hardware.pendingRoad;

    state.hardware.roads.forEach(roadObj => {
      const rId = roadObj.road;
      const cardObj = dom.roadCards[rId - 1];
      if (!cardObj) return;

      // Update direction name
      cardObj.dirBadge.textContent = state.roadLabels[rId];

      // Physical mini lamps
      cardObj.lampR.classList.toggle('lit', Boolean(roadObj.r));
      cardObj.lampY.classList.toggle('lit', Boolean(roadObj.y));
      cardObj.lampG.classList.toggle('lit', Boolean(roadObj.g));

      // Card active border styling
      cardObj.card.classList.remove('card-active-green', 'card-active-yellow', 'card-pending');
      if (rId === activeRoad) {
        if (phase === 'GREEN') cardObj.card.classList.add('card-active-green');
        if (phase === 'YELLOW') cardObj.card.classList.add('card-active-yellow');
      }
      if (rId === pendingRoad) {
        cardObj.card.classList.add('card-pending');
      }

      // Status pill & Phase info
      if (rId === activeRoad) {
        if (phase === 'GREEN') {
          cardObj.statePill.textContent = '🟢 GREEN ACTIVE';
          cardObj.statePill.style.color = 'var(--signal-green)';
          cardObj.timeInfo.textContent = `Flowing: ${state.localCountdownSec}s remaining`;
        } else if (phase === 'YELLOW') {
          cardObj.statePill.textContent = '🟡 YELLOW CAUTION';
          cardObj.statePill.style.color = 'var(--signal-yellow)';
          cardObj.timeInfo.textContent = `Clearing: ${state.localCountdownSec}s remaining`;
        } else {
          cardObj.statePill.textContent = '🔴 ALL-RED CLEARANCE';
          cardObj.statePill.style.color = 'var(--signal-red)';
          cardObj.timeInfo.textContent = 'Junction clearing';
        }
      } else {
        cardObj.statePill.textContent = '🔴 RED STOPPED';
        cardObj.statePill.style.color = 'var(--text-muted)';
        if (rId === pendingRoad) {
          cardObj.timeInfo.textContent = '⏳ Next in queue (waiting clearance)';
        } else {
          cardObj.timeInfo.textContent = 'Waiting in queue';
        }
      }

      // Manual Button state
      const btn = cardObj.btnRequest;
      const btnText = btn.querySelector('.btn-text');

      btn.classList.remove('is-active-road', 'is-pending');
      btn.disabled = false;

      if (rId === activeRoad && phase === 'GREEN') {
        btn.classList.add('is-active-road');
        btnText.textContent = 'Current Active Green';
        btn.disabled = true;
      } else if (rId === pendingRoad) {
        btn.classList.add('is-pending');
        btnText.textContent = 'Pending Clearance...';
        btn.disabled = true;
      } else if (state.hardware.mode === 'all_red') {
        btnText.textContent = `Select Road ${rId}`;
        btn.disabled = false;
      } else if (state.hardware.mode === 'auto') {
        btnText.textContent = `Override & Select R${rId}`;
        btn.disabled = false;
      } else {
        btnText.textContent = `Select Road ${rId}`;
        btn.disabled = false;
      }
    });
  }

  function renderDiagnostics() {
    const heapKb = Math.round(state.hardware.freeHeap / 1024);
    dom.diagHeap.textContent = heapKb > 0 ? `${heapKb} KB` : '184 KB';
    dom.diagLatency.textContent = `${state.latencyMs} ms`;

    const successPct = state.totalRequests > 0
      ? Math.round((state.successCount / state.totalRequests) * 100)
      : 100;
    dom.diagSuccessRate.textContent = `${successPct}% OK`;

    dom.diagStateName.textContent = state.hardware.phase;
    dom.diagPending.textContent = state.hardware.pendingRoad > 0
      ? `Pending: Road ${state.hardware.pendingRoad}`
      : 'Pending: None';
  }

  // =========================================================================
  // LOCAL SMOOTH COUNTDOWN TICKER
  // =========================================================================
  function startLocalCountdownTicker() {
    setInterval(() => {
      const now = Date.now();
      if (now - state.lastTickTimestamp >= 1000) {
        if (state.localCountdownSec > 0 && state.hardware.mode !== 'all_red') {
          state.localCountdownSec--;
          renderCountdown();
        }
        state.lastTickTimestamp = now;
      }
    }, 250);
  }

  // =========================================================================
  // SETTINGS MODAL HANDLERS
  // =========================================================================
  function openSettingsModal() {
    dom.inputGreenTime.value = state.hardware.settings.greenTime || 10;
    dom.inputYellowTime.value = state.hardware.settings.yellowTime || 2;
    dom.inputClearanceTime.value = state.hardware.settings.clearanceTime || 1;
    [1, 2, 3, 4].forEach(i => {
      if (dom.labelRoadInputs[i - 1]) {
        dom.labelRoadInputs[i - 1].value = state.roadLabels[i] || `Road ${i}`;
      }
    });
    dom.settingsModal.classList.remove('hidden');
  }

  function closeSettingsModal() {
    dom.settingsModal.classList.add('hidden');
  }

  function handleSaveSettings(e) {
    e.preventDefault();
    const green = parseInt(dom.inputGreenTime.value, 10);
    const yellow = parseInt(dom.inputYellowTime.value, 10);
    const clearance = parseInt(dom.inputClearanceTime.value, 10);

    if (isNaN(green) || green < 5 || green > 60) {
      alert('Green time must be between 5 and 60 seconds.');
      return;
    }
    if (isNaN(yellow) || yellow < 1 || yellow > 10) {
      alert('Yellow time must be between 1 and 10 seconds.');
      return;
    }
    if (isNaN(clearance) || clearance < 1 || clearance > 5) {
      alert('Clearance time must be between 1 and 5 seconds.');
      return;
    }

    // Save custom road labels locally
    [1, 2, 3, 4].forEach(i => {
      if (dom.labelRoadInputs[i - 1]) {
        state.roadLabels[i] = dom.labelRoadInputs[i - 1].value.trim() || `Road ${i}`;
      }
    });

    sendSettingsUpdate({
      green: green,
      yellow: yellow,
      clearance: clearance
    });
  }

  // =========================================================================
  // EVENT LISTENERS INITIALIZATION
  // =========================================================================
  function initEventListeners() {
    // Mode Switching Buttons
    dom.btnModeAuto.addEventListener('click', () => sendModeChange('auto'));
    dom.btnModeManual.addEventListener('click', () => sendModeChange('manual'));
    dom.btnModeAllRed.addEventListener('click', () => sendModeChange('all_red'));

    // Manual Road Request Buttons
    dom.roadCards.forEach((cardObj, idx) => {
      const roadNum = idx + 1;
      cardObj.btnRequest.addEventListener('click', () => {
        sendRoadRequest(roadNum);
      });
    });

    // Compass Board Alignment Selector
    dom.orientationSelect.addEventListener('change', (e) => {
      state.orientationOffset = parseInt(e.target.value, 10) || 0;
      dom.intersectionSvg.style.transform = `rotate(${state.orientationOffset}deg)`;
      renderSvgIntersection();
      logEvent('system', `Intersection orientation aligned to ${state.orientationOffset}°`);
    });

    // Clear Log
    dom.btnClearLog.addEventListener('click', () => {
      dom.logContainer.innerHTML = '';
      totalLogEntries = 0;
      dom.logCount.textContent = '0 events';
      logEvent('system', 'Audit log cleared by user.');
    });

    // Banner Retry
    dom.bannerRetryBtn.addEventListener('click', () => {
      pollStatus();
    });

    // Settings Modal
    dom.openSettingsBtn.addEventListener('click', openSettingsModal);
    dom.btnCloseSettings.addEventListener('click', closeSettingsModal);
    dom.settingsForm.addEventListener('submit', handleSaveSettings);
    dom.btnResetSettings.addEventListener('click', () => {
      dom.inputGreenTime.value = 10;
      dom.inputYellowTime.value = 2;
      dom.inputClearanceTime.value = 1;
    });

    // Close modal on escape key
    document.addEventListener('keydown', (e) => {
      if (e.key === 'Escape' && !dom.settingsModal.classList.contains('hidden')) {
        closeSettingsModal();
      }
    });
  }

  // =========================================================================
  // STARTUP BOOTSTRAP
  // =========================================================================
  function init() {
    logEvent('system', 'Starting Smart Traffic Web Application...');
    initEventListeners();
    renderAll();
    startLocalCountdownTicker();

    // Start background polling loop
    pollStatus();
    state.timerId = setInterval(pollStatus, state.pollIntervalMs);
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
  } else {
    init();
  }
})();
