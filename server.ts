import express from "express";
import { createServer } from "http";
import { Server } from "socket.io";
import { createServer as createViteServer } from "vite";
import path from "path";
import sqlite3pkg from "sqlite3";
const sqlite3 = sqlite3pkg.verbose();
import bcrypt from "bcryptjs";
import session from "express-session";
import cookieParser from "cookie-parser";
import connectSqlite3 from "connect-sqlite3";

const SQLiteStore = connectSqlite3(session);
const app = express();
const httpServer = createServer(app);
const io = new Server(httpServer);
const PORT = process.env.PORT || 3000;

// Database setup
const db = new sqlite3.Database("game.db", (err) => {
  if (err) {
    console.error("Database connection error:", err);
  } else {
    console.log("Connected to SQLite database.");
  }
});

db.serialize(() => {
  db.run(`CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT UNIQUE,
    password TEXT,
    elo INTEGER DEFAULT 600,
    is_guest BOOLEAN DEFAULT 0
  )`);

  db.run(`CREATE TABLE IF NOT EXISTS games (
    id TEXT PRIMARY KEY,
    board TEXT,
    turn TEXT,
    player_w TEXT,
    player_b TEXT,
    status TEXT,
    winner TEXT,
    is_private BOOLEAN,
    code TEXT
  )`);
});

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
  cookie: { secure: false, sameSite: 'lax' } // Set to true if using https
}));

// Game Logic
const INITIAL_BOARD = [
  ['B', '0', 'W', 'B', '0', 'W'],
  ['0', '0', '0', '0', '0', '0'],
  ['W', '0', '0', '0', '0', 'B'],
  ['B', '0', '0', '0', '0', 'W'],
  ['0', '0', '0', '0', '0', '0'],
  ['W', '0', 'B', 'W', '0', 'B']
];

function checkWin(board: string[][]) {
  const size = 6;
  // Horizontal
  for (let r = 0; r < size; r++) {
    for (let c = 0; c <= size - 4; c++) {
      const p = board[r][c];
      if (p !== '0' && p === board[r][c+1] && p === board[r][c+2] && p === board[r][c+3]) {
        return { winner: p, line: [{r: r, c: c}, {r: r, c: c+1}, {r: r, c: c+2}, {r: r, c: c+3}] };
      }
    }
  }
  // Vertical
  for (let c = 0; c < size; c++) {
    for (let r = 0; r <= size - 4; r++) {
      const p = board[r][c];
      if (p !== '0' && p === board[r+1][c] && p === board[r+2][c] && p === board[r+3][c]) {
        return { winner: p, line: [{r: r, c: c}, {r: r+1, c: c}, {r: r+2, c: c}, {r: r+3, c: c}] };
      }
    }
  }
  // Diagonal Down-Right
  for (let r = 0; r <= size - 4; r++) {
    for (let c = 0; c <= size - 4; c++) {
      const p = board[r][c];
      if (p !== '0' && p === board[r+1][c+1] && p === board[r+2][c+2] && p === board[r+3][c+3]) {
        return { winner: p, line: [{r: r, c: c}, {r: r+1, c: c+1}, {r: r+2, c: c+2}, {r: r+3, c: c+3}] };
      }
    }
  }
  // Diagonal Down-Left
  for (let r = 0; r <= size - 4; r++) {
    for (let c = 3; c < size; c++) {
      const p = board[r][c];
      if (p !== '0' && p === board[r+1][c-1] && p === board[r+2][c-2] && p === board[r+3][c-3]) {
        return { winner: p, line: [{r: r, c: c}, {r: r+1, c: c-1}, {r: r+2, c: c-2}, {r: r+3, c: c-3}] };
      }
    }
  }
  return null;
}

function getValidMoves(board: string[][], r: number, c: number) {
  const piece = board[r][c];
  if (piece === '0') return [];
  const moves: {r: number, c: number}[] = [];
  const dirs = [[0, 1], [0, -1], [1, 0], [-1, 0]];
  for (const [dr, dc] of dirs) {
    let currR = r + dr;
    let currC = c + dc;
    let lastValidR = r;
    let lastValidC = c;
    while (currR >= 0 && currR < 6 && currC >= 0 && currC < 6 && board[currR][currC] === '0') {
      lastValidR = currR;
      lastValidC = currC;
      currR += dr;
      currC += dc;
    }
    if (lastValidR !== r || lastValidC !== c) {
      moves.push({r: lastValidR, c: lastValidC});
    }
  }
  return moves;
}

