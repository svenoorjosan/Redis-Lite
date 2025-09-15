const net = require('net');
const express = require('express');
const app = express();

const REDIS_HOST = process.env.REDIS_HOST || '127.0.0.1';
const REDIS_PORT = +(process.env.REDIS_PORT || 6380);

function reqBulk(args) {
  // Build RESP for an array of bulk strings
  let out = `*${args.length}\r\n`;
  for (const a of args) out += `$${Buffer.byteLength(a)}\r\n${a}\r\n`;
  return Buffer.from(out);
}

// Request INFO and parse bulk reply
function fetchInfo() {
  return new Promise((resolve, reject) => {
    const sock = net.createConnection({ host: REDIS_HOST, port: REDIS_PORT });
    let buf = Buffer.alloc(0);
    let need = null; // total bytes needed for full bulk reply

    sock.on('connect', () => sock.write(reqBulk(['INFO'])));
    sock.on('data', chunk => {
      buf = Buffer.concat([buf, chunk]);
      // Expect: $<len>\r\n<payload>\r\n
      const idx = buf.indexOf('\r\n');
      if (idx === -1) return;
      if (buf[0] !== 36 /*'$'*/) return reject(new Error('Unexpected reply'));
      const len = parseInt(buf.slice(1, idx).toString(), 10);
      if (Number.isNaN(len) || len < 0) return resolve({}); // nil bulk
      const headerLen = idx + 2;
      need = headerLen + len + 2;
      if (buf.length >= need) {
        const payload = buf.slice(headerLen, headerLen + len).toString();
        sock.destroy();
        const out = {};
        for (const line of payload.split('\n')) {
          const p = line.indexOf(':');
          if (p > 0) out[line.slice(0, p)] = line.slice(p + 1).trim();
        }
        resolve(out);
      }
    });
    sock.on('error', reject);
    // Safety: if server keeps connection open, close after a moment
    sock.setTimeout(500, () => sock.end());
  });
}

app.get('/metrics', async (_req, res) => {
  try {
    const m = await fetchInfo();
    res.json(m);
  } catch (e) {
    res.status(500).json({ error: String(e) });
  }
});

app.use(express.static('public'));
const port = +(process.env.PORT || 8080);
app.listen(port, () => console.log(`dashboard on :${port}`));
