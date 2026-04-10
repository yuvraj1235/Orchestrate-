const express = require('express');
const dgram = require('dgram');
const net = require('net');
const fs = require('fs');
const multer = require('multer');
const cors = require('cors');

const app = express();
app.use(cors());
const upload = multer({ dest: 'uploads/' });

const UDP_PORT = 9091;
let routingTable = {};

// 1. Listen for UDP Heartbeats
const udpServer = dgram.createSocket('udp4');
udpServer.on('message', (msg, rinfo) => {
    try {
        // Match the C struct: [int32_t (4 bytes), double (8 bytes)]
        const tcpPort = msg.readInt32LE(0);
        const load = msg.readDoubleLE(4);
        
        const nodeKey = `${rinfo.address}:${tcpPort}`;
        routingTable[nodeKey] = {
            ip: rinfo.address,
            port: tcpPort,
            load: load.toFixed(2),
            lastSeen: Date.now()
        };
    } catch (e) { console.error("UDP Parse Error"); }
});
udpServer.bind(UDP_PORT);

// Cleanup inactive nodes
setInterval(() => {
    const now = Date.now();
    for (let key in routingTable) {
        if (now - routingTable[key].lastSeen > 8000) delete routingTable[key];
    }
}, 4000);

app.get('/status', (req, res) => res.json(routingTable));

app.post('/submit', upload.single('file'), (req, res) => {
    if (!req.file) return res.status(400).send("No file uploaded.");

    // FIND COLD NODE (Lowest Load)
    let bestNode = null;
    let minLoad = Infinity;

    for (let key in routingTable) {
        if (parseFloat(routingTable[key].load) < minLoad) {
            minLoad = routingTable[key].load;
            bestNode = routingTable[key];
        }
    }

    if (!bestNode) {
        fs.unlinkSync(req.file.path);
        return res.status(500).send("No worker nodes active.");
    }

    // CONNECT AND SEND
    const client = new net.Socket();
    let result = "";

    client.connect(bestNode.port, bestNode.ip, () => {
        const stats = fs.statSync(req.file.path);
        const sizeBuf = Buffer.alloc(8);
        sizeBuf.writeBigInt64LE(BigInt(stats.size));
        client.write(sizeBuf);
        fs.createReadStream(req.file.path).pipe(client);
    });

    client.on('data', (data) => { result += data.toString(); });
    client.on('end', () => {
        res.send(`<h3>Success!</h3><p>Executed by Node ${bestNode.port}</p><pre>${result}</pre><a href="/">Back</a>`);
        fs.unlinkSync(req.file.path);
    });
    client.on('error', (err) => {
        res.status(500).send("Connection to worker failed.");
        if(fs.existsSync(req.file.path)) fs.unlinkSync(req.file.path);
    });
});

app.listen(3000, () => console.log('Orchestrator: http://localhost:3000'));