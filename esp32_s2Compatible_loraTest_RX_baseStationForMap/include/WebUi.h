#pragma once

#include <Arduino.h>

const char WEB_UI[] PROGMEM = R"SIDEWALK_HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="color-scheme" content="dark">
<title>Sidewalk receiver</title>
<style>
:root {
  color-scheme: dark;
  --bg: #0b1116;
  --panel: #121b23;
  --line: #26343e;
  --text: #e7eef2;
  --muted: #94a8b6;
  --green: #8ae7b6;
  --amber: #f2c777;
  --red: #ff9b96;
  --mono: ui-monospace,"Cascadia Code",Consolas,monospace;
  --sans: system-ui,-apple-system,"Segoe UI",sans-serif;
}

* {
  box-sizing: border-box;
}

body {
  margin: 0;
  background: var(--bg);
  color: var(--text);
  font: 15px/1.5 var(--sans);
}

button,input {
  font: inherit;
}

button,input,a {
  touch-action: manipulation;
}

button {
  cursor: pointer;
}

button:disabled {
  opacity: .42;
  cursor: default;
}

button:focus-visible,input:focus-visible,summary:focus-visible {
  outline: 2px solid var(--green);
  outline-offset: 4px;
}

main {
  max-width: 1320px;
  margin: auto;
  padding: 28px 32px 40px;
}

.topbar {
  display: flex;
  gap: 20px;
  justify-content: space-between;
  align-items: center;
  padding-bottom: 23px;
  border-bottom: 1px solid var(--line);
}

h1 {
  font-size: 20px;
  letter-spacing: -.5px;
  margin: 0;
  font-weight: 600;
}

.brand {
  display: flex;
  gap: 12px;
  align-items: center;
}

.signal {
  width: 25px;
  display: flex;
  gap: 3px;
  align-items: end;
  height: 25px;
}

.signal i {
  width: 5px;
  background: var(--green);
  border-radius: 2px;
}

.signal i:nth-child(1) {
  height: 9px;
}

.signal i:nth-child(2) {
  height: 16px;
}

.signal i:nth-child(3) {
  height: 23px;
}

.subtitle {
  color: var(--muted);
  font-size: 12px;
  margin-top: 2px;
}

.connection {
  display: flex;
  gap: 9px;
  align-items: center;
  font: 12px var(--mono);
  color: var(--muted);
}

.dot {
  width: 7px;
  height: 7px;
  border-radius: 50%;
  background: var(--amber);
}

.dot.online {
  background: var(--green);
  box-shadow: 0 0 12px #8ae7b63b;
}

.dot.offline {
  background: var(--red);
}

.frequency-grid {
  display: grid;
  grid-template-columns: 1.15fr 1fr;
  gap: 18px;
  margin: 25px 0 18px;
}

.frequency-card {
  border: 1px solid var(--line);
  border-radius: 12px;
  padding: 24px 26px;
  background: var(--panel);
  min-height: 181px;
}

