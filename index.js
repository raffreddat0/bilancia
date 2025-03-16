const express = require('express');
const sqlite3 = require('sqlite3').verbose();
const path = require('path');
const http = require('http');
const { Server } = require('ws');
require('dotenv').config();

const app = express();
const port = 7777;
let prediciton = null; 

app.use(express.text());

const db = new sqlite3.Database('./db.sqlite', (err) => {
  if (err) {
    console.error('Errore nella connessione al database:', err.message);
  } else {
    console.log('Connesso al database SQLite.');
  }
});

app.get('/bilancia', (req, res) => {
  res.sendFile(path.join(__dirname, './index.html'));
});

app.get('/bilancia/admin', (req, res) => {
  res.sendFile(path.join(__dirname, './admin.html'));
});


app.get('/bilancia/data', (req, res) => {
  res.send(prediciton?.name || "null");
});

app.post('/bilancia/db', (req, res) => {
  const query = req.body;
  db.all(query, function (err, rows) {
    if (err) {
      res.status(500).send("error");
      return;
    }
    res.status(201).json(rows);
  });
});

app.patch('/bilancia/db', (req, res) => {
  if (req.headers.authorization !== process.env.auth) {
    res.status(401).send("unauthorized");
    return;
  }

  const query = req.body;
  if (query === "auth")
    return res.status(201).send("success");

  db.run(query, function (err) {
    if (err) {
      res.status(500).send(err.message);
      return;
    }
    res.status(201).send("success");
  });
});

const server = http.createServer(app);
const wss = new Server({ server, path: "/bilancia" });

wss.on('connection', (ws, req) => {
  const urlParams = new URLSearchParams(req.url.split('?')[1]);
  const auth = urlParams.get('auth');

  if (!auth || process.env.auth !== auth) {
    console.log(`auth non valido`);
    ws.close();
    return;
  }

  console.log('Client connesso');
  ws.send("start");

  let pongTimeout;
  const heartbeat = () => {
    if (ws.readyState === ws.OPEN) {
      ws.ping();

      pongTimeout = setTimeout(() => {
        try {
          ws.terminate();
        } catch {}
      }, 5000);
    }
  };

  const heartbeatInterval = setInterval(heartbeat, 30000);

  ws.on('pong', (data) => {
    clearTimeout(pongTimeout); 
  });

  ws.on('message', async (data) => {
    const message = JSON.parse(data.toString());

    if ((prediciton?.probability || 0) < message.probability)
      prediciton = message;
  });

  ws.on('close', () => {
    console.log('Client disconnesso');
    clearInterval(heartbeatInterval);
    clearTimeout(pongTimeout);
  });
});

server.listen(port, '0.0.0.0', () => {
  console.log(`Server in ascolto sulla porta ${port}`);
});

process.on('uncaughtException', (error) => {
  console.error(error);
});

process.on('unhandledRejection', (reason) => {
  console.error(reason);
});