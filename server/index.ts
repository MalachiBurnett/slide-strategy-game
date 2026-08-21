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

import { runAllTests } from "./tests";

const SQLiteStore = connectSqlite3(session);
const app = express();
const httpServer = createServer(app);
const io = new Server(httpServer);
const PORT = Number(process.env.PORT) || 3000;

async function startServer() {
  await initializeDb();
  await runAllTests();

  app.use(express.json());
  app.use(cookieParser());

  const sessionMiddleware = session({
    store: new SQLiteStore({
      db: 'game.db',
      table: 'sessions',
      dir: '.'
    }) as any,
    secret: "slide-secret-key",
    resave: false,
    // Was true, which wrote a sessions row for every request that arrived
    // without a cookie. The 3DS client has no cookie jar, so it never sent
    // connect.sid back and every one of its requests - including a status
    // poll twice a second - created a brand new session and INSERTed it into
    // game.db, on the critical path of the response and contending with the
    // same file the move handler updates. Browsers return the cookie, so
    // with resave:false they were writing nothing; this was purely a 3DS
    // cost. Nothing here needs the empty session persisted: every route that
    // uses one assigns req.session.userId first, which marks it modified and
    // saves it regardless.
    saveUninitialized: false,
    cookie: { secure: false, sameSite: 'lax' }
  });

  // The 3DS endpoints authenticate with an authCode in the body and have no
  // use for a session at all, so skip loading one for them entirely rather
  // than doing a store lookup per poll.
  app.use((req, res, next) => {
    if (req.path.startsWith("/api/3ds/")) return next();
    return sessionMiddleware(req, res, next);
  });

  // API Routes
  app.use("/api", authRouter);

  // Legal pages
  app.get("/privacy", (req, res) => {
    res.sendFile(path.join(process.cwd(), "server", "public", "privacy.html"));
  });

  app.get("/terms", (req, res) => {
    res.sendFile(path.join(process.cwd(), "server", "public", "terms.html"));
  });

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

  app.get("/3ds-download/", (req, res) => {
    const ciaPath = path.join(process.cwd(), "3ds-client", "slide-3ds.cia");
    res.download(ciaPath, "slide-3ds.cia");
  });

  setupMatchmaking(io, app);

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
