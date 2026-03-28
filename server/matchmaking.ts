import { Server, Socket } from "socket.io";
import { db } from "./db";
import { INITIAL_BOARD, getValidMoves, checkWin, calculateEloChange, generateBoard } from "./gameLogic";
import { updateUnlockedSkins } from "./skins";

const socketToUser = new Map<string, number>();
const socketToGame = new Map<string, string>();
const userToGame = new Map<number, string>(); // userId -> gameId
const activeTimers = new Map<string, {timerW: number, timerB: number, turn: string, increment: number, lastMoveTime: number}>();
const publicQueue: {userId: number, elo: number, socketId: string, timeControl: string, variant: string}[] = [];
const privateGames = new Map<string, string>(); // code -> gameId
const spectators = new Map<string, Set<number>>(); // gameId -> Set of spectator userIds
const spectatorSessions = new Map<string, {spectatorId: number, targetUsername: string, gameId: string | null}>(); // socketId -> spectator session

export function setupMatchmaking(io: Server) {
  let onlineCount = 0;

  function broadcastOnlineCount() {
    io.emit("online_count", onlineCount);
  }

  function broadcastQueueCounts() {
    const variants = ['classic', 'fog_of_war', 'random_setup', 'schizophrenic'];
    const timeControls = ['0.25|3', '1|0', '3|2'];
    const counts: Record<string, Record<string, number>> = {};
    
    variants.forEach(v => {
      counts[v] = {};
      timeControls.forEach(tc => {
        counts[v][tc] = publicQueue.filter(q => q.timeControl === tc && q.variant === v).length;
      });
    });
    
    io.emit("queue_counts", counts);
  }

  setInterval(() => {
    const now = Date.now();
    for (const [gameId, data] of activeTimers.entries()) {
      const elapsed = Math.floor((now - data.lastMoveTime) / 1000);
      if (elapsed >= 1) {
        if (data.turn === 'W') {
          data.timerW -= elapsed;
        } else {
          data.timerB -= elapsed;
        }
        data.lastMoveTime = now;

        if (data.timerW <= 0 || data.timerB <= 0) {
          const winner = data.timerW <= 0 ? 'B' : 'W';
          activeTimers.delete(gameId);
          handleGameEnd(io, gameId, winner, 'timeout');
        } else {
          io.to(gameId).emit("timer_update", { timerW: data.timerW, timerB: data.timerB });
        }
      }
    }
  }, 1000);

  io.on("connection", (socket) => {
    onlineCount++;
    broadcastOnlineCount();
    broadcastQueueCounts();

    socket.on("disconnect", () => {
      onlineCount--;
      broadcastOnlineCount();
      const userId = socketToUser.get(socket.id);
      const gameId = socketToGame.get(socket.id);

      if (userId) {
        const idx = publicQueue.findIndex(q => q.userId === userId);
        if (idx !== -1) {
          publicQueue.splice(idx, 1);
          broadcastQueueCounts();
        }
      }

      if (userId && gameId) {
        handleGameEnd(io, gameId, null, 'forfeit', userId);
      }
      
      // Clean up spectator sessions
      const spectatorSession = spectatorSessions.get(socket.id);
      if (spectatorSession) {
        if (spectatorSession.gameId) {
          const spectatorSet = spectators.get(spectatorSession.gameId);
          if (spectatorSet) {
            spectatorSet.delete(spectatorSession.spectatorId);
            if (spectatorSet.size === 0) {
              spectators.delete(spectatorSession.gameId);
            }
          }
        }
        spectatorSessions.delete(socket.id);
      }
      
      socketToUser.delete(socket.id);
      socketToGame.delete(socket.id);
    });

    socket.on("join_queue", (data: {userId: number, elo: number, timeControl: string, variant: string}) => {
      db.get("SELECT is_banned FROM users WHERE id = ?", [data.userId], (err, user: any) => {
        if (err || !user || user.is_banned) {
          return socket.emit("error", "You are banned from playing public matches. Change your name to appeal.");
        }
        
        socketToUser.set(socket.id, data.userId);
        const matchIdx = publicQueue.findIndex(q => q.timeControl === data.timeControl && q.variant === data.variant && Math.abs(q.elo - data.elo) <= 200);
        
        if (matchIdx !== -1) {
          const opponent = publicQueue.splice(matchIdx, 1)[0];
          broadcastQueueCounts();
          startGame(io, data.userId, opponent.userId, opponent.socketId, socket.id, data.timeControl, data.variant || 'classic');
        } else {
          publicQueue.push({ userId: data.userId, elo: data.elo, socketId: socket.id, timeControl: data.timeControl, variant: data.variant || 'classic' });
          broadcastQueueCounts();
        }
      });
    });

    socket.on("leave_queue", (data: { userId: number }) => {
      const idx = publicQueue.findIndex(q => q.userId === data.userId);
      if (idx !== -1) {
        publicQueue.splice(idx, 1);
        broadcastQueueCounts();
      }
    });

    socket.on("create_private", (data: {userId: number, timeControl: string, variant: string}) => {
      db.get("SELECT skin FROM users WHERE id = ?", [data.userId], (err, user: any) => {
        socketToUser.set(socket.id, data.userId);
        const words = ["APPLE", "BREAD", "CHESS", "DREAM", "EAGLE", "FLAME", "GRAPE", "HOUSE", "IMAGE", "JOKER"];
        const code = words[Math.floor(Math.random() * words.length)] + Math.floor(100 + Math.random() * 900);
        const gameId = Math.random().toString(36).substring(7);
        const variant = data.variant || 'classic';
        const board = JSON.stringify(generateBoard(variant));
        const skinW = user?.skin || 'classic';

        let initialTimer = 600;
        let increment = 0;
        if (data.timeControl === '1|0') initialTimer = 60;
        else if (data.timeControl === '3|2') { initialTimer = 180; increment = 2; }
        else if (data.timeControl === '0.25|3') { initialTimer = 15; increment = 3; }

        db.run("INSERT INTO games (id, board, turn, player_w, status, is_private, code, timer_w, timer_b, increment, last_move_time, variant, skin_w) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
          [gameId, board, 'W', data.userId, 'waiting', true, code, initialTimer, initialTimer, increment, Date.now(), variant, skinW]);
        
        privateGames.set(code, gameId);
        socket.join(gameId);
        socket.emit("private_created", { code, gameId, timerW: initialTimer, timerB: initialTimer, variant });
      });
    });

    socket.on("join_private", (data: {userId: number, code: string}) => {
      const gameId = privateGames.get(data.code);
      if (!gameId) return socket.emit("error", "Invalid code");

      db.get("SELECT * FROM games WHERE id = ?", [gameId], (err, game: any) => {
        if (!game || game.status !== 'waiting') return socket.emit("error", "Game full or not found");
        
        db.get("SELECT skin, username FROM users WHERE id = ?", [data.userId], (err, userB: any) => {
          const skinB = userB?.skin || 'classic';
          
          db.run("UPDATE games SET player_b = ?, status = ?, last_move_time = ?, skin_b = ? WHERE id = ?", 
            [data.userId, 'active', Date.now(), skinB, gameId]);
          
          socketToUser.set(socket.id, data.userId);
          socketToGame.set(socket.id, gameId);
          // Track active games for spectating
          userToGame.set(data.userId, gameId);
          userToGame.set(game.player_w, gameId);
          
          socket.join(gameId);

          activeTimers.set(gameId, {
            timerW: game.timer_w,
            timerB: game.timer_b,
            turn: 'W',
            increment: game.increment,
            lastMoveTime: Date.now()
          });

          db.get("SELECT username, skin FROM users WHERE id = ?", [game.player_w], (err, userW: any) => {
            const skinW = userW?.skin || 'classic';

            // To joiner: set color to B
            socket.emit("match_found", { 
              gameId, color: 'B', opponentName: userW?.username, timerW: game.timer_w, timerB: game.timer_b, skinW, skinB, variant: game.variant, board: JSON.parse(game.board)
            });
            // To creator: set color to W (creator is already in room)
            socket.to(gameId).emit("join_private_success", { 
              gameId, 
              opponentName: userB?.username, 
              skinW, 
              skinB, 
              variant: game.variant, 
              board: JSON.parse(game.board),
              timerW: game.timer_w,
              timerB: game.timer_b
            });
          });
        });
      });
    });

    socket.on("join_game", (gameId: string) => {
      socketToGame.set(socket.id, gameId);
      socket.join(gameId);
    });

    socket.on("make_move", (data: {gameId: string, userId: number, from: {r: number, c: number}, to: {r: number, c: number}}) => {
      db.get("SELECT * FROM games WHERE id = ?", [data.gameId], (err, game: any) => {
        if (!game || game.status !== 'active') return;
        const board = JSON.parse(game.board);
        const turn = game.turn;
        const playerW = Number(game.player_w);
        const playerB = Number(game.player_b);

        if (turn === 'W' && data.userId !== playerW) return;
        if (turn === 'B' && data.userId !== playerB) return;

        const piece = board[data.from.r][data.from.c];
        if (piece !== turn) return;

        const validMoves = getValidMoves(board, data.from.r, data.from.c);
        if (!validMoves.some(m => m.r === data.to.r && m.c === data.to.c)) return;

        board[data.to.r][data.to.c] = piece;
        board[data.from.r][data.from.c] = '0';

        if (game.variant === 'schizophrenic') {
          const randR = Math.floor(Math.random() * 6);
          const randC = Math.floor(Math.random() * 6);
          board[randR][randC] = ['0', 'W', 'B'][Math.floor(Math.random() * 3)];
        }

        const winResult = checkWin(board);
        const winner = winResult ? winResult.winner : null;
        const nextTurn = turn === 'W' ? 'B' : 'W';
        const timerData = activeTimers.get(data.gameId);

        if (timerData) {
          const now = Date.now();
          const elapsed = Math.floor((now - timerData.lastMoveTime) / 1000);
          if (turn === 'W') timerData.timerW = Math.max(0, timerData.timerW - elapsed) + timerData.increment;
          else timerData.timerB = Math.max(0, timerData.timerB - elapsed) + timerData.increment;
          timerData.turn = nextTurn;
          timerData.lastMoveTime = now;

          db.run("UPDATE games SET board = ?, turn = ?, timer_w = ?, timer_b = ?, last_move_time = ? WHERE id = ?",
            [JSON.stringify(board), nextTurn, timerData.timerW, timerData.timerB, now, data.gameId]);

          if (winner) {
            handleGameEnd(io, data.gameId, winner, 'win', null, board, winResult.line, timerData);
          } else {
            io.to(data.gameId).emit("game_update", { 
              board, 
              turn: nextTurn, 
              status: 'active', 
              timerW: timerData.timerW, 
              timerB: timerData.timerB, 
              variant: game.variant,
              skinW: game.skin_w,
              skinB: game.skin_b
            });
          }
        }
      });
    });

    socket.on("forfeit", (data: { gameId: string, userId: number }) => {
      handleGameEnd(io, data.gameId, null, 'forfeit', data.userId);
    });

    socket.on("toggle_private_rated", (data: { gameId: string, isRated: boolean }) => {
      db.run("UPDATE games SET is_rated = ? WHERE id = ?", [data.isRated ? 1 : 0, data.gameId]);
      io.to(data.gameId).emit("private_rated_updated", data.isRated);
    });

    // Spectating Events
    socket.on("get_player_status", (data: { username: string }, callback: (status: { inGame: boolean, gameId?: string }) => void) => {
      db.get("SELECT id FROM users WHERE username = ?", [data.username], (err, user: any) => {
        if (err || !user) {
          return callback({ inGame: false });
        }
        const gameId = userToGame.get(user.id);
        callback({ inGame: !!gameId, gameId });
      });
    });

    socket.on("spectate", (data: { spectatorId: number, targetUsername: string }, callback: (response: { success: boolean, message?: string, gameData?: any }) => void) => {
      db.get("SELECT id FROM users WHERE username = ?", [data.targetUsername], (err, targetUser: any) => {
        if (!targetUser) {
          return callback({ success: false, message: "User not found" });
        }

        const targetGameId = userToGame.get(targetUser.id);
        if (!targetGameId) {
          return callback({ success: false, message: "User is not currently in a game" });
        }

        // Get game data
        db.get("SELECT * FROM games WHERE id = ?", [targetGameId], (err, game: any) => {
          if (!game || game.status !== 'active') {
            return callback({ success: false, message: "Game not found or is not active" });
          }

          // Record spectating in database for unlock tracking
          db.run("INSERT OR IGNORE INTO spectating (spectator_id, target_player_id) VALUES (?, ?)", 
            [data.spectatorId, targetUser.id], (err) => {
              if (!err) {
                // Update spectators_spectated_count (for the spectator, not the target)
                db.get("SELECT COUNT(*) as count FROM spectating WHERE spectator_id = ?", 
                  [data.spectatorId], (err, result: any) => {
                    if (result) {
                      db.run("UPDATE users SET spectators_count = ? WHERE id = ?", 
                        [result.count, data.spectatorId], () => {
                          // Check if unlock condition is met
                          updateUnlockedSkins(data.spectatorId);
                        });
                    }
                  });
              }
            });

          // Get player usernames
          db.all("SELECT id, username FROM users WHERE id IN (?, ?)", [game.player_w, game.player_b], (err, users: any[]) => {
            const userW = users?.find(u => u.id == game.player_w);
            const userB = users?.find(u => u.id == game.player_b);

            // Initialize spectators set if needed
            if (!spectators.has(targetGameId)) {
              spectators.set(targetGameId, new Set());
            }
            spectators.get(targetGameId)!.add(data.spectatorId);

            // Join the game room
            socket.join(targetGameId);

            // Track spectator session
            spectatorSessions.set(socket.id, {
              spectatorId: data.spectatorId,
              targetUsername: data.targetUsername,
              gameId: targetGameId
            });

            callback({
              success: true,
              gameData: {
                gameId: targetGameId,
                board: JSON.parse(game.board),
                turn: game.turn,
                skinW: game.skin_w,
                skinB: game.skin_b,
                variant: game.variant,
                timerW: activeTimers.get(targetGameId)?.timerW || game.timer_w,
                timerB: activeTimers.get(targetGameId)?.timerB || game.timer_b,
                playerWUsername: userW?.username || 'Player W',
                playerBUsername: userB?.username || 'Player B'
              }
            });
          });
        });
      });
    });

    socket.on("start_spectating_wait", (data: { spectatorId: number, targetUsername: string }) => {
      // Store the spectator session for when the target user starts a game
      spectatorSessions.set(socket.id, {
        spectatorId: data.spectatorId,
        targetUsername: data.targetUsername,
        gameId: null
      });
    });

    socket.on("stop_spectating", (data: { spectatorId: number }) => {
      const spectatorSession = spectatorSessions.get(socket.id);
      if (spectatorSession && spectatorSession.gameId) {
        const spectatorSet = spectators.get(spectatorSession.gameId);
        if (spectatorSet) {
          spectatorSet.delete(data.spectatorId);
          if (spectatorSet.size === 0) {
            spectators.delete(spectatorSession.gameId);
          }
        }
        socket.leave(spectatorSession.gameId);
      }
      spectatorSessions.delete(socket.id);
    });
  });
}

