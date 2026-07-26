#pragma once

#include <Arduino.h>

const char WEB_PAGE[] PROGMEM = R"HTML(
<!doctype html>
<html lang="pt-BR">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
  <meta name="theme-color" content="#f4f7f5">
  <title>Controle do LED</title>
  <style>
    :root {
      color-scheme: light;
      --bg: #f4f7f5;
      --card: #fff;
      --text: #17221c;
      --muted: #66736b;
      --line: #dfe7e2;
      --green: #22a957;
      --green-dark: #11773a;
      --green-soft: #e5f7eb;
      --level: 0%;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      min-height: 100vh;
      min-height: 100dvh;
      color: var(--text);
      background: var(--bg);
      font-family: Inter, ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
    }
    button, input { font: inherit; }
    button { -webkit-tap-highlight-color: transparent; }
    .page { width: min(100% - 40px, 1080px); margin: 0 auto; padding: 30px 0 40px; }
    header { display: flex; align-items: center; justify-content: space-between; gap: 16px; margin-bottom: 18px; }
    .brand { display: flex; align-items: center; gap: 11px; font-size: 17px; font-weight: 780; letter-spacing: -.02em; }
    .brand-icon { width: 38px; height: 38px; display: grid; place-items: center; border-radius: 12px; color: var(--green-dark); background: var(--green-soft); }
    .brand-icon svg { width: 20px; height: 20px; }
    .connection { display: flex; align-items: center; gap: 7px; color: var(--muted); font-size: 12px; font-weight: 650; }
    .connection-dot { width: 8px; height: 8px; border-radius: 50%; background: #a5afa9; }
    .connection.online .connection-dot { background: var(--green); box-shadow: 0 0 0 4px var(--green-soft); }
    .dashboard { display: grid; grid-template-columns: minmax(300px,5fr) minmax(390px,7fr); grid-template-rows: auto 1fr; gap: 16px; }
    .card { border: 1px solid var(--line); border-radius: 24px; background: var(--card); box-shadow: 0 14px 45px rgba(23, 50, 34, .07); }
    .main-card { grid-row: 1 / 3; min-height: 560px; padding: 32px; display: flex; flex-direction: column; }
    .status { display: contents; }
    .status-copy { min-width: 0; }
    .status-label { margin-bottom: 4px; color: var(--muted); font-size: 12px; font-weight: 650; }
    h1 { margin: 0; font-size: clamp(25px, 7vw, 34px); line-height: 1.1; letter-spacing: -.045em; }
    .led-stage { flex: 1; min-height: 260px; display: grid; place-items: center; }
    .led { flex: 0 0 auto; width: 150px; height: 150px; display: grid; place-items: center; border: 1px solid #dbe7df; border-radius: 50%; background: #edf2ef; transition: .25s ease; }
    .led::after { content: ""; width: 78px; height: 78px; border-radius: 50%; background: #aeb9b2; transition: .25s ease; }
    .led.on { border-color: #bce8ca; background: var(--green-soft); box-shadow: 0 0 calc(8px + var(--output, 0) * .25px) rgba(34,169,87,calc(.08 + var(--output, 0) * .003)); }
    .led.on::after { background: var(--green); opacity: calc(.35 + var(--output, 0) * .0065); box-shadow: inset -7px -8px 12px rgba(0,0,0,.12); }
    .power {
      width: 100%; min-height: 58px; border: 0; border-radius: 16px; color: #fff; background: var(--green);
      cursor: pointer; font-size: 15px; font-weight: 760; transition: .18s ease;
    }
    .power:hover { background: var(--green-dark); }
    .power.off { color: #35423a; background: #edf1ee; }
    .power:focus-visible, input[type="range"]:focus-visible { outline: 3px solid rgba(34,169,87,.28); outline-offset: 3px; }
    .divider { height: 1px; margin: 26px 0; background: var(--line); }
    .control-card { padding: 28px; }
    .control-heading { display: flex; align-items: end; justify-content: space-between; gap: 16px; margin-bottom: 18px; }
    .control-heading label { font-size: 15px; font-weight: 740; }
    .value { font-size: 28px; font-weight: 800; letter-spacing: -.05em; }
    .value small { color: var(--muted); font-size: 13px; letter-spacing: 0; }
    input[type="range"] { width: 100%; height: 30px; margin: 0; appearance: none; background: transparent; cursor: pointer; }
    input[type="range"]::-webkit-slider-runnable-track { height: 9px; border-radius: 99px; background: linear-gradient(90deg,var(--green) var(--level),#e4eae6 var(--level)); }
    input[type="range"]::-moz-range-track { height: 9px; border-radius: 99px; background: linear-gradient(90deg,var(--green) var(--level),#e4eae6 var(--level)); }
    input[type="range"]::-webkit-slider-thumb { width: 26px; height: 26px; margin-top: -8px; appearance: none; border: 5px solid #fff; border-radius: 50%; background: var(--green); box-shadow: 0 2px 9px rgba(0,0,0,.22); }
    input[type="range"]::-moz-range-thumb { width: 16px; height: 16px; border: 5px solid #fff; border-radius: 50%; background: var(--green); box-shadow: 0 2px 9px rgba(0,0,0,.22); }
    .range-labels { display: flex; justify-content: space-between; margin-top: 2px; color: #8b9690; font-size: 10px; }
    .source-row { display: flex; align-items: center; gap: 12px; margin-top: 25px; padding: 15px; border-radius: 14px; background: #f3f8f5; }
    .source-icon { flex: 0 0 auto; width: 35px; height: 35px; display: grid; place-items: center; border-radius: 11px; color: var(--green-dark); background: var(--green-soft); font-size: 16px; font-weight: 800; }
    .source-title { display: block; margin-bottom: 3px; font-size: 12px; font-weight: 760; }
    .source-help { display: block; color: var(--muted); font-size: 11px; line-height: 1.4; }
    details { border: 1px solid var(--line); border-radius: 18px; background: var(--card); overflow: hidden; box-shadow: 0 14px 45px rgba(23, 50, 34, .05); }
    summary { min-height: 56px; display: flex; align-items: center; justify-content: space-between; padding: 0 20px; cursor: pointer; font-size: 13px; font-weight: 720; list-style: none; }
    summary::-webkit-details-marker { display: none; }
    summary::after { content: "+"; color: var(--muted); font-size: 21px; font-weight: 400; }
    details[open] summary::after { content: "-"; }
    .details-body { padding: 2px 20px 20px; }
    .info-row { display: flex; justify-content: space-between; gap: 18px; padding: 11px 0; border-bottom: 1px solid #edf1ee; color: var(--muted); font-size: 12px; }
    .info-row strong { color: var(--text); font-weight: 680; text-align: right; overflow-wrap: anywhere; }
    .network-reset { width: 100%; min-height: 44px; margin-top: 17px; border: 1px solid var(--line); border-radius: 12px; color: #34463b; background: #f7faf8; cursor: pointer; font-size: 12px; font-weight: 720; }
    .firmware-panel { margin-top: 18px; padding: 16px; border-radius: 15px; background: #f3f8f5; }
    .firmware-title { display: block; margin-bottom: 8px; font-size: 12px; font-weight: 780; }
    .firmware-status { min-height: 18px; color: var(--muted); font-size: 11px; line-height: 1.5; }
    .ota-progress { width: 100%; height: 7px; margin-top: 10px; border: 0; border-radius: 99px; overflow: hidden; accent-color: var(--green); }
    .ota-progress[hidden] { display: none; }
    .ota-actions { display: grid; grid-template-columns: 1fr 1fr; gap: 9px; margin-top: 12px; }
    .ota-button { min-height: 42px; border: 1px solid #cfe1d5; border-radius: 11px; color: var(--green-dark); background: #fff; cursor: pointer; font-size: 11px; font-weight: 750; }
    .ota-button.primary { border-color: var(--green); color: #fff; background: var(--green); }
    .ota-button:disabled { cursor: wait; opacity: .55; }
    .ota-button[hidden] { display: none; }
    .danger-zone { margin-top: 20px; padding-top: 18px; border-top: 1px solid var(--line); }
    .danger-title { display: block; color: #9c3434; font-size: 12px; font-weight: 760; }
    .danger-help { display: block; margin-top: 4px; color: var(--muted); font-size: 11px; line-height: 1.45; }
    .factory-reset { width: 100%; min-height: 44px; margin-top: 12px; border: 1px solid #efc8c8; border-radius: 12px; color: #a03737; background: #fff8f8; cursor: pointer; font-size: 12px; font-weight: 740; }
    .network-reset:focus-visible, .factory-reset:focus-visible, .modal-button:focus-visible, .ota-button:focus-visible { outline: 3px solid rgba(34,169,87,.28); outline-offset: 3px; }
    .modal { position: fixed; z-index: 20; inset: 0; display: grid; place-items: center; padding: 20px; background: rgba(12,20,15,.58); backdrop-filter: blur(4px); }
    .modal[hidden] { display: none; }
    .modal-card { width: min(100%,430px); padding: 24px; border-radius: 20px; background: #fff; box-shadow: 0 24px 70px rgba(0,0,0,.28); }
    .modal-card h2 { margin: 0 0 8px; font-size: 21px; letter-spacing: -.03em; }
    .modal-card p { margin: 0; color: var(--muted); font-size: 13px; line-height: 1.5; }
    .modal-card ul { margin: 17px 0; padding: 15px 15px 15px 32px; border-radius: 12px; color: #653232; background: #fff5f5; font-size: 12px; line-height: 1.8; }
    .modal-note { font-size: 11px!important; }
    .modal-actions { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin-top: 20px; }
    .modal-button { min-height: 46px; border: 1px solid var(--line); border-radius: 12px; color: var(--text); background: #f6f8f7; cursor: pointer; font-size: 12px; font-weight: 740; }
    .modal-button.danger { border-color: #b84545; color: #fff; background: #b84545; }
    .modal-button:disabled { cursor: wait; opacity: .6; }
    .toast { position: fixed; left: 50%; bottom: 22px; max-width: calc(100% - 32px); padding: 12px 16px; border-radius: 12px; color: #fff; background: #24352b; box-shadow: 0 12px 30px rgba(0,0,0,.2); font-size: 12px; opacity: 0; pointer-events: none; transform: translate(-50%,15px); transition: .2s ease; }
    .toast.show { opacity: 1; transform: translate(-50%,0); }
    footer { padding-top: 18px; color: #8a958e; font-size: 10px; text-align: center; }
    @media (min-width: 900px) {
      summary { min-height: 50px; cursor: default; }
      summary::after { display: none; }
      .details-body { display: block!important; }
    }
    @media (max-width: 899px) {
      .page { width: min(100% - 32px,560px); padding-top: 24px; }
      .dashboard { grid-template-columns: 1fr; grid-template-rows: auto; }
      .main-card { grid-row: auto; min-height: auto; padding: 26px; display: block; }
      .status { display: flex; align-items: center; justify-content: space-between; gap: 22px; padding-bottom: 25px; }
      .led-stage { flex: 0 0 auto; display: block; min-height: 0; }
      .led { width: 78px; height: 78px; }
      .led::after { width: 40px; height: 40px; }
      .control-card { padding: 26px; }
      details { margin-top: 0; }
    }
    @media (max-width: 430px) {
      .page { width: min(100% - 22px,560px); padding-top: 16px; }
      .main-card, .control-card { padding: 21px; border-radius: 21px; }
      .led { width: 68px; height: 68px; }
      .led::after { width: 35px; height: 35px; }
    }
    @media (prefers-reduced-motion: reduce) { *,*::before,*::after { transition-duration: .01ms!important; } }
  </style>
</head>
<body>
  <div class="page">
    <header>
      <div class="brand"><span class="brand-icon" aria-hidden="true"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8"><path d="M9 18h6M10 22h4M8.2 14.5A7 7 0 1 1 15.8 14.5C14.7 15.3 14 16.1 14 18h-4c0-1.9-.7-2.7-1.8-3.5Z"/></svg></span>Controle do LED</div>
      <div class="connection" id="connection"><i class="connection-dot"></i><span id="connectionText">Conectando</span></div>
    </header>

    <main class="dashboard">
      <section class="card main-card">
        <div class="status">
          <div class="status-copy"><div class="status-label">Estado atual</div><h1 id="statusTitle">Carregando...</h1></div>
          <div class="led-stage"><div class="led" id="led" aria-hidden="true"></div></div>
        </div>
        <button class="power off" id="power" type="button" aria-pressed="false">Ligar LED</button>
      </section>

      <section class="card control-card">
        <div class="control-heading"><label for="brightness">Intensidade</label><span class="value"><span id="brightnessValue">0</span><small>%</small></span></div>
        <input id="brightness" type="range" min="0" max="100" value="0" aria-label="Intensidade do LED">
        <div class="range-labels"><span>Apagado</span><span>M&aacute;ximo</span></div>
        <div class="source-row">
          <span class="source-icon" id="sourceIcon">P</span>
          <div><span class="source-title" id="sourceTitle">Controle atual: potenci&ocirc;metro</span><span class="source-help">Mova o potenci&ocirc;metro ou ajuste o slider. O &uacute;ltimo usado assume o controle.</span></div>
        </div>
      </section>

      <details id="deviceDetails">
        <summary>Informa&ccedil;&otilde;es do dispositivo</summary>
        <div class="details-body">
        <div class="info-row"><span>Endere&ccedil;o</span><strong id="address">--</strong></div>
        <div class="info-row"><span>IP</span><strong id="ip">--</strong></div>
        <div class="info-row"><span>Sinal Wi-Fi</span><strong id="signal">--</strong></div>
        <div class="info-row"><span>Leitura do potenci&ocirc;metro</span><strong id="adc">--</strong></div>
        <div class="info-row"><span>Firmware instalado</span><strong id="firmwareCurrent">--</strong></div>
        <div class="firmware-panel">
          <span class="firmware-title">Atualiza&ccedil;&otilde;es do firmware</span>
          <div class="firmware-status" id="firmwareStatus">Aguardando consulta</div>
          <progress class="ota-progress" id="otaProgress" max="100" value="0" hidden></progress>
          <div class="ota-actions">
            <button class="ota-button" id="otaCheck" type="button">Verificar agora</button>
            <button class="ota-button primary" id="otaInstall" type="button" hidden>Atualizar agora</button>
          </div>
        </div>
        <button class="network-reset" id="wifiReset" type="button">Configurar outra rede Wi-Fi</button>
        <div class="danger-zone">
          <span class="danger-title">Restaura&ccedil;&atilde;o de f&aacute;brica</span>
          <span class="danger-help">Apaga a rede e todas as prefer&ecirc;ncias salvas.</span>
          <button class="factory-reset" id="factoryReset" type="button">Restaurar configura&ccedil;&otilde;es de f&aacute;brica</button>
        </div>
        </div>
      </details>
    </main>
    <footer>Controle local &middot; ESP32</footer>
  </div>
  <div class="toast" id="toast" role="status"></div>
  <div class="modal" id="factoryModal" role="dialog" aria-modal="true" aria-labelledby="factoryTitle" hidden>
    <div class="modal-card">
      <h2 id="factoryTitle">Restaurar configura&ccedil;&otilde;es?</h2>
      <p>Esta a&ccedil;&atilde;o apagar&aacute; permanentemente:</p>
      <ul><li>Rede e senha Wi-Fi</li><li>Estado e modo do LED</li><li>Intensidade manual salva</li></ul>
      <p class="modal-note">O firmware ser&aacute; mantido. Depois do rein&iacute;cio, conecte-se novamente &agrave; rede CONFIGURE-LED.</p>
      <div class="modal-actions">
        <button class="modal-button" id="factoryCancel" type="button">Cancelar</button>
        <button class="modal-button danger" id="factoryConfirm" type="button">Apagar e restaurar</button>
      </div>
    </div>
  </div>

  <script>
    const $ = id => document.getElementById(id);
    const ui = {
      connection: $('connection'), connectionText: $('connectionText'), status: $('statusTitle'), led: $('led'),
      power: $('power'), slider: $('brightness'), value: $('brightnessValue'),
      sourceIcon: $('sourceIcon'), sourceTitle: $('sourceTitle'), details: $('deviceDetails'),
      address: $('address'), ip: $('ip'), signal: $('signal'), adc: $('adc'),
      firmwareCurrent: $('firmwareCurrent'), firmwareStatus: $('firmwareStatus'),
      otaProgress: $('otaProgress'), otaCheck: $('otaCheck'), otaInstall: $('otaInstall'),
      reset: $('wifiReset'), factoryReset: $('factoryReset'), factoryModal: $('factoryModal'),
      factoryCancel: $('factoryCancel'), factoryConfirm: $('factoryConfirm'), toast: $('toast')
    };
    let state = null;
    let sequence = 0;
    let lastInteraction = 0;
    let sliderTimer = 0;
    let toastTimer = 0;
    let refreshing = false;

    function showToast(message) {
      ui.toast.textContent = message;
      ui.toast.classList.add('show');
      clearTimeout(toastTimer);
      toastTimer = setTimeout(() => ui.toast.classList.remove('show'), 3000);
    }
    function signalLabel(rssi) {
      if (!rssi) return '--';
      if (rssi >= -55) return 'Excelente';
      if (rssi >= -67) return 'Bom';
      if (rssi >= -75) return 'Regular';
      return 'Fraco';
    }
    function otaLabel() {
      const labels = {
        idle: 'Aguardando consulta automatica', checking: 'Consultando o GitHub...',
        up_to_date: 'Firmware atualizado', available: 'Versao ' + state.otaAvailableVersion + ' disponivel',
        downloading: 'Baixando firmware: ' + state.otaProgress + '%',
        verifying: 'Verificando integridade do firmware...', rebooting: 'Reiniciando com o novo firmware...',
        error: state.otaError || 'Falha ao consultar atualizacoes'
      };
      let label = labels[state.otaStatus] || labels.idle;
      if (state.otaLastCheck && !['checking','downloading','verifying','rebooting'].includes(state.otaStatus)) {
        label += ' · ' + new Date(state.otaLastCheck * 1000).toLocaleString('pt-BR');
      }
      return label;
    }
    function render() {
      if (!state) return;
      const usePot = state.mode === 'potentiometer';
      const shownValue = usePot ? state.potentiometerBrightness : state.manualBrightness;
      ui.status.textContent = state.power ? 'Ligado em ' + state.outputBrightness + '%' : 'Desligado';
      ui.led.classList.toggle('on', state.power);
      ui.led.style.setProperty('--output', state.outputBrightness);
      ui.power.classList.toggle('off', state.power);
      ui.power.textContent = state.power ? 'Desligar LED' : 'Ligar LED';
      ui.power.setAttribute('aria-pressed', state.power);
      ui.slider.value = shownValue;
      ui.slider.style.setProperty('--level', shownValue + '%');
      ui.value.textContent = shownValue;
      ui.sourceIcon.textContent = usePot ? 'P' : 'W';
      ui.sourceTitle.textContent = 'Controle atual: ' + (usePot ? 'potenciometro' : 'interface web');
      ui.address.textContent = state.hostname;
      ui.ip.textContent = state.ip;
      ui.signal.textContent = signalLabel(state.rssi) + (state.rssi ? ' (' + state.rssi + ' dBm)' : '');
      ui.adc.textContent = state.potentiometerAdc + ' / 4095';
      ui.firmwareCurrent.textContent = state.firmwareVersion + ' (' + state.firmwareBuild + ')';
      ui.firmwareStatus.textContent = otaLabel();
      const otaBusy = ['checking','downloading','verifying','rebooting'].includes(state.otaStatus);
      const showProgress = ['downloading','verifying'].includes(state.otaStatus);
      ui.otaProgress.hidden = !showProgress;
      ui.otaProgress.value = state.otaProgress;
      ui.otaCheck.disabled = otaBusy;
      ui.otaInstall.hidden = !state.otaUpdateAvailable;
      ui.otaInstall.disabled = otaBusy;
      ui.connection.classList.toggle('online', state.wifiConnected);
      ui.connectionText.textContent = state.wifiConnected ? 'Online' : 'Sem conexao';
    }
    async function refresh() {
      if (refreshing || Date.now() - lastInteraction < 500) return;
      refreshing = true;
      const controller = new AbortController();
      const timeout = setTimeout(() => controller.abort(), 3000);
      try {
        const response = await fetch('/api/state', { cache: 'no-store', signal: controller.signal });
        if (!response.ok) throw new Error();
        state = await response.json();
        render();
      } catch (_) {
        ui.connection.classList.remove('online');
        ui.connectionText.textContent = 'Reconectando';
      } finally {
        clearTimeout(timeout);
        refreshing = false;
      }
    }
    async function control(values) {
      lastInteraction = Date.now();
      const current = ++sequence;
      try {
        const response = await fetch('/api/control', {
          method: 'POST', headers: {
            'Content-Type': 'application/x-www-form-urlencoded',
            'X-CSRF-Token': state.csrfToken
          },
          body: new URLSearchParams(values)
        });
        const result = await response.json();
        if (!response.ok) throw new Error(result.error || 'Comando rejeitado');
        if (current === sequence) { state = result; render(); }
      } catch (error) {
        showToast(error.message || 'Nao foi possivel falar com o ESP32');
        refresh();
      }
    }
    ui.power.addEventListener('click', () => {
      if (!state) return;
      clearTimeout(sliderTimer);
      state.power = !state.power;
      state.outputBrightness = state.power ? (state.mode === 'potentiometer' ? state.potentiometerBrightness : state.manualBrightness) : 0;
      render();
      control({ power: state.power });
    });
    ui.slider.addEventListener('input', () => {
      if (!state) return;
      const value = Number(ui.slider.value);
      state.manualBrightness = value;
      state.mode = 'web';
      state.power = true;
      state.outputBrightness = value;
      render();
      lastInteraction = Date.now();
      clearTimeout(sliderTimer);
      sliderTimer = setTimeout(() => control({ brightness: value }), 90);
    });
    ui.reset.addEventListener('click', async () => {
      if (!confirm('Apagar a rede salva e configurar outra rede Wi-Fi?')) return;
      try {
        const response = await fetch('/api/wifi/reset', {
          method: 'POST', headers: { 'X-CSRF-Token': state.csrfToken }
        });
        const result = await response.json();
        if (!response.ok) throw new Error(result.error || 'Falha ao reconfigurar o Wi-Fi');
        showToast('Reiniciando. Conecte-se a rede CONFIGURE-LED.');
      } catch (error) {
        showToast(error.message || 'O ESP32 esta reiniciando.');
      }
    });
    async function otaAction(path) {
      try {
        const response = await fetch(path, {
          method: 'POST', headers: { 'X-CSRF-Token': state.csrfToken }
        });
        const result = await response.json();
        if (!response.ok) throw new Error(result.error || 'Operacao OTA rejeitada');
        showToast(result.message);
        await refresh();
      } catch (error) {
        showToast(error.message || 'Nao foi possivel iniciar a operacao OTA');
      }
    }
    ui.otaCheck.addEventListener('click', () => otaAction('/api/ota/check'));
    ui.otaInstall.addEventListener('click', () => {
      if (!state || !state.otaUpdateAvailable) return;
      if (!confirm('Instalar o firmware ' + state.otaAvailableVersion + '? O ESP32 sera reiniciado.')) return;
      otaAction('/api/ota/install');
    });
    ui.factoryReset.addEventListener('click', () => {
      ui.factoryModal.hidden = false;
      ui.factoryCancel.focus();
    });
    ui.factoryCancel.addEventListener('click', () => {
      ui.factoryModal.hidden = true;
      ui.factoryReset.focus();
    });
    ui.factoryModal.addEventListener('click', event => {
      if (event.target === ui.factoryModal) ui.factoryCancel.click();
    });
    document.addEventListener('keydown', event => {
      if (event.key === 'Escape' && !ui.factoryModal.hidden) ui.factoryCancel.click();
    });
    ui.factoryConfirm.addEventListener('click', async () => {
      ui.factoryConfirm.disabled = true;
      ui.factoryCancel.disabled = true;
      try {
        const response = await fetch('/api/factory-reset', {
          method: 'POST', headers: { 'X-CSRF-Token': state.csrfToken }
        });
        const result = await response.json();
        if (!response.ok) throw new Error(result.error || 'Falha ao restaurar');
        if (state) { state.power = false; state.outputBrightness = 0; render(); }
        ui.factoryModal.hidden = true;
        showToast('Dados apagados. O ESP32 esta reiniciando.');
      } catch (error) {
        showToast(error.message || 'Nao foi possivel restaurar');
        ui.factoryConfirm.disabled = false;
        ui.factoryCancel.disabled = false;
      }
    });
    const desktopQuery = matchMedia('(min-width: 900px)');
    function syncDeviceDetails(event) { ui.details.open = event.matches; }
    syncDeviceDetails(desktopQuery);
    if (desktopQuery.addEventListener) desktopQuery.addEventListener('change', syncDeviceDetails);
    else desktopQuery.addListener(syncDeviceDetails);
    refresh();
    setInterval(refresh, 1000);
  </script>
</body>
</html>
)HTML";
