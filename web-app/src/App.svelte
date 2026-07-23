<script>
  import './style.css'
import { ws } from './lib/stores/websocket.js';

  const devMode = window.location.hostname === 'localhost';
  let host = devMode ? 'localhost:5173' : window.location.host;
  let connected = false;
  let statusText = 'disconnected';
  let messages = [];
  let inputMsg = '{"type":"ping"}';

  ws.subscribe(state => {
    connected = state.status === 'connected';
    statusText = state.status;
    messages = state.messages;
  });

  function connect() {
    ws.connect(host, devMode);
  }

  function disconnect() {
    ws.disconnect();
  }

  function send() {
    try {
      const parsed = JSON.parse(inputMsg);
      ws.send(parsed);
    } catch {
      ws.send({ type: 'echo', data: inputMsg });
    }
  }

  function clearLog() {
    ws.clearLog();
  }

  function formatTime(ts) {
    return new Date(ts).toLocaleTimeString();
  }

  function formatJson(obj) {
    try {
      return JSON.stringify(obj, null, 2);
    } catch {
      return String(obj);
    }
  }
</script>

<main>
  <section class="connection-panel">
    <h2>Connection</h2>
    <div>
      {#if devMode}
        <span>Vite proxy &rarr; ESP32</span>
      {:else}
        <label for="host-input">Host:</label>
        <input id="host-input" bind:value={host} placeholder="studybud.local" />
      {/if}
      <span class="status {statusText}">{statusText}</span>
    </div>
    <div>
      <button on:click={connect} disabled={connected}>Connect</button>
      <button on:click={disconnect} disabled={!connected}>Disconnect</button>
    </div>
  </section>

  <section class="message-log">
    <h2>Log ({messages.length}) <button on:click={clearLog}>Clear</button></h2>
    <div class="log-area">
      {#each messages as msg}
        <div class="msg {msg.dir}">
          <span class="time">{formatTime(msg.time)}</span>
          <span class="dir">{msg.dir === 'in' ? '<--' : '-->'}</span>
          <pre>{formatJson(msg.data || msg.raw)}</pre>
        </div>
      {/each}
      {#if messages.length === 0}
        <p>No messages yet.</p>
      {/if}
    </div>
  </section>

  <section class="send-panel">
    <h2>Send Message</h2>
    <div>
        <input bind:value={inputMsg} placeholder={'{"type":"ping"}'} />
      <button on:click={send} disabled={!connected}>Send</button>
    </div>
  </section>
</main>

<style>
  :global(*) { box-sizing: border-box; margin: 0; padding: 0; }
  :global(body) { font-family: monospace; background: #0a0a0a; color: #ccc; }
  :global(button) { font-family: monospace; }
  main { display: flex; flex-direction: column; height: 100vh; padding: 0.5rem; }
  section { margin-bottom: 0.8rem; padding: 0.5rem; border: 1px solid #333; border-radius: 4px; }
  h2 { color: #fff; font-size: 0.9rem; margin-bottom: 0.4rem; }
  .connection-panel div, .send-panel div { display: flex; align-items: center; gap: 0.5rem; margin-bottom: 0.4rem; }
  .status { font-size: 0.7rem; text-transform: uppercase; margin-left: 0.4rem; }
  .status.connected { color: #4f4; }
  .status.disconnected { color: #f44; }
  .status.connecting, .status.reconnecting { color: #fa0; }
  input { background: #222; border: 1px solid #444; color: #fff; padding: 0.4rem; font-family: monospace; font-size: 0.85rem; border-radius: 3px; }
  button { background: #333; border: 1px solid #555; color: #fff; padding: 0.4rem 0.8rem; cursor: pointer; font-family: monospace; font-size: 0.85rem; border-radius: 3px; }
  button:hover { background: #444; }
  button:disabled { opacity: 0.4; cursor: default; }
  .log-area { max-height: 300px; overflow-y: auto; background: #0a0a0a; padding: 0.5rem; border-radius: 3px; }
  .msg { padding: 0.25rem 0; border-bottom: 1px solid #1a1a1a; font-size: 0.8rem; display: flex; align-items: flex-start; gap: 0.5rem; }
  .msg.in { color: #8cf; }
  .msg.out { color: #8f8; }
  .time { color: #666; white-space: nowrap; }
  .dir { white-space: nowrap; }
  pre { margin: 0; white-space: pre-wrap; word-break: break-all; }
  .send-panel input { flex: 1; }
</style>
