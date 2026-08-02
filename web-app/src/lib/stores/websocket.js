import { writable } from 'svelte/store';

const INITIAL_RECONNECT_DELAY = 1000;
const MAX_RECONNECT_DELAY = 30000;

function createWebSocketStore() {
  const { subscribe, set, update } = writable({
    status: 'disconnected',
    messages: [],
    lastMessage: null,
    seq: 0,
  });

  let ws = null;
  let reconnectDelay = INITIAL_RECONNECT_DELAY;
  let reconnectTimer = null;
  let url = '';

  function connect(host, useProxy) {
    if (useProxy) {
      const loc = window.location;
      url = `ws://${loc.hostname}:${loc.port}/ws`;
    } else {
      url = `ws://${host}/ws`;
    }
    doConnect();
  }

  function doConnect() {
    if (ws && (ws.readyState === WebSocket.CONNECTING || ws.readyState === WebSocket.OPEN)) {
      return;
    }

    update(s => ({ ...s, status: 'connecting' }));

    ws = new WebSocket(url);

    ws.onopen = () => {
      reconnectDelay = INITIAL_RECONNECT_DELAY;
      update(s => ({ ...s, status: 'connected' }));
      console.log('[WS] Connected to', url);
    };

    ws.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data);
        update(s => ({
          ...s,
          seq: s.seq + 1,
          messages: [...s.messages.slice(-199), { time: Date.now(), data, dir: 'in' }],
          lastMessage: data,
        }));
      } catch {
        update(s => ({
          ...s,
          seq: s.seq + 1,
          messages: [...s.messages.slice(-199), { time: Date.now(), raw: event.data, dir: 'in' }],
        }));
      }
    };

    ws.onerror = (err) => {
      console.error('[WS] Error:', err);
    };

    ws.onclose = () => {
      update(s => ({ ...s, status: 'disconnected' }));
      scheduleReconnect();
    };
  }

  function scheduleReconnect() {
    if (reconnectTimer) clearTimeout(reconnectTimer);
    update(s => ({ ...s, status: 'reconnecting' }));
    reconnectTimer = setTimeout(() => {
      reconnectDelay = Math.min(reconnectDelay * 2, MAX_RECONNECT_DELAY);
      doConnect();
    }, reconnectDelay);
  }

  function send(data) {
    if (!ws || ws.readyState !== WebSocket.OPEN) {
      console.warn('[WS] Not connected, cannot send');
      return false;
    }
    const msg = typeof data === 'string' ? data : JSON.stringify(data);
    ws.send(msg);
    update(s => ({
      ...s,
      seq: s.seq + 1,
      messages: [...s.messages.slice(-199), { time: Date.now(), data: JSON.parse(msg), dir: 'out' }],
    }));
    return true;
  }

  function disconnect() {
    if (reconnectTimer) {
      clearTimeout(reconnectTimer);
      reconnectTimer = null;
    }
    reconnectDelay = INITIAL_RECONNECT_DELAY;
    if (ws) {
      ws.onclose = null;
      ws.close();
      ws = null;
    }
    set({ status: 'disconnected', messages: [], lastMessage: null, seq: 0 });
  }

  function clearLog() {
    update(s => ({ ...s, messages: [], seq: 0 }));
  }

  return { subscribe, connect, send, disconnect, clearLog };
}

export const ws = createWebSocketStore();
