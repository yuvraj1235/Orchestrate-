//server.js
const express = require('express');
const dgram = require('dgram');
const net = require('net');
const fs = require('fs');
const multer = require('multer');

const app = express();
const upload = multer({ dest: 'uploads/' });

const UDP_PORT = 9091;
const TCP_PORT = 9090;
let routingTable = {};

// 1. Listen for UDP Heartbeats
const udpServer = dgram.createSocket('udp4');
udpServer.on('message', (msg, rinfo) => {
    try {
        const load = msg.readDoubleLE(0);
        routingTable[rinfo.address] = {
            load: load.toFixed(2),
            lastSeen: Date.now()
        };
    } catch (e) { console.error("UDP Parse Error"); }
});
udpServer.bind(UDP_PORT);

// Clean up dead nodes (no heartbeat for 10 seconds)
setInterval(() => {
    const now = Date.now();
    for (let ip in routingTable) {
        if (now - routingTable[ip].lastSeen > 10000) delete routingTable[ip];
    }
}, 5000);

app.use(express.static('public'));

app.get('/status', (req, res) => res.json(routingTable));

app.post('/submit', upload.single('file'), (req, res) => {
    if (!req.file) return res.status(400).send("No file uploaded.");

    const sourcePath = req.file.path;

    // Find node with minimum load
    let bestIp = null;
    let minLoad = Infinity;
    for (let ip in routingTable) {
        if (parseFloat(routingTable[ip].load) < minLoad) {
            minLoad = routingTable[ip].load;
            bestIp = ip;
        }
    }

    if (!bestIp) {
        fs.unlinkSync(sourcePath);
        return res.status(500).send("No worker nodes active.");
    }

    const client = new net.Socket();
    let output = "";

    client.connect(TCP_PORT, bestIp, () => {
        const stats = fs.statSync(sourcePath);
        const sizeBuf = Buffer.alloc(8);
        // Ensure 8-byte write for C's 'long' or 'int64_t'
        sizeBuf.writeBigInt64LE(BigInt(stats.size));
        client.write(sizeBuf);

        const fileStream = fs.createReadStream(sourcePath);
        fileStream.pipe(client);
    });

    client.on('data', (data) => { output += data.toString(); });

    client.on('end', () => {
        if (!res.headersSent) {
            res.send(`<h2>Result from ${bestIp}</h2><pre>${output}</pre><a href="/">Go Back</a>`);
        }
        if (fs.existsSync(sourcePath)) fs.unlinkSync(sourcePath);
    });

    client.on('error', (err) => {
        if (!res.headersSent) res.status(500).send("Worker communication error.");
        if (fs.existsSync(sourcePath)) fs.unlinkSync(sourcePath);
    });
});

app.listen(3000, () => console.log('Web UI: http://localhost:3000'));