.frequency-card.current {
  background: linear-gradient(115deg,#172923,#121d24 72%);
  border-color: #335145;
}

.eyebrow {
  font-size: 12px;
  color: var(--muted);
  letter-spacing: .03em;
}

.frequency {
  font: clamp(28px,3.8vw,48px)/1.2 var(--mono);
  letter-spacing: -2px;
  margin: 14px 0 12px;
  white-space: nowrap;
}

.frequency .unit {
  font-size: 16px;
  color: var(--muted);
  letter-spacing: 0;
  margin-left: 9px;
}

.current .frequency {
  color: var(--green);
}

.subline {
  font: 12px var(--mono);
  color: var(--muted);
  min-height: 18px;
}

.subline strong {
  color: var(--text);
  font-weight: 400;
}

.pill {
  display: inline-block;
  border: 1px solid #405348;
  padding: 2px 8px;
  border-radius: 5px;
  color: var(--green);
  font: 11px/1.5 var(--mono);
  margin-right: 9px;
}

.controls {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 12px;
  padding: 17px 0 20px;
}

.modes {
  display: flex;
  gap: 5px;
}

.button {
  border: 1px solid var(--line);
  background: #15212a;
  color: var(--text);
  border-radius: 6px;
  padding: 8px 15px;
  font-size: 13px;
  min-height: 38px;
}

.button:hover:enabled {
  border-color: #617a8b;
  background: #1c2d38;
}

.button.active {
  color: var(--green);
  border-color: #5a9478;
  background: #1e352c;
}

.button.small {
  padding: 7px 12px;
}

.tune {
  display: flex;
  gap: 7px;
  align-items: center;
  margin-left: auto;
}

.tune label {
  font-size: 12px;
  color: var(--muted);
  margin-right: 4px;
}

.tune input {
  background: #0b131a;
  color: var(--text);
  font: 13px var(--mono);
  border: 1px solid var(--line);
  border-radius: 6px;
  padding: 9px;
  width: 115px;
  min-height: 38px;
}

.notice {
  margin: 0 0 22px;
  display: flex;
  gap: 10px;
  align-items: start;
  color: var(--muted);
  font-size: 13px;
  min-height: 21px;
}

.notice.error {
  color: var(--red);
}

.notice span:first-child {
  color: var(--green);
}

.notice.error span:first-child {
  color: var(--red);
}

.metrics {
  display: grid;
  grid-template-columns: repeat(6,1fr);
  border: 1px solid var(--line);
  border-radius: 9px;
  margin-bottom: 25px;
  background: #101820;
}

.metric {
  padding: 15px 18px;
  border-right: 1px solid var(--line);
  min-width: 0;
}

.metric:last-child {
  border: 0;
}

.metric-value {
  font: 20px var(--mono);
  margin: 5px 0 0;
  overflow-wrap: anywhere;
}

.metric-value.good {
  color: var(--green);
}

.gateway {
  display: flex;
  flex-wrap: wrap;
  gap: 7px 25px;
  border-bottom: 1px solid var(--line);
  padding: 0 0 20px;
  margin: 0 0 24px;
  color: var(--muted);
  font-size: 12px;
}

.gateway strong {
  color: var(--text);
  font: 12px var(--mono);
  margin-left: 7px;
}

.packet-heading {
  display: flex;
  gap: 15px;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 12px;
}

h2 {
  font-size: 16px;
  font-weight: 600;
  margin: 0;
}

.packet-caption {
  color: var(--muted);
  font-size: 12px;
  margin: 3px 0 0;
}

.retention {
  color: var(--muted);
  font: 11px var(--mono);
  white-space: nowrap;
}

.table-wrap {
  overflow-x: auto;
  border: 1px solid var(--line);
  border-radius: 9px;
}

table {
  border-collapse: collapse;
  width: 100%;
  table-layout: fixed;
  text-align: left;
}

th {
  color: var(--muted);
  font: 11px var(--mono);
  padding: 13px 16px;
  background: #131e27;
  border-bottom: 1px solid var(--line);
  font-weight: 400;
}

td {
  padding: 14px 16px;
  border-bottom: 1px solid #1c2b36;
  vertical-align: top;
  font: 12px/1.6 var(--mono);
}

tbody tr:last-child td {
  border-bottom: 0;
}

.col-time {
  width: 152px;
}

.col-frequency {
  width: 125px;
}

.col-rssi {
  width: 85px;
}

.col-kind {
  width: 108px;
}

.frame-kind {
  color: var(--green);
}

.crc {
  font: 10px var(--mono);
  color: var(--muted);
  display: block;
  margin-top: 4px;
}

.bytes {
  color: #c8d5df;
  overflow-wrap: anywhere;
}

.bytes summary {
  cursor: pointer;
  list-style: none;
  display: flex;
  gap: 9px;
  align-items: baseline;
}

.bytes summary::-webkit-details-marker {
  display: none;
}

.bytes summary::before {
  content: "+";
  color: var(--green);
  flex-shrink: 0;
}

.bytes[open] summary::before {
  content: "−";
}

.byte-preview {
  white-space: nowrap;
  text-overflow: ellipsis;
  overflow: hidden;
  min-width: 0;
}

.byte-count {
  font-size: 10px;
  color: var(--muted);
  white-space: nowrap;
  margin-left: auto;
}

.bytes pre {
  font: 12px/1.8 var(--mono);
  color: var(--text);
  white-space: pre-wrap;
  overflow-wrap: anywhere;
  margin: 12px 0 0;
  border-top: 1px solid var(--line);
  padding-top: 12px;
}

.packet-extra {
  font-size: 10px;
  color: var(--muted);
  margin-top: 8px;
  overflow-wrap: anywhere;
}

.empty {
  padding: 48px 18px;
  text-align: center;
  color: var(--muted);
  font: 13px var(--sans);
}

.footer {
  display: flex;
  flex-wrap: wrap;
  justify-content: space-between;
  gap: 10px;
  font-size: 11px;
  color: var(--muted);
  padding-top: 18px;
}

.hidden {
  display: none!important;
}

#gap-note {
  color: var(--amber);
  font-size: 12px;
  margin: 0 0 12px;
}

#error-text {
  color: var(--red);
  margin: 8px 0;
  font-size: 12px;
}

.clock {
  white-space: nowrap;
}

.gateway-heading {
  display: flex;
  align-items: center;
  justify-content: space-between;
  margin-bottom: 12px;
  gap: 15px;
}

.gateway-heading .button {
  font-size: 11px;
  padding: 5px 10px;
  min-height: 30px;
}

.gateway-cards {
  display: grid;
  grid-template-columns: repeat(auto-fit,minmax(330px,1fr));
  gap: 12px;
  margin-bottom: 10px;
  align-items: start;
}

.gateway-card {
  display: block;
  text-align: left;
  color: var(--text);
  border: 1px solid var(--line);
  border-radius: 9px;
  background: var(--panel);
  padding: 17px;
  width: 100%;
  min-width: 0;
}

.gateway-filter:hover {
  border-color: #617a8b;
}

.gateway-card.selected {
  border-color: var(--green);
}

.gateway-card-top {
  display: flex;
  justify-content: space-between;
  gap: 12px;
  align-items: baseline;
}

.gateway-card-id {
  font: 14px var(--mono);
  overflow-wrap: anywhere;
}

.gateway-lock {
  font: 10px var(--mono);
  color: var(--amber);
  white-space: nowrap;
}

.gateway-lock.locked {
  color: var(--green);
}

.gateway-card-meta {
  color: var(--muted);
  font: 10px var(--mono);
  margin-top: 5px;
}

.gateway-next {
  font: 24px var(--mono);
  letter-spacing: -1px;
  margin-top: 15px;
  color: var(--green);
}

.gateway-next-label {
  font-size: 10px;
  color: var(--muted);
  letter-spacing: 0;
  margin-bottom: 4px;
}

.gateway-card-bottom {
  display: flex;
  gap: 5px 15px;
  flex-wrap: wrap;
  border-top: 1px solid var(--line);
  margin-top: 13px;
  padding-top: 10px;
  font: 10px var(--mono);
  color: var(--muted);
}

.gateway-note {
  font-size: 11px;
  color: var(--muted);
  margin: 9px 0 24px;
}

.gateway-empty {
  font-size: 13px;
  color: var(--muted);
  padding: 15px 0;
}

.gateway-last {
  color: var(--muted);
  font: 10px var(--mono);
  margin-top: 8px;
}

.gateway-filter {
  display: block;
  width: 100%;
  border: 0;
  border-bottom: 1px solid var(--line);
  background: transparent;
  color: var(--text);
  text-align: left;
  padding: 0 0 11px;
}

.gateway-fields {
  margin-top: 12px;
}

.gateway-fields summary {
  cursor: pointer;
  color: var(--muted);
  font-size: 11px;
}

