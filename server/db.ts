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
        email TEXT UNIQUE,
        password TEXT,
        is_verified BOOLEAN DEFAULT 0,
        verification_token TEXT,
        elo INTEGER DEFAULT 600,
        is_guest BOOLEAN DEFAULT 0,
        skin TEXT DEFAULT 'classic',
        theme TEXT DEFAULT 'wooden',
        unlocked_skins TEXT DEFAULT '["classic"]',
        wins INTEGER DEFAULT 0,
        games_played INTEGER DEFAULT 0,
        games_random_setup INTEGER DEFAULT 0,
        games_1min INTEGER DEFAULT 0,
        games_fog_of_war INTEGER DEFAULT 0
      )`, (err) => { if (err) reject(err); });

      const userMigrations = [
        { col: "email", type: "TEXT UNIQUE" },
        { col: "is_verified", type: "BOOLEAN DEFAULT 0" },
        { col: "verification_token", type: "TEXT" },
        { col: "is_guest", type: "BOOLEAN DEFAULT 0" },
        { col: "skin", type: "TEXT DEFAULT 'classic'" },
        { col: "theme", type: "TEXT DEFAULT 'wooden'" },
        { col: "unlocked_skins", type: "TEXT DEFAULT '[\"classic\"]'" },
        { col: "wins", type: "INTEGER DEFAULT 0" },
        { col: "games_played", type: "INTEGER DEFAULT 0" },
        { col: "games_random_setup", type: "INTEGER DEFAULT 0" },
        { col: "games_1min", type: "INTEGER DEFAULT 0" },
        { col: "games_fog_of_war", type: "INTEGER DEFAULT 0" },
        { col: "is_banned", type: "BOOLEAN DEFAULT 0" },
        { col: "password_reset_token", type: "TEXT" },
        { col: "new_email", type: "TEXT" },
        { col: "spectators_count", type: "INTEGER DEFAULT 0" }
      ];

      userMigrations.forEach(m => {
        db.run(`ALTER TABLE users ADD COLUMN ${m.col} ${m.type}`, (err: any) => {
          if (err && !err.message.includes("duplicate column name")) {
            console.error(`Migration error (adding ${m.col}):`, err.message);
          }
        });
      });

      db.run(`CREATE TABLE IF NOT EXISTS banned_names (
        name TEXT PRIMARY KEY
      )`, (err) => { if (err) reject(err); });

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
        is_rated BOOLEAN DEFAULT 1,
        moves TEXT DEFAULT '[]',
        start_board TEXT,
        variant_data TEXT DEFAULT '[]'
      )`, (err) => { if (err) reject(err); });

      const gameMigrations = [
        { name: "is_private", type: "BOOLEAN" },
        { name: "code", type: "TEXT" },
        { name: "timer_w", type: "INTEGER" },
        { name: "timer_b", type: "INTEGER" },
        { name: "increment", type: "INTEGER" },
        { name: "last_move_time", type: "INTEGER" },
        { name: "variant", type: "TEXT DEFAULT 'classic'" },
        { name: "is_rated", type: "BOOLEAN DEFAULT 1" },
        { name: "skin_w", type: "TEXT DEFAULT 'classic'" },
        { name: "skin_b", type: "TEXT DEFAULT 'classic'" },
        { name: "moves", type: "TEXT DEFAULT '[]'" },
        { name: "start_board", type: "TEXT" },
        { name: "variant_data", type: "TEXT DEFAULT '[]'" }
      ];

      gameMigrations.forEach(col => {
        db.run(`ALTER TABLE games ADD COLUMN ${col.name} ${col.type}`, (err: any) => {
          if (err && !err.message.includes("duplicate column name")) {
            console.error(`Migration error (adding ${col.name}):`, err.message);
          }
        });
      });

      db.run(`CREATE TABLE IF NOT EXISTS spectating (
        spectator_id INTEGER,
        target_player_id INTEGER,
        PRIMARY KEY (spectator_id, target_player_id)
      )`, (err) => { if (err) reject(err); });
      
      db.run(`CREATE TABLE IF NOT EXISTS game_logs (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        game_id TEXT,
        player_w_id INTEGER,
        player_b_id INTEGER,
        player_w_username TEXT,
        player_b_username TEXT,
        player_w_elo INTEGER,
        player_b_elo INTEGER,
        winner TEXT,
        outcome TEXT,
        moves TEXT,
        variant TEXT,
        start_board TEXT,
        variant_data TEXT,
        is_rated BOOLEAN,
        timestamp DATETIME DEFAULT CURRENT_TIMESTAMP
      )`, (err) => { if (err) reject(err); });

      resolve();
    });
  });
}