function calculateEloChange(playerElo: number, opponentElo: number, result: number) {
  const K = 16;
  const expectedScore = 1 / (1 + Math.pow(10, (opponentElo - playerElo) / 400));
  return Math.round(K * (result - expectedScore));
}

// Auth Routes
app.get("/api/leaderboard", (req, res) => {
  db.all("SELECT id, username, elo FROM users WHERE is_guest = 0 ORDER BY elo DESC LIMIT 10", (err, rows) => {
    if (err) return res.status(500).json({ error: "Error fetching leaderboard" });
    res.json(rows);
  });
});
app.post("/api/register", (req, res) => {
  const { username, password } = req.body;
  if (!username || !password) return res.status(400).json({ error: "Missing fields" });
  const hash = bcrypt.hashSync(password, 10);
  db.run("INSERT INTO users (username, password, elo) VALUES (?, ?, ?)", [username, hash, 600], function(err) {
    if (err) return res.status(400).json({ error: "Username taken" });
    res.json({ success: true });
  });
});

app.post("/api/login", (req, res) => {
  const { username, password } = req.body;
  db.get("SELECT * FROM users WHERE username = ?", [username], (err, user: any) => {
    if (err || !user || !bcrypt.compareSync(password, user.password)) {
      return res.status(400).json({ error: "Invalid credentials" });
    }
    (req.session as any).userId = user.id;
    (req.session as any).username = user.username;
    res.json({ id: user.id, username: user.username, elo: user.elo });
  });
});

app.post("/api/guest", (req, res) => {
  const guestName = "Guest_" + Math.random().toString(36).substring(7);
  db.run("INSERT INTO users (username, elo, is_guest) VALUES (?, ?, ?)", [guestName, 400, 1], function(err) {
    if (err) return res.status(500).json({ error: "Error creating guest" });
    const guestId = this.lastID;
    (req.session as any).userId = guestId;
    (req.session as any).username = guestName;
    res.json({ id: guestId, username: guestName, elo: 400 });
  });
});

app.get("/api/me", (req, res) => {
  const userId = (req.session as any).userId;
  if (!userId) return res.status(401).json({ error: "Not logged in" });
  db.get("SELECT id, username, elo FROM users WHERE id = ?", [userId], (err, user) => {
    if (err || !user) return res.status(401).json({ error: "User not found" });
    res.json(user);
  });
});

// Matchmaking
const publicQueue: {userId: number, elo: number, socketId: string}[] = [];
const privateGames = new Map<string, string>(); // code -> gameId