function startGame(io: Server, userIdW: number, userIdB: number, socketIdB: string, socketIdW: string, timeControl: string, variant: string) {
  const gameId = Math.random().toString(36).substring(7);
  const board = generateBoard(variant);
  let initialTimer = 600;
  let increment = 0;
  if (timeControl === '1|0') initialTimer = 60;
  else if (timeControl === '3|2') { initialTimer = 180; increment = 2; }
  else if (timeControl === '0.25|3') { initialTimer = 15; increment = 3; }

  db.all("SELECT id, username, skin FROM users WHERE id IN (?, ?)", [userIdW, userIdB], (err, users: any[]) => {
    const userW = users.find(u => u.id === userIdW);
    const userB = users.find(u => u.id === userIdB);
    const skinW = userW?.skin || 'classic';
    const skinB = userB?.skin || 'classic';

    db.run("INSERT INTO games (id, board, turn, player_w, player_b, status, timer_w, timer_b, increment, last_move_time, variant, skin_w, skin_b) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
      [gameId, JSON.stringify(board), 'W', userIdW, userIdB, 'active', initialTimer, initialTimer, increment, Date.now(), variant, skinW, skinB]);

    // Track active games for spectating
    userToGame.set(userIdW, gameId);
    userToGame.set(userIdB, gameId);

    activeTimers.set(gameId, { timerW: initialTimer, timerB: initialTimer, turn: 'W', increment, lastMoveTime: Date.now() });

    io.to(socketIdW).emit("match_found", { 
      gameId, color: 'W', opponentName: userB?.username, timerW: initialTimer, timerB: initialTimer, skinW, skinB, variant, board 
    });
    io.to(socketIdB).emit("match_found", { 
      gameId, color: 'B', opponentName: userW?.username, timerW: initialTimer, timerB: initialTimer, skinW, skinB, variant, board 
    });
  });
}

