async function tick() {
  const r = await fetch('/metrics');
  const m = await r.json();
  const el = document.getElementById('stats');
  el.innerHTML = `
<pre>
uptime:            ${m.uptime ?? '?'} s
connected_clients: ${m.connected_clients ?? '?'}
keys:              ${m.keys ?? '?'}
total_commands:    ${m.total_commands ?? '?'}
expired_keys:      ${m.expired_keys ?? '?'}
aof_bytes:         ${m.aof_bytes ?? '?'}
</pre>`;
}
tick();
setInterval(tick, 1000);