io.on("connection", (socket) => {
  console.log("User connected:", socket.id);

  socket.on("join_queue", async (data: {userId: number, elo: number}) => {
    // Check if already in queue
    const idx = publicQueue.findIndex(q => q.userId === data.userId);
    if (idx !== -1) publicQueue.splice(idx, 1);

    // Find match
    const matchIdx = publicQueue.findIndex(q => Math.abs(q.elo - data.elo) <= 200);
    if (matchIdx !== -1) {
      const opponent = publicQueue.splice(matchIdx, 1)[0];
      const gameId = Math.random().toString(36).substring(7);
      const board = JSON.stringify(INITIAL_BOARD);
      db.run("INSERT INTO games (id, board, turn, player_w, player_b, status) VALUES (?, ?, ?, ?, ?, ?)",
        [gameId, board, 'W', data.userId, opponent.userId, 'active']);
      
      db.all("SELECT id, username FROM users WHERE id IN (?, ?)", [data.userId, opponent.userId], (err, users: any[]) => {
        const userW = users.find(u => u.id === data.userId);
        const userB = users.find(u => u.id === opponent.userId);
        io.to(socket.id).emit("match_found", { gameId, color: 'W', opponentName: userB?.username });
        io.to(opponent.socketId).emit("match_found", { gameId, color: 'B', opponentName: userW?.username });
      });
    } else {
      publicQueue.push({ userId: data.userId, elo: data.elo, socketId: socket.id });
    }
  });

  socket.on("create_private", async (data: {userId: number}) => {
    const words = ["APPLE", "BREAD", "CHESS", "DREAM", "EAGLE", "FLAME", "GRAPE", "HOUSE", "IMAGE", "JOKER"];
    const code = words[Math.floor(Math.random() * words.length)] + Math.floor(100 + Math.random() * 900);
    const gameId = Math.random().toString(36).substring(7);
    const board = JSON.stringify(INITIAL_BOARD);
    db.run("INSERT INTO games (id, board, turn, player_w, status, is_private, code) VALUES (?, ?, ?, ?, ?, ?, ?)",
      [gameId, board, 'W', data.userId, 'waiting', true, code]);
    
    privateGames.set(code, gameId);
    socket.join(gameId);
    socket.emit("private_created", { code, gameId });
  });

  socket.on("join_private", async (data: {userId: number, code: string}) => {
    const gameId = privateGames.get(data.code);
    if (!gameId) return socket.emit("error", "Invalid code");

    db.get("SELECT * FROM games WHERE id = ?", [gameId], (err, game: any) => {
      if (!game || game.status !== 'waiting') return socket.emit("error", "Game full or not found");
      db.run("UPDATE games SET player_b = ?, status = ? WHERE id = ?", [data.userId, 'active', gameId]);
      socket.join(gameId);
      
      db.all("SELECT id, username FROM users WHERE id IN (?, ?)", [game.player_w, data.userId], (err, users: any[]) => {
        const userW = users.find(u => u.id === Number(game.player_w));
        const userB = users.find(u => u.id === data.userId);
        
        // Send to the joiner that they are Black
        socket.emit("match_found", { gameId, color: 'B', opponentName: userW?.username });
        // Send to the creator (already in the room) that they are White
        socket.to(gameId).emit("match_found", { gameId, color: 'W', opponentName: userB?.username });
      });
    });
  });

  socket.on("join_game", (gameId: string) => {
    socket.join(gameId);
  });

  socket.on("make_move", async (data: {gameId: string, userId: number, from: {r: number, c: number}, to: {r: number, c: number}}) => {
    db.get("SELECT * FROM games WHERE id = ?", [data.gameId], (err, game: any) => {
      if (!game || game.status !== 'active') return;
      const board = JSON.parse(game.board);
      const turn = game.turn;
      const playerW = Number(game.player_w);
      const playerB = Number(game.player_b);

      // Validate turn
      if (turn === 'W' && data.userId !== playerW) return;
      if (turn === 'B' && data.userId !== playerB) return;

      // Validate piece
      const piece = board[data.from.r][data.from.c];
      if (piece !== turn) return;

      // Validate move
      const validMoves = getValidMoves(board, data.from.r, data.from.c);
      const isValid = validMoves.some(m => m.r === data.to.r && m.c === data.to.c);
      if (!isValid) return;

      // Execute move
      board[data.to.r][data.to.c] = piece;
      board[data.from.r][data.from.c] = '0';

      // Check win
      const winResult = checkWin(board);
      const winner = winResult ? winResult.winner : null;
      const winningLine = winResult ? winResult.line : null;
      const nextTurn = turn === 'W' ? 'B' : 'W';
      const status = winner ? 'finished' : 'active';

      db.run("UPDATE games SET board = ?, turn = ?, status = ?, winner = ? WHERE id = ?",
        [JSON.stringify(board), nextTurn, status, winner, data.gameId]);

      if (winner) {
        // Fetch players' current ELO
        db.all("SELECT id, elo FROM users WHERE id IN (?, ?)", [playerW, playerB], (err, rows: any[]) => {
          if (err || !rows || rows.length < 2) return;
          
          const whitePlayer = rows.find(r => r.id === playerW);
          const blackPlayer = rows.find(r => r.id === playerB);
          
          if (!whitePlayer || !blackPlayer) return;

          const winnerElo = winner === 'W' ? whitePlayer.elo : blackPlayer.elo;
          const loserElo = winner === 'W' ? blackPlayer.elo : whitePlayer.elo;
          
          const change = calculateEloChange(winnerElo, loserElo, 1);
          
          // Apply changes
          const newWinnerElo = winnerElo + change;
          const newLoserElo = loserElo - change;

          db.run("UPDATE users SET elo = ? WHERE id = ?", [newWinnerElo, winner === 'W' ? playerW : playerB]);
          db.run("UPDATE users SET elo = ? WHERE id = ?", [newLoserElo, winner === 'W' ? playerB : playerW]);
          
          io.to(data.gameId).emit("game_update", { 
            board, 
            turn: nextTurn, 
            status, 
            winner, 
            winningLine,
            eloChange: change 
          });
        });
      } else {
        io.to(data.gameId).emit("game_update", { board, turn: nextTurn, status, winner, winningLine });
      }
    });
  });
});

async function startServer() {
  console.log("Starting server...");
  if (process.env.NODE_ENV !== "production") {
    console.log("Initializing Vite in middleware mode...");
    try {
      const vite = await createViteServer({
        server: { middlewareMode: true },
        appType: "spa",
      });
      app.use(vite.middlewares);
      console.log("Vite middleware attached.");
    } catch (e) {
      console.error("Vite server creation failed:", e);
    }
  } else {
    console.log("Running in production mode.");
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