.field-grid {
  display: grid;
  grid-template-columns: minmax(100px,1fr) minmax(0,1.3fr);
  gap: 5px 10px;
  margin: 11px 0;
  font-size: 11px;
}

.field-grid dt {
  color: var(--muted);
}

.field-grid dd {
  margin: 0;
  color: var(--text);
  font-family: var(--mono);
  overflow-wrap: anywhere;
}

.gateway-data {
  margin-top: 16px;
  border-top: 1px solid var(--line);
  padding-top: 12px;
}

.gateway-data h3 {
  font-size: 12px;
  font-weight: 600;
  margin: 0 0 9px;
}

.data-empty {
  font-size: 11px;
  color: var(--muted);
  padding: 5px 0;
}

.data-packet {
  border-top: 1px solid var(--line);
  padding: 11px 0;
}

.data-packet:first-child {
  border-top: 0;
}

.data-packet > summary {
  cursor: pointer;
  color: var(--text);
  font: 11px/1.7 var(--mono);
  overflow-wrap: anywhere;
}

.data-packet > summary span {
  color: var(--muted);
  margin-left: 8px;
}

.data-association {
  color: var(--amber);
  font-size: 11px;
  margin: 9px 0;
}

.data-association.explicit {
  color: var(--green);
}

.payload-label {
  color: var(--muted);
  font-size: 10px;
  margin-top: 12px;
}

.payload-hex {
  font: 11px/1.9 var(--mono);
  color: var(--green);
  background: #0b131a;
  border: 1px solid var(--line);
  border-radius: 5px;
  padding: 10px;
  margin: 6px 0 10px;
  white-space: pre-wrap;
  overflow-wrap: anywhere;
  user-select: text;
  max-height: 260px;
  overflow-y: auto;
}

.data-packet .bytes {
  font: 10px/1.8 var(--mono);
}

.data-packet .bytes pre {
  font-size: 10px;
}

.other-data {
  margin: 22px 0;
  border: 1px solid var(--line);
  border-radius: 9px;
  background: var(--panel);
  padding: 17px;
}

.other-data h2 {
  margin-bottom: 8px;
}

.other-data-list {
  display: grid;
  grid-template-columns: repeat(auto-fit,minmax(310px,1fr));
  gap: 0 22px;
  align-items: start;
}

#data-listen {
  margin-top: 9px;
  color: var(--muted);
  font: 10px/1.7 var(--mono);
  overflow-wrap: anywhere;
}

@media(max-width:900px) {
  main {
    padding: 22px 20px 30px;
  }
  .metrics {
    grid-template-columns: repeat(3,1fr);
  }
  .metric:nth-child(3) {
    border-right: 0;
  }
  .metric:nth-child(-n+3) {
    border-bottom: 1px solid var(--line);
  }
  .col-time {
    width: 133px;
  }
  .col-frequency {
    width: 110px;
  }
  .col-rssi {
    width: 75px;
  }
  .col-kind {
    width: 95px;
  }
  table {
    min-width: 730px;
  }
  .frequency-card {
    padding: 21px;
  }
  .frequency {
    font-size: 36px;
  }
}

@media(max-width:560px) {
  main {
    padding: 18px 14px 28px;
  }
  .topbar {
    align-items: start;
    gap: 10px;
  }
  h1 {
    font-size: 18px;
  }
  .connection {
    font-size: 10px;
    padding-top: 5px;
  }
  .subtitle {
    font-size: 10px;
  }
  .frequency-grid {
    grid-template-columns: 1fr;
    gap: 10px;
    margin-top: 18px;
  }
  .gateway-cards,
  .other-data-list {
    grid-template-columns: minmax(0,1fr);
  }
  .frequency-card {
    min-height: 145px;
    padding: 18px 20px;
  }
  .frequency {
    font-size: 36px;
    margin: 10px 0;
  }
  .controls {
    gap: 14px;
  }
  .modes {
    width: 100%;
  }
  .modes .button {
    flex: 1;
  }
  .tune {
    width: 100%;
    margin: 0;
  }
  .tune input {
    flex: 1;
  }
  .metric {
    padding: 12px;
  }
  .metric-value {
    font-size: 18px;
  }
  .metric .eyebrow {
    font-size: 10px;
  }
  .gateway {
    gap: 8px 18px;
  }
  .packet-heading {
    align-items: start;
  }
  .retention {
    padding-top: 3px;
    font-size: 10px;
  }
  .footer {
    font-size: 10px;
  }
}
</style>
</head>
<body>
<main>
<header class="topbar">
  <div class="brand"><div class="signal" aria-hidden="true"><i></i><i></i><i></i></div><div><h1>Sidewalk receiver</h1><div class="subtitle">SX1276 · Sub-GHz FSK · 50 kb/s</div></div></div>
  <div class="connection" role="status"><span id="connection-dot" class="dot"></span><span id="connection-text">Connecting</span></div>
</header>
<section class="frequency-grid" aria-label="Receiver frequencies">
  <div class="frequency-card current">
    <div class="eyebrow">Current frequency</div>
    <div class="frequency"><span id="current-frequency">—</span><span class="unit">MHz</span></div>
    <div class="subline"><span id="mode" class="pill">STARTING</span><span id="signal-strength">Waiting for receiver</span></div>
    <div id="data-listen">Discovering gateway schedules</div>
  </div>
  <div class="frequency-card"><div class="eyebrow">Predicted next beacon</div><div class="frequency"><span id="next-frequency">—</span><span class="unit">MHz</span></div><div id="next-time" class="subline">Waiting for a validated beacon schedule</div></div>
