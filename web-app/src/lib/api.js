export async function fetchStatus(host, useProxy) {
  try {
    const url = useProxy
      ? `/api/status`
      : `http://${host}/api/status`;
    const resp = await fetch(url);
    if (!resp.ok) throw new Error(`HTTP ${resp.status}`);
    return await resp.json();
  } catch (err) {
    console.error('[API] fetchStatus failed:', err);
    return null;
  }
}