function handleGameEnd(io: Server, gameId: string, winner: string | null, reason: 'win' | 'timeout' | 'forfeit', forfeiterId?: number | null, finalBoard?: any, winningLine?: any, finalTimers?: any) {
  activeTimers.delete(gameId);
  db.get("SELECT * FROM games WHERE id = ?", [gameId], (err, game: any) => {
    if (!game) return;
    
    const isAlreadyFinished = game.status === 'finished';
    const playerW = Number(game.player_w);
    const playerB = Number(game.player_b);
    let finalWinner = winner;

    if (!isAlreadyFinished) {
      if (reason === 'forfeit' && forfeiterId) {
        finalWinner = forfeiterId === playerW ? 'B' : 'W';
      }
      db.run("UPDATE games SET status = 'finished', winner = ? WHERE id = ?", [finalWinner, gameId]);
    }

    // Clean up userToGame mapping
    userToGame.delete(playerW);
    userToGame.delete(playerB);

    const emitFinalUpdate = (eloChange: number = 0) => {
      io.to(gameId).emit("game_update", {
        status: 'finished',
        winner: finalWinner,
        eloChange,
        board: finalBoard || JSON.parse(game.board),
        winningLine,
        timerW: finalTimers?.timerW || game.timer_w,
        timerB: finalTimers?.timerB || game.timer_b,
        skinW: game.skin_w,
        skinB: game.skin_b,
        variant: game.variant
      });
    };

    if (isAlreadyFinished || !finalWinner || !playerB) {
      return emitFinalUpdate(0);
    }

    db.all("SELECT id, elo FROM users WHERE id IN (?, ?)", [playerW, playerB], (err, users: any[]) => {
      if (!users || users.length < 2) return emitFinalUpdate(0);
      
      const whitePlayer = users.find(u => u.id === playerW);
      const blackPlayer = users.find(u => u.id === playerB);
      if (!whitePlayer || !blackPlayer) return emitFinalUpdate(0);

      const winnerElo = finalWinner === 'W' ? whitePlayer.elo : blackPlayer.elo;
      const loserElo = finalWinner === 'W' ? blackPlayer.elo : whitePlayer.elo;
      const change = calculateEloChange(winnerElo, loserElo, 1);
      
      const winnerId = finalWinner === 'W' ? playerW : playerB;
      const loserId = finalWinner === 'W' ? playerB : playerW;

      const isRandomSetup = game.variant === 'random_setup';
      const isFogOfWar = game.variant === 'fog_of_war';
      const isOneMin = game.timer_w === 60 || (game.timer_w === 15 && game.increment === 3);

      const winnerUpdates = ["elo = elo + ?", "wins = wins + 1", "games_played = games_played + 1"];
      const loserUpdates = ["elo = elo - ?", "games_played = games_played + 1"];
      const winnerParams: any[] = [change];
      const loserParams: any[] = [change];

      if (isRandomSetup) {
        winnerUpdates.push("games_random_setup = games_random_setup + 1");
        loserUpdates.push("games_random_setup = games_random_setup + 1");
      }
      if (isFogOfWar) {
        winnerUpdates.push("games_fog_of_war = games_fog_of_war + 1");
        loserUpdates.push("games_fog_of_war = games_fog_of_war + 1");
      }
      if (isOneMin) {
        winnerUpdates.push("games_1min = games_1min + 1");
        loserUpdates.push("games_1min = games_1min + 1");
      }

      winnerParams.push(winnerId);
      loserParams.push(loserId);

      if (game.is_rated) {
        db.run(`UPDATE users SET ${winnerUpdates.join(", ")} WHERE id = ?`, winnerParams, () => updateUnlockedSkins(winnerId));
        db.run(`UPDATE users SET ${loserUpdates.join(", ")} WHERE id = ?`, loserParams, () => updateUnlockedSkins(loserId));
      } else {
        const winUpdatesUnrated = winnerUpdates.filter(u => !u.includes("elo"));
        const loseUpdatesUnrated = loserUpdates.filter(u => !u.includes("elo"));
        db.run(`UPDATE users SET ${winUpdatesUnrated.join(", ")} WHERE id = ?`, winnerParams.slice(1), () => updateUnlockedSkins(winnerId));
        db.run(`UPDATE users SET ${loseUpdatesUnrated.join(", ")} WHERE id = ?`, loserParams.slice(1), () => updateUnlockedSkins(loserId));
      }

      emitFinalUpdate(game.is_rated ? change : 0);
    });
  });
}
