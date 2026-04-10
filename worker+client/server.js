const express = require('express');
const dgram = require('dgram');
const net = require('net');
const fs = require('fs');
const multer = require('multer');
const cors = require('cors');

const app = express();
app.use(cors()); // Fixes the CSP/CORS issues
const upload = multer({ dest: 'uploads/' });

const UDP_PORT = 9091;
const TCP_PORT = 9090;
let routingTable = {};

// 1. Listen for UDP Heartbeats from C Workers
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

// Clean up dead nodes
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

    // Load Balancing: Find node with minimum load
    let bestIp = null;
    let minLoad = Infinity;
    for (let ip in routingTable) {
        let currentLoad = parseFloat(routingTable[ip].load);
        if (currentLoad < minLoad) {
            minLoad = currentLoad;
            bestIp = ip;
        }
    }

    if (!bestIp) {
        if (fs.existsSync(sourcePath)) fs.unlinkSync(sourcePath);
        return res.status(500).send("No worker nodes active.");
    }

    const client = new net.Socket();
    let output = "";

    client.connect(TCP_PORT, bestIp, () => {
        const stats = fs.statSync(sourcePath);
        // Create 8-byte buffer for the C 'int64_t'
        const sizeBuf = Buffer.alloc(8);
        sizeBuf.writeBigInt64LE(BigInt(stats.size));
        client.write(sizeBuf);

        const fileStream = fs.createReadStream(sourcePath);
        fileStream.pipe(client);
    });

    client.on('data', (data) => { output += data.toString(); });

    client.on('end', () => {
        res.send(`<h2>Result from ${bestIp} (Load: ${minLoad}%)</h2><pre>${output}</pre><br><a href="/">Back</a>`);
        if (fs.existsSync(sourcePath)) fs.unlinkSync(sourcePath);
    });

    client.on('error', (err) => {
        if (!res.headersSent) res.status(500).send("Worker Node Connection Failed.");
        if (fs.existsSync(sourcePath)) fs.unlinkSync(sourcePath);
    });
});

app.listen(3000, () => console.log('Web Server running at http://localhost:3000'));