</section>
<section class="controls" aria-label="Receiver controls">
  <div class="modes"><button class="button" data-action="scan" title="Discover gateways and follow known schedules between scans">Scan + follow</button><button class="button" data-action="hold" title="Receive on the current frequency">Hold</button><button class="button" data-action="follow" title="Prioritize known gateway schedules and scan gaps" disabled>Follow gateways</button></div>
  <form id="tune-form" class="tune"><label for="frequency-input">Tune MHz</label><input id="frequency-input" type="number" min="902" max="928" step="0.001" value="902.2" required inputmode="decimal"><button class="button small" type="submit">Tune</button></form>
</section>
<p id="notice" class="notice" role="status"><span aria-hidden="true">●</span><span id="device-message">Connecting to the receiver…</span></p>
<p id="error-text" class="hidden" role="alert"></p>
<section class="metrics" aria-label="Reception statistics">
  <div class="metric"><div class="eyebrow">CRC valid</div><div id="crc-ok" class="metric-value good">—</div></div>
  <div class="metric"><div class="eyebrow">CRC failed</div><div id="crc-fail" class="metric-value">—</div></div>
  <div class="metric"><div class="eyebrow">Sync events</div><div id="syncs" class="metric-value">—</div></div>
  <div class="metric"><div class="eyebrow">Missed beacons</div><div id="misses" class="metric-value">—</div></div>
  <div class="metric"><div class="eyebrow">FIFO overflows</div><div id="overflows" class="metric-value">—</div></div>
  <div class="metric"><div class="eyebrow">Uptime</div><div id="uptime" class="metric-value">—</div></div>
</section>
<section aria-label="Discovered gateways">
  <div class="gateway-heading">
    <h2>Gateways <span id="gateway-count" class="retention">0</span></h2>
    <button id="all-packets" class="button active" type="button" aria-pressed="true">All packets</button>
  </div>
  <div id="gateway-cards" class="gateway-cards"></div>
  <div id="gateway-empty" class="gateway-empty">Listening for gateway discovery beacons…</div>
  <p class="gateway-note">One receiver shares time between gateways. Overlapping packets can be missed. Gateway IDs may rotate. Each gateway retains its latest five data frames here.</p>
</section>
<section class="other-data" aria-label="Other data frames">
  <div class="packet-heading">
    <h2>Other data frames</h2>
    <span id="data-retention" class="retention">0 / 100 retained</span>
  </div>
  <p class="packet-caption">Frames without a current gateway association. Unsupported formats retain their raw bytes.</p>
  <div id="other-data-list" class="other-data-list"></div>
</section>
<section aria-label="Received packets">
  <div class="packet-heading"><div><h2>Received packets</h2><p class="packet-caption">Raw link frames with encrypted fields preserved. Select bytes to expand.</p></div><span id="retention" class="retention">0 / 100 shown</span></div>
  <p id="gap-note" class="hidden">Some packets aged out of the receiver buffer while this page was disconnected.</p>
  <div class="table-wrap"><table><thead><tr><th class="col-time" scope="col">Local time</th><th class="col-frequency" scope="col">MHz</th><th class="col-rssi" scope="col">dBm</th><th class="col-kind" scope="col">Frame</th><th scope="col">Received frame bytes</th></tr></thead><tbody id="packets"><tr id="empty-row"><td colspan="5" class="empty">Waiting for a complete CRC-valid frame.</td></tr></tbody></table></div>
</section>
<footer class="footer"><span id="network">Wi-Fi connecting</span><span id="gps">GPS waiting</span><span>Beacon authentication tags unverified</span><span id="clock" class="clock"></span></footer>
</main>
<script>
'use strict';
const byId = id => document.getElementById(id);
const state = {
  after: 0,
  dataAfter: 0,
  uptime: null,
  next: null,
  nextAt: 0,
  dataListen: null,
  dataListenAt: 0,
  mode: 'scan',
  pending: false,
  online: false,
  gateway: null,
  radioOk: false,
  rows: [],
  dataPackets: [],
  gateways: new Map(),
  filter: null,
  otherDataFingerprint: null,
  otherDataNodes: new Map()
};
const modes = Array.from(document.querySelectorAll('[data-action]'));
const number = value => Number.isFinite(Number(value)) ? Number(value).toLocaleString() : '—';
const frequency = hz => Number.isFinite(Number(hz)) && Number(hz)>0 ? (Number(hz)/1e6).toFixed(3) : '—';
function duration(ms) {
  const seconds = Math.max(0, Math.floor(Number(ms) / 1000));
  if (!Number.isFinite(seconds)) return '—';
  if (seconds >= 3600) return Math.floor(seconds / 3600) + 'h ' + Math.floor(seconds % 3600 / 60) + 'm';
  if (seconds >= 60) return Math.floor(seconds / 60) + 'm ' + seconds % 60 + 's';
  return seconds + 's';
}
function setConnection(online, text) {
  state.online = online;
  byId('connection-dot').className = 'dot ' + (online ? 'online' : 'offline');
  byId('connection-text').textContent = text;
  updateButtons();
}
function updateButtons() {
  const hasSchedule = state.next || Array.from(state.gateways.values()).some(entry => entry.gateway.locked);
  for (const button of modes) {
    button.disabled = state.pending || !state.online || !state.radioOk
      || (button.dataset.action === 'follow' && !hasSchedule);
  }
  for (const input of byId('tune-form').elements) {
    input.disabled = state.pending || !state.online || !state.radioOk;
  }
}
function showError(message) {
  byId('error-text').textContent = message;
  byId('error-text').classList.toggle('hidden', !message);
}
function renderNext() {
  if (!state.next) {
    byId('next-frequency').textContent = '—';
    byId('next-time').textContent = 'Waiting for a validated beacon schedule';
    return;
  }
  byId('next-frequency').textContent = frequency(state.next.frequencyHz);
  const remaining = state.nextAt - performance.now();
  const timing = remaining > 0 ? 'in ' + (remaining / 1000).toFixed(1) + ' s' : 'Awaiting reception';
  byId('next-time').textContent = timing + ' · sequence ' + String(state.next.sequence ?? '—');
}
function element(tag, className, text) {
  const node = document.createElement(tag);
  if (className) node.className = className;
  if (text !== undefined) node.textContent = text;
  return node;
}
function gatewayKey(id, pan) {
  return String(id) + '|' + String(pan);
}
function setFilter(key) {
  state.filter = key;
  for (const [entryKey, entry] of state.gateways) {
    const selected = key === entryKey;
    entry.node.classList.toggle('selected', selected);
    entry.filterButton.setAttribute('aria-pressed', String(selected));
  }
  byId('all-packets').classList.toggle('active', key === null);
  byId('all-packets').setAttribute('aria-pressed', String(key === null));
  updatePacketVisibility();
}
function updatePacketVisibility() {
  let visible = 0;
  for (const row of state.rows) {
    const show = state.filter === null || (row.dataset.pan
      ? row.dataset.gatewayKey === state.filter
      : row.dataset.gateway === state.filter.split('|')[0]);
    row.classList.toggle('hidden', !show);
    if (show) visible++;
  }
  byId('empty-row').classList.toggle('hidden', visible > 0);
  byId('empty-row').firstElementChild.textContent = state.filter === null
    ? 'Waiting for a complete CRC-valid frame.' : 'No frames for this gateway in the browser history.';
  byId('retention').textContent = visible + ' shown · ' + state.rows.length + ' / 100 retained';
}
function renderGatewayTimes() {
  const now = performance.now();
  for (const entry of state.gateways.values()) {
    const gateway = entry.gateway;
    if (Number(gateway.nextFrequencyHz) > 0) {
      const remaining = entry.nextAt - now;
      const predictedTime = new Date(entry.nextEpochMs).toLocaleTimeString([], {
        hour12: false,
        hour: '2-digit',
        minute: '2-digit',
        second: '2-digit'
      });
      const label = gateway.locked ? 'Next beacon' : 'Predicted beacon';
      const countdown = remaining > 0 ? 'in ' + (remaining / 1000).toFixed(1) + ' s' : 'awaiting reception';
      entry.timing.textContent = label + ' at ' + predictedTime + ' · ' + countdown;
    } else {
      entry.timing.textContent = 'Validating beacon schedule';
    }
  }
}

