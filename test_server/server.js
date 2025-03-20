import WebSocket, { WebSocketServer } from 'ws';

const wss = new WebSocketServer({
    port: 8080
});

const clients = [];

wss.on("connection", ws => {
    const index = clients.push(ws) - 1;
    ws.on("message", data => {
        clients.forEach((client, i) => {
            if(i === index) return;
            client.send(data);
        });
    });
});