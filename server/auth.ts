import express from "express";
import bcrypt from "bcryptjs";
import { db } from "./db";
import { getSkins, updateUnlockedSkins } from "./skins";
import { Resend } from "resend";
import { v4 as uuidv4 } from "uuid";

const router = express.Router();
const resend = new Resend("re_GKiAm1ZA_FR6THpnvaNp4fLBmiEMj3g5b");

router.get("/leaderboard", (req, res) => {
  db.all("SELECT id, username, elo FROM users WHERE is_guest = 0 ORDER BY elo DESC LIMIT 10", (err, rows) => {
    if (err) return res.status(500).json({ error: "Error fetching leaderboard" });
    res.json(rows);
  });
});

router.post("/register", (req, res) => {
  const { username, email, password } = req.body;
  if (!username || !email || !password) return res.status(400).json({ error: "Missing fields" });
  
  const hash = bcrypt.hashSync(password, 10);
  const token = uuidv4();
  
  db.run("INSERT INTO users (username, email, password, elo, verification_token, is_verified) VALUES (?, ?, ?, ?, ?, ?)", 
    [username, email, hash, 600, token, 0], async function(err) {
    if (err) {
      if (err.message.includes("UNIQUE constraint failed: users.username")) {
        return res.status(400).json({ error: "Username taken" });
      }
      if (err.message.includes("UNIQUE constraint failed: users.email")) {
        return res.status(400).json({ error: "Email already registered" });
      }
      return res.status(500).json({ error: "Database error" });
    }

    try {
      await resend.emails.send({
        from: "verify@slide.wiizardsoftware.uk",
        to: email,
        subject: "Verify your Slide account",
        html: `<p>Hello ${username},</p><p>Please verify your email by clicking the link below:</p><p><a href="https://slide.wiizardsoftware.uk/verify/${token}">Verify Email</a></p>`
      });
      res.json({ success: true, message: "Verification email sent!" });
    } catch (sendErr) {
      console.error("Email send error:", sendErr);
      res.json({ success: true, message: "User registered, but failed to send verification email." });
    }
  });
});

router.post("/verify/:token", (req, res) => {
  const { token } = req.params;
  db.get("SELECT * FROM users WHERE verification_token = ?", [token], (err, user: any) => {
    if (err || !user) return res.status(400).json({ error: "Invalid token" });
    db.run("UPDATE users SET is_verified = 1, verification_token = NULL WHERE id = ?", [user.id], (updErr) => {
      if (updErr) return res.status(500).json({ error: "Verification failed" });
      res.json({ success: true });
    });
  });
});

router.post("/login", (req, res) => {
  const { username, password } = req.body; // username can be username or email
  db.get("SELECT * FROM users WHERE username = ? OR email = ?", [username, username], (err, user: any) => {
    if (err || !user || !bcrypt.compareSync(password, user.password)) {
      return res.status(400).json({ error: "Invalid credentials" });
    }
    if (user.is_guest === 0 && user.is_verified === 0) {
      return res.status(403).json({ error: "Please verify your email before logging in" });
    }
    (req.session as any).userId = user.id;
    (req.session as any).username = user.username;
    res.json({ 
      id: user.id, 
      username: user.username, 
      elo: user.elo, 
      wins: user.wins || 0,
      gamesPlayed: user.games_played || 0,
      gamesRandomSetup: user.games_random_setup || 0,
      games1min: user.games_1min || 0,
      gamesFogOfWar: user.games_fog_of_war || 0,
      skin: user.skin, 
      theme: user.theme,
      unlockedSkins: JSON.parse(user.unlocked_skins || '["classic"]')
    });
  });
});

router.post("/logout", (req, res) => {
  req.session.destroy((err) => {
    if (err) return res.status(500).json({ error: "Logout failed" });
    res.clearCookie('connect.sid');
    res.json({ success: true });
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
      gamesRandomSetup: user.games_random_setup || 0,
      games1min: user.games_1min || 0,
      gamesFogOfWar: user.games_fog_of_war || 0,
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