function renderDataListen() {
  const opportunity = state.dataListen;
  if (!opportunity) {
    byId('data-listen').textContent = state.mode === 'hold'
      ? 'Stationary reception'
      : 'Scanning between gateway opportunities';
    return;
  }
  const directions = {shared: 'Shared data slot', uplink: 'Uplink slot', downlink: 'Downlink slot'};
  const label = directions[opportunity.direction] || 'Data slot';
  const remaining = state.dataListenAt - performance.now();
  const timing = remaining > 0
    ? 'in ' + (remaining / 1000).toFixed(2) + ' s · estimated timing'
    : 'listening · estimated timing';
  byId('data-listen').textContent = label + ' ' + String(opportunity.slot ?? '—')
    + ' · gateway ' + String(opportunity.gateway || '—')
    + ' · ' + frequency(opportunity.frequencyHz) + ' MHz · ' + timing;
}

function formatHex(value) {
  const bytes = String(value ?? '').replace(/[^0-9a-f]/gi, '').toUpperCase().match(/.{1,2}/g) || [];
  const lines = [];
  for (let offset = 0; offset < bytes.length; offset += 16) {
    lines.push(bytes.slice(offset, offset + 16).join(' '));
  }
  return lines.join('\n');
}

function addField(grid, label, value) {
  grid.append(element('dt', '', label));
  grid.append(element('dd', '', value === null || value === undefined || value === '' ? '—' : String(value)));
}

function flagValue(value) {
  if (value === true) return 'Yes';
  if (value === false) return 'No';
  return 'Unavailable';
}

function addGatewayFields(entry) {
  const gateway = entry.gateway;
  entry.fields.replaceChildren();
  addField(entry.fields, 'Cloud indicator', gateway.cloudConnection);
  addField(entry.fields, 'Roaming allowed', flagValue(gateway.roamingAllowed));
  addField(entry.fields, 'Load indicator', gateway.loadIndicator);
  addField(entry.fields, 'TX power', gateway.advertisedPowerDbm == null ? null : gateway.advertisedPowerDbm + ' dBm');
  addField(entry.fields, 'UL adjustment', gateway.uplinkAdjustmentDb == null ? null : gateway.uplinkAdjustmentDb + ' dB');
  addField(entry.fields, 'Authentication tag', gateway.authenticationTagBytes == null ? null : gateway.authenticationTagBytes + ' bytes, unverified');
  addField(entry.fields, 'Hopping seed', gateway.seed);
  addField(entry.fields, 'CS schedule', flagValue(gateway.hasSchedule));
  if (gateway.hasSchedule) {
    addField(entry.fields, 'CS UL offset', gateway.csUlOffset + ' slots');
    addField(entry.fields, 'CS UL periodicity', gateway.csUlPeriodicity + ' slots');
    addField(entry.fields, 'CS DL offset', gateway.csDlOffset + ' slots');
    addField(entry.fields, 'CS DL periodicity', gateway.csDlPeriodicity + ' slots');
  }
}

