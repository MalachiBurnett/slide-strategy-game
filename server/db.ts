import sqlite3pkg from "sqlite3";
const sqlite3 = sqlite3pkg.verbose();
import path from "path";

const dbPath = path.join(process.cwd(), "game.db");
export const db = new sqlite3.Database(dbPath, (err) => {
  if (err) {
    console.error("Database connection error:", err);
  } else {
    console.log("Connected to SQLite database.");
  }
});

export function initializeDb() {
  return new Promise<void>((resolve, reject) => {
    db.serialize(() => {
      db.run(`CREATE TABLE IF NOT EXISTS users (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        username TEXT UNIQUE,
        password TEXT,
        elo INTEGER DEFAULT 600,
        is_guest BOOLEAN DEFAULT 0,
        skin TEXT DEFAULT 'classic',
        theme TEXT DEFAULT 'wooden',
        unlocked_skins TEXT DEFAULT '["classic"]',
        wins INTEGER DEFAULT 0,
        games_played INTEGER DEFAULT 0
      )`, (err) => { if (err) reject(err); });

      const userMigrations = [
        { col: "is_guest", type: "BOOLEAN DEFAULT 0" },
        { col: "skin", type: "TEXT DEFAULT 'classic'" },
        { col: "theme", type: "TEXT DEFAULT 'wooden'" },
        { col: "unlocked_skins", type: "TEXT DEFAULT '[\"classic\"]'" },
        { col: "wins", type: "INTEGER DEFAULT 0" },
        { col: "games_played", type: "INTEGER DEFAULT 0" }
      ];

      userMigrations.forEach(m => {
        db.run(`ALTER TABLE users ADD COLUMN ${m.col} ${m.type}`, (err: any) => {
          if (err && !err.message.includes("duplicate column name")) {
            console.error(`Migration error (adding ${m.col}):`, err.message);
          }
        });
      });

      db.run(`CREATE TABLE IF NOT EXISTS games (
        id TEXT PRIMARY KEY,
        board TEXT,
        turn TEXT,
        player_w TEXT,
        player_b TEXT,
        status TEXT,
        winner TEXT,
        is_private BOOLEAN,
        code TEXT,
        timer_w INTEGER,
        timer_b INTEGER,
        increment INTEGER,
        last_move_time INTEGER,
        variant TEXT DEFAULT 'classic',
        is_rated BOOLEAN DEFAULT 1
      )`, (err) => { if (err) reject(err); });

      const gameMigrations = [
        { name: "is_private", type: "BOOLEAN" },
        { name: "code", type: "TEXT" },
        { name: "timer_w", type: "INTEGER" },
        { name: "timer_b", type: "INTEGER" },
        { name: "increment", type: "INTEGER" },
        { name: "last_move_time", type: "INTEGER" },
        { name: "variant", type: "TEXT DEFAULT 'classic'" },
        { name: "is_rated", type: "BOOLEAN DEFAULT 1" }
      ];

      gameMigrations.forEach(col => {
        db.run(`ALTER TABLE games ADD COLUMN ${col.name} ${col.type}`, (err: any) => {
          if (err && !err.message.includes("duplicate column name")) {
            console.error(`Migration error (adding ${col.name}):`, err.message);
          }
        });
      });

      resolve();
    });
  });
}
