import express from "express";
import bcrypt from "bcryptjs";
import { db } from "./db";
import { getSkins, updateUnlockedSkins } from "./skins";

const router = express.Router();

router.get("/leaderboard", (req, res) => {
  db.all("SELECT id, username, elo FROM users WHERE is_guest = 0 ORDER BY elo DESC LIMIT 10", (err, rows) => {
    if (err) return res.status(500).json({ error: "Error fetching leaderboard" });
    res.json(rows);
  });
});

router.post("/register", (req, res) => {
  const { username, password } = req.body;
  if (!username || !password) return res.status(400).json({ error: "Missing fields" });
  const hash = bcrypt.hashSync(password, 10);
  db.run("INSERT INTO users (username, password, elo) VALUES (?, ?, ?)", [username, hash, 600], function(err) {
    if (err) return res.status(400).json({ error: "Username taken" });
    res.json({ success: true });
  });
});

router.post("/login", (req, res) => {
  const { username, password } = req.body;
  db.get("SELECT * FROM users WHERE username = ?", [username], (err, user: any) => {
    if (err || !user || !bcrypt.compareSync(password, user.password)) {
      return res.status(400).json({ error: "Invalid credentials" });
    }
    (req.session as any).userId = user.id;
    (req.session as any).username = user.username;
    res.json({ 
      id: user.id, 
      username: user.username, 
      elo: user.elo, 
      wins: user.wins || 0,
      gamesPlayed: user.games_played || 0,
      skin: user.skin, 
      theme: user.theme,
      unlockedSkins: JSON.parse(user.unlocked_skins || '["classic"]')
    });
  });
});

router.post("/guest", (req, res) => {
  const guestName = "Guest_" + Math.random().toString(36).substring(7);
  db.run("INSERT INTO users (username, elo, is_guest) VALUES (?, ?, ?)", [guestName, 400, 1], function(err) {
    if (err) return res.status(500).json({ error: "Error creating guest" });
    const guestId = this.lastID;
    (req.session as any).userId = guestId;
    (req.session as any).username = guestName;
    res.json({ id: guestId, username: guestName, elo: 400, wins: 0, gamesPlayed: 0, skin: 'classic', theme: 'wooden', unlockedSkins: ['classic'] });
  });
});

router.get("/me", (req, res) => {
  const userId = (req.session as any).userId;
  if (!userId) return res.status(401).json({ error: "Not logged in" });
  db.get("SELECT * FROM users WHERE id = ?", [userId], (err, user: any) => {
    if (err || !user) return res.status(401).json({ error: "User not found" });
    res.json({
      id: user.id,
      username: user.username,
      elo: user.elo,
      wins: user.wins || 0,
      gamesPlayed: user.games_played || 0,
      skin: user.skin,
      theme: user.theme,
      unlockedSkins: JSON.parse(user.unlocked_skins || '["classic"]')
    });
  });
});

router.post("/cosmetics", (req, res) => {
  const userId = (req.session as any).userId;
  if (!userId) return res.status(401).json({ error: "Not logged in" });
  const { skin, theme } = req.body;
  
  db.get("SELECT * FROM users WHERE id = ?", [userId], (err, user: any) => {
    if (err || !user) return res.status(401).json({ error: "User not found" });
    
    const unlockedSkins = JSON.parse(user.unlocked_skins || '["classic"]');
    const updates: string[] = [];
    const params: any[] = [];
    
    if (skin) {
      // SECURITY: Server-side check if skin is unlocked
      if (unlockedSkins.includes(skin)) {
        updates.push("skin = ?");
        params.push(skin);
      } else {
        return res.status(403).json({ error: "Skin not unlocked" });
      }
    }
    
    if (theme) {
      const validThemes = ['dark', 'light', 'beach', 'wooden', 'connect4', 'wii', 'oscar'];
      if (validThemes.includes(theme)) {
        updates.push("theme = ?");
        params.push(theme);
      } else {
        return res.status(400).json({ error: "Invalid theme" });
      }
    }
    
    if (updates.length === 0) return res.json({ success: true });
    
    params.push(userId);
    db.run(`UPDATE users SET ${updates.join(", ")} WHERE id = ?`, params, (err) => {
      if (err) return res.status(500).json({ error: "Update failed" });
      res.json({ success: true });
    });
  });
});

export default router;