function createDataPacket(packet) {
  const details = element('details', 'data-packet');
  details.dataset.packetId = String(packet.id);
  const summary = element('summary');
  const time = packetTime(packet);
  summary.append(document.createTextNode(time.label));
  summary.title = time.title;
  const types = {data: 'Data', network: 'Network', ack: 'ACK', unknown: 'Unknown format'};
  const packetType = types[packet.kind] || 'Unknown format';
  summary.append(element('span', '', packetType + ' · ' + frequency(packet.frequencyHz) + ' MHz'));
  details.append(summary);

  let association = 'No gateway association';
  if (packet.association === 'explicit') {
    association = 'Gateway ID matched in frame header';
  } else if (packet.association === 'scheduled') {
    association = 'Observed during this gateway schedule';
  }
  const associationNode = element('div', 'data-association', association);
  associationNode.classList.toggle('explicit', packet.association === 'explicit');
  details.append(associationNode);

  const data = packet.data || {};
  const fields = element('dl', 'field-grid');
  addField(fields, 'Source ID', data.sourceId);
  addField(fields, 'Destination ID', data.destinationId);
  addField(fields, 'Sequence', data.sequence);
  addField(fields, 'Frame type', data.frameTypeName || data.frameType);
  addField(fields, 'Protocol version', data.protocolVersion);
  addField(fields, 'Network header valid', flagValue(data.networkValid));
  addField(fields, 'Encrypted flag', flagValue(data.encrypted));
  addField(fields, 'ACK requested', flagValue(data.ackRequested));
  addField(fields, 'Network epoch', data.epoch);
  addField(fields, 'Direction', packet.direction || 'unknown');
  addField(fields, 'RSSI', packet.rssiDbm + ' dBm');
  addField(fields, 'Frame length', packet.length + ' bytes');
  addField(fields, 'Header length', data.headerLength === null || data.headerLength === undefined ? null : data.headerLength + ' bytes');
  if (data.counterHex) addField(fields, 'Security counter bytes', formatHex(data.counterHex));
  if (data.authenticationTagHex) addField(fields, 'Authentication tag', formatHex(data.authenticationTagHex) + ' (unverified)');
  if (data.parseStatus) addField(fields, 'Parser status', String(data.parseStatus).replace(/_/g, ' '));
  if (packet.gateway) addField(fields, 'Associated gateway', packet.gateway);
  if (packet.pan) addField(fields, 'Associated PAN', packet.pan);
  details.append(fields);

  if (typeof data.payloadHex === 'string') {
    const payloadLength = data.payloadLength ?? Math.floor(data.payloadHex.length / 2);
    const isCiphertext = data.networkValid === true && data.encrypted === true;
    const label = isCiphertext ? 'Encrypted payload bytes' : 'Extracted payload bytes';
    details.append(element('div', 'payload-label', label + ' · ' + payloadLength + ' bytes'));
    const payload = element('pre', 'payload-hex', formatHex(data.payloadHex) || '(empty payload)');
    payload.tabIndex = 0;
    payload.setAttribute('aria-label', label);
    details.append(payload);
  } else {
    details.append(element('p', 'data-empty', 'Payload boundaries unavailable for this frame format. Raw frame bytes are retained below.'));
  }

  const raw = element('details', 'bytes');
  raw.append(element('summary', '', 'Raw link frame · ' + packet.length + ' bytes · CRC valid'));
  const rawBytes = element('pre', '', formatHex(packet.hex));
  rawBytes.tabIndex = 0;
  raw.append(rawBytes);
  details.append(raw);
  return details;
}

function renderDataCollection(container, packets, nodes, emptyMessage) {
  const nextNodes = new Map();
  const children = [];
  for (let index = 0; index < packets.length; index++) {
    const packet = packets[index];
    const existing = nodes.get(packet.id);
    const node = existing || createDataPacket(packet);
    if (!existing && index === 0) node.open = true;
    children.push(node);
    nextNodes.set(packet.id, node);
  }
  if (children.length === 0) children.push(element('p', 'data-empty', emptyMessage));
  container.replaceChildren(...children);
  return nextNodes;
}

function renderDataPanels() {
  for (const [key, entry] of state.gateways) {
    const packets = state.dataPackets.filter(packet => packet.association !== 'none'
      && gatewayKey(packet.gateway, packet.pan) === key).slice(0, 5);
    const fingerprint = packets.map(packet => packet.id).join(',');
    if (entry.dataFingerprint !== fingerprint) {
      entry.dataNodes = renderDataCollection(entry.dataList, packets, entry.dataNodes,
        'No data frames captured for this gateway yet.');
      entry.dataFingerprint = fingerprint;
    }
    entry.dataHeading.textContent = 'Data frames · ' + packets.length + ' / 5 shown';
  }
  const otherPackets = state.dataPackets.filter(packet => packet.association === 'none'
    || !packet.gateway || !state.gateways.has(gatewayKey(packet.gateway, packet.pan))).slice(0, 10);
  const fingerprint = otherPackets.map(packet => packet.id).join(',');
  if (state.otherDataFingerprint !== fingerprint) {
    state.otherDataNodes = renderDataCollection(byId('other-data-list'), otherPackets,
      state.otherDataNodes, 'No unassigned data frames captured yet.');
    state.otherDataFingerprint = fingerprint;
  }
  byId('data-retention').textContent = state.dataPackets.length + ' / 100 data frames retained';
}

function appendDataPacket(packet) {
  if (!Number.isSafeInteger(packet.id) || packet.id <= state.dataAfter) return;
  state.dataAfter = packet.id;
  if (packet.kind === 'beacon') return;
  state.dataPackets.unshift(packet);
  if (state.dataPackets.length > 100) state.dataPackets.length = 100;
}

