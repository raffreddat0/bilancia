const express = require('express');
const sqlite3 = require('sqlite3').verbose();
const path = require('path');
const http = require('http');
const { Server } = require('ws');
const { writeFileSync } = require('fs');
require('dotenv').config();

const app = express();
const port = 7777;
let prediciton = null; 

app.use(express.text());

const db = new sqlite3.Database('./database.sqlite', (err) => {
  if (err) {
    console.error('Errore nella connessione al database:', err.message);
  } else {
    console.log('Connesso al database SQLite.');
  }
});

app.get('/', (req, res) => {
  res.sendFile(path.join(__dirname, './index.html'));
});

app.get('/admin', (req, res) => {
  res.sendFile(path.join(__dirname, './admin.html'));
});

app.get('/model', (req, res) => {
  res.send(process.env.model);
});

app.patch('/settings', (req, res) => {
  if (req.headers.authorization !== process.env.auth) {
    res.status(401).send("unauthorized");
    return;
  }

  const query = req.body;
  if (query === "auth")
    return res.status(201).send("success");

  if (query.startsWith("model=")) {
    writeFileSync("./.env", `auth=${process.env.auth}\n${query}`);
    process.env.model = query.split("=")[1];
    res.send("success");
  }

  if (query.startsWith("auth=")) {
    writeFileSync("./.env", `${query}\nmodel=${process.env.model}`);
    process.env.auth = query.split("=")[1];
    res.send("success");
  }
});

app.get('/data', (req, res) => {
  res.send(prediciton?.name || "null");
});

app.get('/db', (req, res) => {
  const query = "SELECT * FROM foods";

  db.all(query, (err, rows) => {
    if (err) {
      res.status(500).send("Error fetching data");
      return;
    }

    let table = `
      <html>
      <head>
        <title>Database (Foods)</title>
        <style>
          table { border-collapse: collapse; width: 100%; }
          th, td { border: 1px solid black; padding: 8px; text-align: left; }
          th { background-color: #f2f2f2; }
        </style>
      </head>
      <body>
        <h1>Food Table</h1>
        <table>
          <tr>
    `;

    if (rows.length > 0) {
      const columns = Object.keys(rows[0]);
      columns.forEach(col => {
        table += `<th>${col}</th>`;
      });
      table += `</tr>`;

      rows.forEach(row => {
        table += `<tr>`;
        columns.forEach(col => {
          table += `<td>${row[col]}</td>`;
        });
        table += `</tr>`;
      });
    } else {
      table += `<tr><td colspan="100%">No data available</td></tr>`;
    }

    table += `
        </table>
      </body>
      </html>
    `;

    res.send(table);
  });
});

app.post('/db', (req, res) => {
  const query = req.body;
  db.all(query, function (err, rows) {
    if (err) {
      res.status(500).send(err.message);
      return;
    }
    res.status(201).json(rows);
  });
});

app.patch('/db', (req, res) => {
  if (req.headers.authorization !== process.env.auth) {
    res.status(401).send("unauthorized");
    return;
  }

  const query = req.body;
  db.run(query, function (err) {
    if (err) {
      res.status(500).send(err.message);
      return;
    }
    res.status(201).send("success");
  });
});

app.use((req, res) => {
  res.redirect('/');  
});

const server = http.createServer(app);
const wss = new Server({ server });

wss.on('connection', (ws, req) => {
  const urlParams = new URLSearchParams(req.url.split('?')[1]);
  const auth = urlParams.get('auth');
  const arduino = process.env.auth !== auth ;

  if (auth && process.env.auth !== auth) {
    console.log(`auth non valido`);
    ws.close();
    return;
  }

  console.log('Client connesso');
  if (!arduino)
    ws.send("start " + process.env.model);

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
    if (!arduino) {
      const message = JSON.parse(data.toString());

      if ((prediciton?.probability || 0) < message.probability)
        prediciton = message;
    } else {
      const message = data.toString();

      if (!message === "info" || !prediciton?.name)
        return;

      const query = `SELECT * FROM foods WHERE nome="${prediciton?.name}"`;
      db.all(query, function (err, rows) {
        if (err) {
          ws.send("error");
          return;
        }
        ws.send(JSON.stringify(rows));
      });
    }
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