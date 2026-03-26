import express from "express";
import { createServer } from "http";
import { Server } from "socket.io";
import { createServer as createViteServer } from "vite";
import path from "path";
import fs from "fs";
import session from "express-session";
import cookieParser from "cookie-parser";
import connectSqlite3 from "connect-sqlite3";

import { initializeDb } from "./db";
import authRouter from "./auth";
import { setupMatchmaking } from "./matchmaking";
import { getSkins } from "./skins";

const SQLiteStore = connectSqlite3(session);
const app = express();
const httpServer = createServer(app);
const io = new Server(httpServer);
const PORT = process.env.PORT || 3000;

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
  });
}

startServer();