function renderGateways(gateways) {
  const seen = new Set();
  for (const gateway of gateways) {
    const key = gatewayKey(gateway.id, gateway.pan);
    seen.add(key);
    let entry = state.gateways.get(key);
    if (!entry) {
      const node = element('article', 'gateway-card');
      const filterButton = element('button', 'gateway-filter');
      filterButton.type = 'button';
      filterButton.title = 'Filter packet history to this gateway';
      const top = element('span', 'gateway-card-top');
      const id = element('span', 'gateway-card-id');
      const lock = element('span', 'gateway-lock');
      top.append(id, lock);
      const meta = element('span', 'gateway-card-meta');
      meta.style.display = 'block';
      filterButton.append(top, meta);
      const next = element('div', 'gateway-next');
      const timing = element('div', 'gateway-next-label');
      const last = element('div', 'gateway-last');
      const bottom = element('div', 'gateway-card-bottom');
      const advertised = element('details', 'gateway-fields');
      advertised.append(element('summary', '', 'Advertised beacon fields · CS slots are 63 ms'));
      const fields = element('dl', 'field-grid');
      advertised.append(fields);
      const dataSection = element('section', 'gateway-data');
      const dataHeading = element('h3', '', 'Data frames');
      const dataList = element('div');
      dataSection.append(dataHeading, dataList);
      node.append(filterButton, next, timing, last, bottom, advertised, dataSection);
      entry = {
        node, filterButton, id, lock, meta, next, timing, last, bottom, fields,
        dataHeading, dataList, gateway, nextAt: 0, nextEpochMs: 0,
        dataNodes: new Map(), dataFingerprint: null
      };
      filterButton.addEventListener('click', () => setFilter(state.filter === key ? null : key));
      state.gateways.set(key, entry);
      byId('gateway-cards').append(node);
    }
    entry.gateway = gateway;
    entry.nextAt = performance.now() + Number(gateway.nextInMs || 0);
    entry.nextEpochMs = Date.now() + Number(gateway.nextInMs || 0);
    entry.id.textContent = String(gateway.id || 'Unknown');
    entry.lock.textContent = gateway.locked ? 'Validated' : 'Acquiring';
    entry.lock.classList.toggle('locked', Boolean(gateway.locked));
    entry.meta.textContent = 'PAN ' + String(gateway.pan ?? '—') + ' · ' + number(gateway.rssiDbm) + ' dBm';
    entry.next.textContent = Number(gateway.nextFrequencyHz) > 0
      ? frequency(gateway.nextFrequencyHz) + ' MHz' : '— MHz';
    const lastTime = packetTime({epochMs: gateway.lastSeenEpochMs, uptimeMs: gateway.lastSeenMs});
    entry.last.textContent = 'Last ' + lastTime.label + ' · ' + frequency(gateway.lastFrequencyHz) + ' MHz';
    entry.last.title = lastTime.title;
    entry.bottom.textContent = 'Seq ' + String(gateway.sequence ?? '—')
      + ' · ' + number(gateway.matches || 0) + ' matches'
      + ' · ' + number(gateway.packets || 0) + ' beacons'
      + ' · ' + number(gateway.misses || 0) + ' misses';
    entry.node.classList.toggle('selected', state.filter === key);
    entry.filterButton.setAttribute('aria-pressed', String(state.filter === key));
    addGatewayFields(entry);
  }
  for (const [key, entry] of state.gateways) {
    if (!seen.has(key)) {
      entry.node.remove();
      state.gateways.delete(key);
    }
  }
  if (state.filter !== null && !state.gateways.has(state.filter)) setFilter(null);
  byId('gateway-count').textContent = String(gateways.length);
  byId('gateway-empty').classList.toggle('hidden', gateways.length > 0);
  renderGatewayTimes();
  renderDataPanels();
}
function renderStatus(status) {
  if (state.uptime !== null && status.uptimeMs < state.uptime) {
    state.after = 0;
    state.dataAfter = 0;
    state.dataPackets = [];
    state.otherDataFingerprint = null;
    state.otherDataNodes.clear();
    for (const row of state.rows) row.remove();
    state.rows = [];
    for (const entry of state.gateways.values()) entry.node.remove();
    state.gateways.clear();
    setFilter(null);
    byId('empty-row').classList.remove('hidden');
    byId('gap-note').classList.add('hidden');
  }
  state.uptime = status.uptimeMs;
  state.mode = status.mode;
  state.dataListen = status.dataListen || null;
  state.dataListenAt = performance.now() + Number(state.dataListen?.inMs || 0);
  state.next = status.next && status.next.frequencyHz ? status.next : null;
  state.nextAt = performance.now() + Number(state.next?.inMs || 0);
  state.gateway = status.gateway;
  state.radioOk = Boolean(status.radioOk);
  byId('current-frequency').textContent = frequency(status.frequencyHz);
  const modeLabels = {scan: 'SCANNING', hold: 'HOLD', follow: 'FOLLOWING'};
  byId('mode').textContent = state.radioOk
    ? (modeLabels[status.mode] || String(status.mode || 'READY').toUpperCase()) : 'RADIO ERROR';
  byId('signal-strength').textContent = state.radioOk
    ? number(status.rssiDbm) + ' dBm · FSK receiver' : 'Check radio wiring';
  for (const button of modes) {
    const active = button.dataset.action === status.mode;
    button.classList.toggle('active', active);
    button.setAttribute('aria-pressed', String(active));
  }
  byId('device-message').textContent = status.message
    || (state.radioOk ? 'Receiver running' : 'SX1276 initialization failed');
  byId('notice').classList.toggle('error', !state.radioOk);
  const metrics = [
    ['crc-ok', 'crcOk'], ['crc-fail', 'crcFail'], ['syncs', 'syncs'],
    ['misses', 'misses'], ['overflows', 'overflows']
  ];
  for (const [id, key] of metrics) byId(id).textContent = number(status[key]);
  byId('uptime').textContent = duration(status.uptimeMs);
  renderGateways(Array.isArray(status.gateways) ? status.gateways : (status.gateway ? [status.gateway] : []));
  const wifi = status.wifi || {};
  byId('network').textContent = wifi.connected
    ? String(wifi.ssid || 'Wi-Fi') + ' · ' + String(wifi.ip || '') + ' · ' + number(wifi.rssi) + ' dBm'
    : 'Wi-Fi reconnecting';
  const gps = status.gps || {};
  byId('gps').textContent = gps.valid
    ? 'GPS ' + Number(gps.lat).toFixed(5) + ', ' + Number(gps.lon).toFixed(5) + ' · ' + number(gps.satellites) + ' satellites'
    : 'GPS waiting · ' + number(gps.satellites || 0) + ' satellites';
  renderNext();
  renderDataListen();
  updateButtons();
}
function cell(text, className) {
  return element('td', className, text);
}
function packetTime(packet) {
  if (Number(packet.epochMs) > 0) {
    const date = new Date(Number(packet.epochMs));
    return {
      label: date.toLocaleTimeString([], {hour12: false, hour: '2-digit', minute: '2-digit', second: '2-digit'})
        + '.' + String(date.getMilliseconds()).padStart(3, '0'),
      title: date.toLocaleString() + ' · device uptime ' + duration(packet.uptimeMs)
    };
  }
  return {
    label: '+' + (Number(packet.uptimeMs) / 1000).toFixed(3) + ' s',
    title: 'Receiver uptime. Wall clock was unavailable when this frame arrived.'
  };
}
function appendPacket(packet) {
  if (!Number.isSafeInteger(packet.id) || packet.id <= state.after) return;
  const row = document.createElement('tr');
  row.dataset.gateway = String(packet.gateway || '');
  row.dataset.pan = String(packet.pan || '');
  row.dataset.gatewayKey = gatewayKey(packet.gateway || '', packet.pan || '');
  const time = packetTime(packet);
  const timeCell = cell(time.label);
  timeCell.title = time.title;
  row.append(timeCell, cell(frequency(packet.frequencyHz)), cell(number(packet.rssiDbm)));
  const kinds = {beacon: 'Beacon', data: 'Data', network: 'Network', ack: 'ACK', unknown: 'Unknown'};
  const kind = cell(kinds[packet.kind] || 'Unknown', 'frame-kind');
  kind.append(element('span', 'crc', 'CRC valid'));
  row.append(kind);
  const payload = document.createElement('td');
  const details = element('details', 'bytes');
  const summary = document.createElement('summary');
  const bytePairs = String(packet.hex || '').replace(/[^0-9a-f]/gi, '').toUpperCase().match(/.{1,2}/g) || [];
  const preview = element('span', 'byte-preview', bytePairs.slice(0, 12).join(' ') + (bytePairs.length > 12 ? ' …' : ''));
  const count = element('span', 'byte-count', String(packet.length ?? bytePairs.length) + ' B');
  summary.append(preview, count);
  const raw = element('pre', '', formatHex(packet.hex));
  details.append(summary, raw);
  if (packet.gateway || packet.kind === 'beacon') {
    const metadata = [];
    if (packet.gateway) metadata.push('Gateway ' + String(packet.gateway));
    if (packet.pan) metadata.push('PAN ' + String(packet.pan));
    const sequence = packet.kind === 'beacon' ? packet.sequence : packet.data?.sequence;
    if (sequence !== undefined && sequence !== null) metadata.push('Sequence ' + String(sequence));
    const extra = element('div', 'packet-extra', metadata.join(' · '));
    details.append(extra);
  }
  payload.append(details);
  row.append(payload);
  byId('packets').insertBefore(row, byId('packets').firstChild);
  state.rows.unshift(row);
  while (state.rows.length > 100) state.rows.pop().remove();
  state.after = packet.id;
}
async function request(url, options = {}) {
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), 4500);
  try {
    const response = await fetch(url, {...options, signal: controller.signal, cache: 'no-store'});
    const text = await response.text();
    let body;
    try {
      body = text ? JSON.parse(text) : {};
    } catch {
      body = {message: text.slice(0, 150)};
    }
    if (!response.ok) throw new Error(body.message || body.error || 'Receiver returned HTTP ' + response.status);
    return body;
  } finally {
    clearTimeout(timeout);
  }
}
async function poll() {
  try {
    const status = await request('/api/status');
    renderStatus(status);
    const packets = await request('/api/packets?after=' + state.after);
    if (state.after > 0 && Number(packets.oldestId) > state.after + 1) {
      byId('gap-note').classList.remove('hidden');
    }
    const ordered = Array.isArray(packets.packets)
      ? packets.packets.slice().sort((a, b) => a.id - b.id) : [];
    for (const packet of ordered) appendPacket(packet);
    const data = await request('/api/data?after=' + state.dataAfter);
    const dataPackets = Array.isArray(data.packets)
      ? data.packets.slice().sort((a, b) => a.id - b.id) : [];
    for (const packet of dataPackets) appendDataPacket(packet);
    renderDataPanels();
    updatePacketVisibility();
    setConnection(true, 'Live');
  } catch (error) {
    setConnection(false, 'Reconnecting');
    byId('device-message').textContent = 'Receiver connection lost. Retrying automatically.';
    byId('notice').classList.add('error');
  } finally {
    setTimeout(poll, 1000);
  }
}
async function control(action, mhz) {
  if (state.pending) return;
  state.pending = true;
  updateButtons();
  showError('');
  try {
    const body = new URLSearchParams({action});
    if (mhz !== undefined) body.set('mhz', String(mhz));
    const result = await request('/api/control', {
      method: 'POST',
      headers: {'Content-Type': 'application/x-www-form-urlencoded'},
      body: body.toString()
    });
    if (result.ok === false) throw new Error(result.message || 'Receiver could not apply that change');
    if (result.message) byId('device-message').textContent = result.message;
  } catch (error) {
    showError(error.message || 'Could not update the receiver');
  } finally {
    state.pending = false;
    updateButtons();
  }
}
for (const button of modes) button.addEventListener('click', () => control(button.dataset.action));
byId('all-packets').addEventListener('click', () => setFilter(null));
byId('tune-form').addEventListener('submit', event => {
  event.preventDefault();
  const value = Number(byId('frequency-input').value);
  if (Number.isFinite(value)) control('tune', value);
});
setInterval(() => {
  renderNext();
  renderGatewayTimes();
  renderDataListen();
  byId('clock').textContent = new Date().toLocaleTimeString([], {hour12: false}) + ' local';
}, 200);
updateButtons();
poll();
</script>
</body>
</html>)SIDEWALK_HTML";
