const net = require('net');
const dgram = require('dgram');
const express = require('express');
const path = require('path');
const app = express();
const PORT = 3000;

let discoveredServers = [];

// 1. Discover Servers via UDP Broadcast (Port 9001)
function scanNetwork() {
    const client = dgram.createSocket('udp4');
    
    client.on('error', (err) => {
        console.error(`UDP error: ${err.stack}`);
        client.close();
    });

    client.bind(() => { 
        client.setBroadcast(true); 
        const message = Buffer.from("DISCOVER");
        // Sending to 255.255.255.255 to reach all devices on the local network
        client.send(message, 9001, '255.255.255.255');
    });

    client.on('message', (msg, rinfo) => {
        if (msg.toString().includes("ALIVE")) {
            // Check if server is already in our list
            if (!discoveredServers.find(s => s.ip === rinfo.address)) {
                discoveredServers.push({ ip: rinfo.address, load: '...', status: 'Online' });
            }
        }
    });

    // Close the socket after 2 seconds to free the port
    setTimeout(() => client.close(), 2000); 
}

// 2. Query TCP Load for a specific IP (Port 9000)
function getTcpLoad(ip) {
    return new Promise((resolve) => {
        const client = new net.Socket();
        client.setTimeout(1500); // 1.5 second timeout
        
        client.connect(9000, ip, () => {
            client.write('LOAD');
        });

        client.on('data', (data) => {
            resolve(data.toString().trim());
            client.destroy();
        });

        client.on('error', () => {
            resolve('Offline');
            client.destroy();
        });

        client.on('timeout', () => {
            resolve('Timeout');
            client.destroy();
        });
    });
}

// ROUTE: Serve the Dashboard (index.html)
app.get('/', (req, res) => {
    // Changed __current_dir to __dirname
    res.sendFile(path.join(__dirname, 'index.html'));
});

// API: Get current list of servers and their loads
app.get('/api/servers', async (req, res) => {
    for (let server of discoveredServers) {
        server.load = await getTcpLoad(server.ip);
    }
    res.json(discoveredServers);
});

// API: Trigger a network scan
app.post('/api/scan', (req, res) => {
    scanNetwork();
    res.send("Scanning started...");
});

app.listen(PORT, () => {
    console.log(`=========================================`);
    console.log(`Monitor API running on http://localhost:${PORT}`);
    console.log(`Press Ctrl+C to stop`);
    console.log(`=========================================`);
    
    // Auto-scan once at startup
    scanNetwork();
});