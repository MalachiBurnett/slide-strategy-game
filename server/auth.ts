import express from "express";
import bcrypt from "bcryptjs";
import { db } from "./db";
import { getSkins, updateUnlockedSkins } from "./skins";
import { Resend } from "resend";
import { v4 as uuidv4 } from "uuid";

const router = express.Router();
const resend = new Resend(process.env.RESEND_API_KEY);

router.get("/leaderboard", (req, res) => {
  db.all("SELECT id, username, elo FROM users WHERE is_guest = 0 ORDER BY elo DESC LIMIT 10", (err, rows) => {
    if (err) return res.status(500).json({ error: "Error fetching leaderboard" });
    res.json(rows);
  });
});

router.post("/register", (req, res) => {
  const { username, email, password } = req.body;
  if (!username || !email || !password) return res.status(400).json({ error: "Missing fields" });
  
  db.get("SELECT * FROM banned_names WHERE name = ?", [username], (err, banned) => {
    if (banned) return res.status(403).json({ error: "This name is banned and cannot be used." });

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
});

// Profile endpoints
router.post("/profile/username", (req, res) => {
  const userId = (req.session as any).userId;
  if (!userId) return res.status(401).json({ error: "Not logged in" });
  const { username } = req.body;
  if (!username) return res.status(400).json({ error: "Username required" });

  db.get("SELECT * FROM banned_names WHERE name = ?", [username], (err, banned) => {
    if (banned) return res.status(403).json({ error: "This name is banned." });

    db.run("UPDATE users SET username = ?, is_banned = 0 WHERE id = ?", [username, userId], function(err) {
      if (err) {
        if (err.message.includes("UNIQUE constraint failed")) return res.status(400).json({ error: "Username taken" });
        return res.status(500).json({ error: "Update failed" });
      }
      (req.session as any).username = username;
      res.json({ success: true, username });
    });
  });
});

router.post("/request-password-reset", (req, res) => {
  const { usernameOrEmail } = req.body;
  if (!usernameOrEmail) return res.status(400).json({ error: "Username or email required" });

  db.get("SELECT * FROM users WHERE username = ? OR email = ?", [usernameOrEmail, usernameOrEmail], async (err, user: any) => {
    if (!user || user.is_guest) {
      // Don't reveal whether the account exists or is a guest
      return res.json({ success: true, message: "If an account exists, a reset email has been sent." });
    }
    
    const token = uuidv4();
    db.run("UPDATE users SET password_reset_token = ? WHERE id = ?", [token, user.id]);
    
    try {
      await resend.emails.send({
        from: "security@slide.wiizardsoftware.uk",
        to: user.email,
        subject: "Slide Password Reset",
        html: `<p>Hello ${user.username},</p><p>Click the link below to reset your password:</p><p><a href="https://slide.wiizardsoftware.uk/reset-password/${token}">Reset Password</a></p>`
      });
      // Always return success message to avoid account enumeration
      res.json({ success: true, message: "If an account exists, a reset email has been sent." });
    } catch (e) {
      // Still return success to avoid revealing email send failures
      res.json({ success: true, message: "If an account exists, a reset email has been sent." });
    }
  });
});

router.post("/profile/password-reset-request", (req, res) => {
  const userId = (req.session as any).userId;
  if (!userId) return res.status(401).json({ error: "Not logged in" });

  db.get("SELECT * FROM users WHERE id = ?", [userId], async (err, user: any) => {
    if (!user || user.is_guest) return res.status(400).json({ error: "Cannot reset password for guest" });
    
    const token = uuidv4();
    db.run("UPDATE users SET password_reset_token = ? WHERE id = ?", [token, userId]);
    
    try {
      await resend.emails.send({
        from: "security@slide.wiizardsoftware.uk",
        to: user.email,
        subject: "Slide Password Reset",
        html: `<p>Hello ${user.username},</p><p>Click the link below to reset your password:</p><p><a href="https://slide.wiizardsoftware.uk/reset-password/${token}">Reset Password</a></p>`
      });
      res.json({ success: true, message: "Reset email sent!" });
    } catch (e) {
      res.status(500).json({ error: "Failed to send email" });
    }
  });
});

router.post("/profile/password-reset-confirm", (req, res) => {
  const { token, newPassword } = req.body;
  if (!token || !newPassword) return res.status(400).json({ error: "Missing fields" });

  const hash = bcrypt.hashSync(newPassword, 10);
  db.run("UPDATE users SET password = ?, password_reset_token = NULL WHERE password_reset_token = ?", [hash, token], function(err) {
    if (err || this.changes === 0) return res.status(400).json({ error: "Invalid or expired token" });
    res.json({ success: true });
  });
});

router.post("/profile/email-change-request", (req, res) => {
  const userId = (req.session as any).userId;
  if (!userId) return res.status(401).json({ error: "Not logged in" });
  const { newEmail } = req.body;
  if (!newEmail) return res.status(400).json({ error: "Email required" });

  const token = uuidv4();
  db.run("UPDATE users SET verification_token = ?, new_email = ? WHERE id = ?", [token, newEmail, userId]);

  try {
    resend.emails.send({
      from: "verify@slide.wiizardsoftware.uk",
      to: newEmail,
      subject: "Verify your new email for Slide",
      html: `<p>Please verify your new email by clicking here:</p><p><a href="https://slide.wiizardsoftware.uk/verify-email-change/${token}">Verify New Email</a></p>`
    });
    res.json({ success: true, message: "Verification email sent to new address!" });
  } catch (e) {
    res.status(500).json({ error: "Failed to send email" });
  }
});

router.post("/profile/verify-email-change/:token", (req, res) => {
  const { token } = req.params;
  db.get("SELECT * FROM users WHERE verification_token = ?", [token], (err, user: any) => {
    if (!user || !user.new_email) return res.status(400).json({ error: "Invalid token" });
    
    db.run("UPDATE users SET email = ?, new_email = NULL, verification_token = NULL WHERE id = ?", [user.new_email, user.id], (err) => {
      if (err) return res.status(500).json({ error: "Update failed" });
      res.json({ success: true });
    });
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

router.post("/report", (req, res) => {
  const userId = (req.session as any).userId;
  const username = (req.session as any).username || "Guest";
  const { complaint } = req.body;

  if (!complaint) return res.status(400).json({ error: "Complaint required" });

  console.log(`[REPORT] User: ${username} (ID: ${userId}) - Message: ${complaint}`);

  try {
    resend.emails.send({
      from: "report@slide.wiizardsoftware.uk",
      to: "maliburnett@icloud.com",
      subject: `Slide Report from ${username}`,
      html: `<p><strong>User:</strong> ${username}</p><p><strong>Message:</strong> ${complaint}</p>`
    });
    res.json({ success: true, message: "Report sent successfully!" });
  } catch (e) {
    console.error("Report email error:", e);
    res.status(500).json({ error: "Failed to send report" });
  }
});

export default router;
