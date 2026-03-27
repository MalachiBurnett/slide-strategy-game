import express from "express";
import { createServer } from "http";
import { Server } from "socket.io";
import { createServer as createViteServer } from "vite";
import path from "path";
import fs from "fs";
import session from "express-session";
import cookieParser from "cookie-parser";
import connectSqlite3 from "connect-sqlite3";
import dotenv from "dotenv";

dotenv.config();

import { initializeDb } from "./db";
import authRouter from "./auth";
import { setupMatchmaking } from "./matchmaking";
import { getSkins } from "./skins";
import { db } from "./db";

const SQLiteStore = connectSqlite3(session);
const app = express();
const httpServer = createServer(app);
const io = new Server(httpServer);
const PORT = Number(process.env.PORT) || 3000;

async function startServer() {
  await initializeDb();

  app.use(express.json());
  app.use(cookieParser());
  app.use(session({
    store: new SQLiteStore({
      db: 'game.db',
      table: 'sessions',
      dir: '.'
    }) as any,
    secret: "slide-secret-key",
    resave: false,
    saveUninitialized: true,
    cookie: { secure: false, sameSite: 'lax' }
  }));

  // API Routes
  app.use("/api", authRouter);

  app.get("/api/skins", (req, res) => {
    res.json(getSkins());
  });

  app.get("/api/skins/:skinId/:color.svg", (req, res) => {
    const { skinId, color } = req.params;
    const svgPath = path.join(process.cwd(), 'skins', skinId, `${color}.svg`);
    if (fs.existsSync(svgPath)) {
      res.setHeader('Content-Type', 'image/svg+xml');
      res.sendFile(svgPath);
    } else {
      res.status(404).send('Not found');
    }
  });

  setupMatchmaking(io);

  if (process.env.NODE_ENV !== "production") {
    const vite = await createViteServer({
      server: { middlewareMode: true },
      appType: "spa",
    });
    app.use(vite.middlewares);
  } else {
    const distPath = path.join(process.cwd(), 'dist');
    app.use(express.static(distPath));
    app.get('*', (req, res) => {
      res.sendFile(path.join(distPath, 'index.html'));
    });
  }

  httpServer.listen(PORT, "0.0.0.0", () => {
    console.log(`Server running on http://localhost:${PORT}`);
    console.log(`Type 'ban <username>' to ban a user.`);
  });

  process.stdin.on('data', (data) => {
    const text = data.toString().trim();
    if (text.startsWith('ban ')) {
      const username = text.split(' ')[1];
      if (username) {
        db.run("UPDATE users SET is_banned = 1 WHERE username = ?", [username], function(err) {
          if (err) console.error("Error banning user:", err);
          else if (this.changes > 0) {
            console.log(`User ${username} has been banned.`);
            db.run("INSERT OR IGNORE INTO banned_names (name) VALUES (?)", [username], (err) => {
              if (err) console.error("Error adding to banned_names:", err);
            });
          } else {
            console.log(`User ${username} not found.`);
          }
        });
      }
    } else if (text.startsWith('unban ')) {
      const username = text.split(' ')[1];
      if (username) {
        db.run("UPDATE users SET is_banned = 0 WHERE username = ?", [username], function(err) {
          if (err) console.error("Error unbanning user:", err);
          else if (this.changes > 0) {
            console.log(`User ${username} has been unbanned.`);
          } else {
            console.log(`User ${username} not found.`);
          }
        });
      }
    }
  });
}

startServer();
