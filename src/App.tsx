import React, { useState, useEffect, useCallback } from 'react';
import { motion, AnimatePresence } from 'motion/react';
import { io, Socket } from 'socket.io-client';
import { User, LogIn, UserPlus, Play, Trophy, LogOut, Hash, Lock, Users, EyeOff } from 'lucide-react';

// Types
type Piece = 'W' | 'B' | '0';
type Board = Piece[][];
type Turn = 'W' | 'B';
type Status = 'active' | 'waiting' | 'finished';

interface GameState {
  board: Board;
  turn: Turn;
  status: Status;
  winner: Piece | null;
  winningLine?: {r: number, c: number}[] | null;
  eloChange?: number;
}

interface UserData {
  id: number;
  username: string;
  elo: number;
}

interface LeaderboardEntry {
  id: number;
  username: string;
  elo: number;
}

const INITIAL_BOARD: Board = [
  ['B', '0', 'W', 'B', '0', 'W'],
  ['0', '0', '0', '0', '0', '0'],
  ['W', '0', '0', '0', '0', 'B'],
  ['B', '0', '0', '0', '0', 'W'],
  ['0', '0', '0', '0', '0', '0'],
  ['W', '0', 'B', 'W', '0', 'B']
];

const socket: Socket = io();

export default function App() {
  console.log("App rendering...");
  const [user, setUser] = useState<UserData | null>(null);
  const [view, setView] = useState<'auth' | 'lobby' | 'game'>('auth');
  const [authMode, setAuthMode] = useState<'login' | 'register'>('login');
  const [username, setUsername] = useState('');
  const [password, setPassword] = useState('');
  const [error, setError] = useState('');
  
  const [gameId, setGameId] = useState<string | null>(null);
  const [gameState, setGameState] = useState<GameState | null>(null);
  const [playerColor, setPlayerColor] = useState<Turn | null>(null);
  const [opponentName, setOpponentName] = useState<string | null>(null);
  const [selected, setSelected] = useState<{r: number, c: number} | null>(null);
  const [validMoves, setValidMoves] = useState<{r: number, c: number}[]>([]);
  const [privateCode, setPrivateCode] = useState<string | null>(null);
  const [joinCode, setJoinCode] = useState('');
  const [isLocal, setIsLocal] = useState(false);
  const [showWinScreen, setShowWinScreen] = useState(false);
  const [isWinScreenHidden, setIsWinScreenHidden] = useState(false);
  const [showLeaderboard, setShowLeaderboard] = useState(false);
  const [leaderboard, setLeaderboard] = useState<LeaderboardEntry[]>([]);
  const [showTutorial, setShowTutorial] = useState(false);

  // Auth Functions
  const handleAuth = async (e: React.FormEvent) => {
    e.preventDefault();
    const endpoint = authMode === 'login' ? '/api/login' : '/api/register';
    const res = await fetch(endpoint, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ username, password })
    });
    const data = await res.json();
    if (res.ok) {
      if (authMode === 'login') {
        setUser(data);
        setView('lobby');
      } else {
        setAuthMode('login');
        setError('Registration successful! Please login.');
      }
    } else {
      setError(data.error);
    }
  };

  const handleGuest = async () => {
    const res = await fetch('/api/guest', { method: 'POST' });
    const data = await res.json();
    if (res.ok) {
      setUser(data);
      setView('lobby');
    }
  };

  const checkMe = useCallback(async () => {
    const res = await fetch('/api/me');
    if (res.ok) {
      const data = await res.json();
      setUser(data);
      setView('lobby');
    }
  }, []);

  useEffect(() => {
    checkMe();
  }, [checkMe]);

  const fetchLeaderboard = async () => {
    const res = await fetch('/api/leaderboard');
    if (res.ok) {
      const data = await res.json();
      setLeaderboard(data);
    }
  };

  // Game Functions
  useEffect(() => {
    socket.on('match_found', (data: { gameId: string, color: Turn, opponentName?: string }) => {
      setGameId(data.gameId);
      setPlayerColor(data.color);
      setOpponentName(data.opponentName || 'Opponent');
      socket.emit('join_game', data.gameId);
      setGameState({ board: INITIAL_BOARD, turn: 'W', status: 'active', winner: null });
      setView('game');
      setPrivateCode(null);
    });

    socket.on('private_created', (data: { code: string, gameId: string }) => {
      setPrivateCode(data.code);
      setGameId(data.gameId);
      setPlayerColor('W');
      socket.emit('join_game', data.gameId);
    });

    socket.on('game_update', (data: GameState) => {
      setGameState(data);
      if (data.status === 'finished' && data.eloChange !== undefined) {
        // We'll update ELO in a separate useEffect to ensure we have the latest user state
      }
    });

    socket.on('error', (msg: string) => {
      setError(msg);
    });

    return () => {
      socket.off('match_found');
      socket.off('private_created');
      socket.off('game_update');
      socket.off('error');
    };
  }, []);

  useEffect(() => {
    if (gameState?.status === 'finished' && gameState.eloChange !== undefined && user && !isLocal) {
      const isWinner = gameState.winner === playerColor;
      const change = gameState.eloChange;
      setUser(prev => prev ? { ...prev, elo: isWinner ? prev.elo + change : prev.elo - change } : null);
    }
  }, [gameState?.status, gameState?.winner, gameState?.eloChange, playerColor, isLocal]);

  useEffect(() => {
    if (gameState?.winner) {
      const timer = setTimeout(() => {
        setShowWinScreen(true);
      }, 800); // 800ms delay for the animation to finish
      return () => clearTimeout(timer);
    } else {
      setShowWinScreen(false);
      setIsWinScreenHidden(false);
    }
  }, [gameState?.winner]);

  const getValidMoves = (board: Board, r: number, c: number) => {
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
  };

  const checkWin = (board: Board) => {
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
  };

  const handleSquareClick = (r: number, c: number) => {
    if (!gameState || gameState.status !== 'active') return;
    
    // In local mode, any turn is valid for the person at the machine
    if (!isLocal && gameState.turn !== playerColor) return;

    const currentTurn = gameState.turn;

    if (selected) {
      const isMove = validMoves.some(m => m.r === r && m.c === c);
      if (isMove) {
        if (isLocal) {
          const newBoard = gameState.board.map(row => [...row]);
          const piece = newBoard[selected.r][selected.c];
          newBoard[r][c] = piece;
          newBoard[selected.r][selected.c] = '0';
          
          const winResult = checkWin(newBoard);
          const winner = winResult ? winResult.winner : null;
          const winningLine = winResult ? winResult.line : null;
          
          setGameState({
            board: newBoard,
            turn: currentTurn === 'W' ? 'B' : 'W',
            status: winner ? 'finished' : 'active',
            winner: winner,
            winningLine: winningLine
          });
        } else {
          socket.emit('make_move', { gameId, userId: user?.id, from: selected, to: {r, c} });
        }
        setSelected(null);
        setValidMoves([]);
      } else {
        const piece = gameState.board[r][c];
        if (piece === currentTurn) {
          setSelected({r, c});
          setValidMoves(getValidMoves(gameState.board, r, c));
        } else {
          setSelected(null);
          setValidMoves([]);
        }
      }
    } else {
      const piece = gameState.board[r][c];
      if (piece === currentTurn) {
        setSelected({r, c});
        setValidMoves(getValidMoves(gameState.board, r, c));
      }
    }
  };

  const startPublicMatch = () => {
    if (!user) return;
    setGameId(null);
    setGameState(null);
    setOpponentName(null);
    socket.emit('join_queue', { userId: user.id, elo: user.elo });
    setError('Searching for opponent...');
  };

  const createPrivateMatch = () => {
    if (!user) return;
    socket.emit('create_private', { userId: user.id });
  };

  const joinPrivateMatch = () => {
    if (!user || !joinCode) return;
    socket.emit('join_private', { userId: user.id, code: joinCode.toUpperCase() });
  };

  const startLocalMatch = () => {
    setIsLocal(true);
    setGameId('local');
    setPlayerColor('W'); // Not strictly used for turn validation in local, but good for UI
    setGameState({ board: INITIAL_BOARD, turn: 'W', status: 'active', winner: null });
    setView('game');
  };

  // UI Components
  if (view === 'auth') {
    return (
      <div className="min-h-screen bg-[#f4f1ea] flex items-center justify-center p-4 font-sans text-[#4a3728]">
        <motion.div 
          initial={{ opacity: 0, y: 20 }}
          animate={{ opacity: 1, y: 0 }}
          className="bg-white p-8 rounded-2xl shadow-xl w-full max-w-md border-b-4 border-[#d2b48c]"
        >
          <div className="flex justify-center mb-6">
            <div className="bg-[#8b4513] p-4 rounded-full">
              <Trophy className="w-12 h-12 text-white" />
            </div>
          </div>
          <h1 className="text-3xl font-bold text-center mb-2">SLIDE</h1>
          <p className="text-center text-gray-500 mb-8 italic">A minimalist strategy game</p>
          
          <form onSubmit={handleAuth} className="space-y-4">
            <div className="relative">
              <User className="absolute left-3 top-3 w-5 h-5 text-gray-400" />
              <input 
                type="text" 
                placeholder="Username" 
                className="w-full pl-10 pr-4 py-3 bg-gray-50 border border-gray-200 rounded-xl focus:ring-2 focus:ring-[#8b4513] outline-none transition-all"
                value={username}
                onChange={(e) => setUsername(e.target.value)}
              />
            </div>
            <div className="relative">
              <Lock className="absolute left-3 top-3 w-5 h-5 text-gray-400" />
              <input 
                type="password" 
                placeholder="Password" 
                className="w-full pl-10 pr-4 py-3 bg-gray-50 border border-gray-200 rounded-xl focus:ring-2 focus:ring-[#8b4513] outline-none transition-all"
                value={password}
                onChange={(e) => setPassword(e.target.value)}
              />
            </div>
            
            <button type="submit" className="w-full py-3 bg-[#8b4513] text-white rounded-xl font-bold hover:bg-[#6d3610] transition-colors flex items-center justify-center gap-2">
              {authMode === 'login' ? <LogIn className="w-5 h-5" /> : <UserPlus className="w-5 h-5" />}
              {authMode === 'login' ? 'Login' : 'Register'}
            </button>
          </form>

          <div className="mt-6 flex flex-col gap-3">
            <button 
              onClick={() => setAuthMode(authMode === 'login' ? 'register' : 'login')}
              className="text-sm text-[#8b4513] hover:underline text-center"
            >
              {authMode === 'login' ? "Don't have an account? Register" : "Already have an account? Login"}
            </button>
            <div className="flex items-center gap-4 my-2">
              <div className="flex-1 h-px bg-gray-200"></div>
              <span className="text-xs text-gray-400 uppercase font-bold">OR</span>
              <div className="flex-1 h-px bg-gray-200"></div>
            </div>
            <button 
              onClick={handleGuest}
              className="w-full py-3 bg-white border-2 border-[#8b4513] text-[#8b4513] rounded-xl font-bold hover:bg-gray-50 transition-colors flex items-center justify-center gap-2"
            >
              <Users className="w-5 h-5" />
              Play as Guest
            </button>
          </div>
          
          {error && <p className="mt-4 text-red-500 text-center text-sm font-medium">{error}</p>}
        </motion.div>
      </div>
    );
  }

  if (view === 'lobby') {
    return (
      <div className="min-h-screen bg-[#f4f1ea] p-4 sm:p-8 font-sans text-[#4a3728]">
        <div className="max-w-4xl mx-auto">
          <header className="flex flex-col sm:flex-row justify-between items-center gap-6 mb-8 sm:mb-12">
            <div className="text-center sm:text-left">
              <h1 className="text-4xl font-bold tracking-tight">SLIDE</h1>
              <div className="flex items-center justify-center sm:justify-start gap-2 mt-2">
                <div className="w-2 h-2 bg-green-500 rounded-full animate-pulse"></div>
                <span className="text-sm text-gray-500 font-medium">Online</span>
              </div>
            </div>
            <div className="flex items-center gap-4 sm:gap-6">
              <button 
                onClick={() => { fetchLeaderboard(); setShowLeaderboard(true); }}
                className="p-2 sm:p-3 bg-white rounded-full shadow-md hover:shadow-lg transition-all text-[#8b4513]"
                title="Leaderboard"
              >
                <Trophy className="w-5 h-5 sm:w-6 h-6" />
              </button>
              <button 
                onClick={() => setShowTutorial(true)}
                className="p-2 sm:p-3 bg-white rounded-full shadow-md hover:shadow-lg transition-all text-[#8b4513]"
                title="Tutorial"
              >
                <Hash className="w-5 h-5 sm:w-6 h-6" />
              </button>
              <div className="text-right">
                <p className="font-bold text-lg sm:text-xl">{user?.username}</p>
                <p className="text-xs sm:text-sm text-[#8b4513] font-bold">ELO: {user?.elo}</p>
              </div>
              <button 
                onClick={() => { setUser(null); setView('auth'); }}
                className="p-2 sm:p-3 bg-white rounded-full shadow-md hover:shadow-lg transition-all text-gray-400 hover:text-red-500"
              >
                <LogOut className="w-5 h-5 sm:w-6 h-6" />
              </button>
            </div>
          </header>

          <div className="grid grid-cols-1 md:grid-cols-2 gap-6 sm:gap-8">
            <motion.div 
              whileHover={{ scale: 1.02 }}
              className="bg-white p-6 sm:p-8 rounded-3xl shadow-xl border-b-8 border-[#d2b48c]"
            >
              <div className="bg-[#8b4513] w-14 h-14 sm:w-16 sm:h-16 rounded-2xl flex items-center justify-center mb-4 sm:mb-6">
                <Play className="w-7 h-7 sm:w-8 h-8 text-white fill-current" />
              </div>
              <h2 className="text-xl sm:text-2xl font-bold mb-3 sm:mb-4">Public Match</h2>
              <p className="text-sm sm:text-base text-gray-500 mb-6 sm:mb-8 leading-relaxed">Match against players within ±200 ELO range. Prove your strategic dominance.</p>
              <button 
                onClick={startPublicMatch}
                className="w-full py-3 sm:py-4 bg-[#8b4513] text-white rounded-2xl font-bold text-base sm:text-lg hover:bg-[#6d3610] transition-all shadow-lg shadow-[#8b4513]/20"
              >
                Find Match
              </button>
            </motion.div>

            <motion.div 
              whileHover={{ scale: 1.02 }}
              className="bg-white p-6 sm:p-8 rounded-3xl shadow-xl border-b-8 border-[#d2b48c]"
            >
              <div className="bg-[#d2b48c] w-14 h-14 sm:w-16 sm:h-16 rounded-2xl flex items-center justify-center mb-4 sm:mb-6">
                <Users className="w-7 h-7 sm:w-8 h-8 text-white" />
              </div>
              <h2 className="text-xl sm:text-2xl font-bold mb-3 sm:mb-4">Local Play</h2>
              <p className="text-sm sm:text-base text-gray-500 mb-6 sm:mb-8 leading-relaxed">Play with a friend on the same machine. Perfect for testing strategies.</p>
              <button 
                onClick={startLocalMatch}
                className="w-full py-3 sm:py-4 bg-[#d2b48c] text-white rounded-2xl font-bold text-base sm:text-lg hover:bg-[#b89b74] transition-all shadow-lg shadow-[#d2b48c]/20"
              >
                Start Local Game
              </button>
            </motion.div>

            <motion.div 
              whileHover={{ scale: 1.02 }}
              className="bg-white p-6 sm:p-8 rounded-3xl shadow-xl border-b-8 border-[#d2b48c] md:col-span-2"
            >
              <div className="bg-gray-200 w-14 h-14 sm:w-16 sm:h-16 rounded-2xl flex items-center justify-center mb-4 sm:mb-6">
                <Hash className="w-7 h-7 sm:w-8 h-8 text-gray-600" />
              </div>
              <h2 className="text-xl sm:text-2xl font-bold mb-3 sm:mb-4">Private Match</h2>
              <p className="text-sm sm:text-base text-gray-500 mb-6 sm:mb-8 leading-relaxed">Create a room or join a friend using a secret code.</p>
              
              <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
                <button 
                  onClick={createPrivateMatch}
                  className="w-full py-3 sm:py-4 bg-white border-2 border-[#8b4513] text-[#8b4513] rounded-2xl font-bold text-base sm:text-lg hover:bg-gray-50 transition-all"
                >
                  Create Room
                </button>
                <div className="flex flex-col sm:flex-row gap-2">
                  <input 
                    type="text" 
                    placeholder="Enter Code" 
                    className="flex-1 px-4 py-3 sm:py-4 bg-gray-50 border border-gray-200 rounded-2xl outline-none focus:ring-2 focus:ring-[#8b4513] font-mono text-center uppercase text-base sm:text-lg"
                    value={joinCode}
                    onChange={(e) => setJoinCode(e.target.value)}
                  />
                  <button 
                    onClick={joinPrivateMatch}
                    className="w-full sm:w-auto px-8 py-3 sm:py-4 bg-[#8b4513] text-white rounded-2xl font-bold hover:bg-[#6d3610] transition-all"
                  >
                    Join
                  </button>
                </div>
              </div>
            </motion.div>
          </div>

          {error && (
            <motion.div 
              initial={{ opacity: 0, y: 10 }}
              animate={{ opacity: 1, y: 0 }}
              className="mt-8 p-4 bg-[#8b4513]/10 border border-[#8b4513]/20 rounded-2xl text-center text-[#8b4513] font-medium"
            >
              {error}
            </motion.div>
          )}

          {privateCode && (
            <div className="fixed inset-0 bg-black/50 backdrop-blur-sm flex items-center justify-center p-4 z-50">
              <motion.div 
                initial={{ scale: 0.9, opacity: 0 }}
                animate={{ scale: 1, opacity: 1 }}
                className="bg-white p-8 rounded-3xl shadow-2xl max-w-sm w-full text-center relative"
              >
                <button 
                  onClick={() => { setPrivateCode(null); setGameId(null); setPlayerColor(null); }}
                  className="absolute top-4 right-4 p-2 text-gray-400 hover:text-red-500 transition-colors"
                >
                  <LogOut className="w-5 h-5" />
                </button>
                <h3 className="text-2xl font-bold mb-2">Room Created</h3>
                <p className="text-gray-500 mb-6">Share this code with your friend</p>
                <div className="bg-gray-100 p-6 rounded-2xl text-4xl font-mono font-bold tracking-widest text-[#8b4513] mb-8">
                  {privateCode}
                </div>
                <p className="text-sm text-gray-400 animate-pulse mb-4">Waiting for opponent to join...</p>
                <button 
                  onClick={() => { setPrivateCode(null); setGameId(null); setPlayerColor(null); }}
                  className="text-sm font-bold text-[#8b4513] hover:underline"
                >
                  Cancel and Go Back
                </button>
              </motion.div>
            </div>
          )}

          {showLeaderboard && (
            <div className="fixed inset-0 bg-black/50 backdrop-blur-sm flex items-center justify-center p-4 z-50">
              <motion.div 
                initial={{ scale: 0.9, opacity: 0 }}
                animate={{ scale: 1, opacity: 1 }}
                className="bg-white p-8 rounded-3xl shadow-2xl max-w-md w-full relative"
              >
                <button 
                  onClick={() => setShowLeaderboard(false)}
                  className="absolute top-4 right-4 p-2 text-gray-400 hover:text-red-500 transition-colors"
                >
                  <LogOut className="w-5 h-5" />
                </button>
                <h3 className="text-2xl font-bold mb-6 text-center">Top Players</h3>
                <div className="space-y-3">
                  {leaderboard.map((entry, idx) => (
                    <div key={entry.id} className="flex items-center justify-between p-4 bg-gray-50 rounded-2xl border-b-2 border-gray-100">
                      <div className="flex items-center gap-4">
                        <span className="font-bold text-gray-400 w-6">#{idx + 1}</span>
                        <span className="font-bold">{entry.username}</span>
                      </div>
                      <span className="font-bold text-[#8b4513]">{entry.elo} ELO</span>
                    </div>
                  ))}
                </div>
              </motion.div>
            </div>
          )}

          {showTutorial && (
            <div className="fixed inset-0 bg-black/50 backdrop-blur-sm flex items-center justify-center p-4 z-50">
              <motion.div 
                initial={{ scale: 0.9, opacity: 0 }}
                animate={{ scale: 1, opacity: 1 }}
                className="bg-white p-8 rounded-3xl shadow-2xl max-w-lg w-full relative max-h-[80vh] overflow-y-auto"
              >
                <button 
                  onClick={() => setShowTutorial(false)}
                  className="absolute top-4 right-4 p-2 text-gray-400 hover:text-red-500 transition-colors"
                >
                  <LogOut className="w-5 h-5" />
                </button>
                <h3 className="text-2xl font-bold mb-6 text-center">How to Play</h3>
                <div className="space-y-6 text-gray-600">
                  <div>
                    <h4 className="font-bold text-[#8b4513] mb-2">Objective</h4>
                    <p>Be the first to align 4 pieces of your color in a row (horizontally, vertically, or diagonally).</p>
                  </div>
                  <div>
                    <h4 className="font-bold text-[#8b4513] mb-2">Movement</h4>
                    <p>Pieces move like chess Rooks—they slide in any of the four cardinal directions (up, down, left, right) until they hit the board edge or another piece.</p>
                  </div>
                  <div>
                    <h4 className="font-bold text-[#8b4513] mb-2">Turns</h4>
                    <p>White always moves first. Players take turns sliding one piece at a time.</p>
                  </div>
                  <button 
                    onClick={() => setShowTutorial(false)}
                    className="w-full py-3 bg-[#8b4513] text-white rounded-xl font-bold mt-4"
                  >
                    Got it!
                  </button>
                </div>
              </motion.div>
            </div>
          )}
        </div>
      </div>
    );
  }

  if (view === 'game' && gameState) {
    const isMovable = (r: number, c: number) => {
      if (gameState.status !== 'active') return false;
      const piece = gameState.board[r][c];
      if (piece !== gameState.turn) return false;
      if (!isLocal && piece !== playerColor) return false;
      return getValidMoves(gameState.board, r, c).length > 0;
    };

    const isWinner = gameState.winner === playerColor;

    return (
      <div className="min-h-screen bg-[#f4f1ea] flex flex-col items-center justify-center p-4 font-sans text-[#4a3728]">
        <div className="w-full max-w-2xl flex justify-between items-center mb-8">
          <div className={`p-4 rounded-2xl transition-all ${gameState.turn === 'W' ? 'bg-white shadow-lg scale-110 border-b-4 border-[#d2b48c]' : 'opacity-50'}`}>
            <div className="flex items-center gap-3">
              <div className="w-6 h-6 bg-white border-2 border-gray-300 rounded-full shadow-inner"></div>
              <span className="font-bold">
                {isLocal ? 'White (P1)' : (playerColor === 'W' ? (user?.username || 'You') : (opponentName || 'White'))}
              </span>
            </div>
          </div>
          <div className="text-center">
            <h2 className="text-xl font-bold text-[#8b4513]">{isLocal ? 'LOCAL' : 'VS'}</h2>
            <div className="text-xs font-bold text-gray-400 mt-1 uppercase tracking-widest">
              {gameState.status === 'active' ? 'In Progress' : 'Game Over'}
            </div>
          </div>
          <div className={`p-4 rounded-2xl transition-all ${gameState.turn === 'B' ? 'bg-white shadow-lg scale-110 border-b-4 border-[#d2b48c]' : 'opacity-50'}`}>
            <div className="flex items-center gap-3">
              <div className="w-6 h-6 bg-[#2a2a2a] rounded-full shadow-lg"></div>
              <span className="font-bold">
                {isLocal ? 'Black (P2)' : (playerColor === 'B' ? (user?.username || 'You') : (opponentName || 'Black'))}
              </span>
            </div>
          </div>
        </div>

        <div className="relative bg-[#8b4513] p-4 rounded-3xl shadow-2xl border-b-8 border-[#6d3610]">
          <div className="grid grid-cols-6 gap-2 bg-[#6d3610] p-2 rounded-xl border-4 border-[#6d3610]">
            {gameState.board.map((row, r) => (
              row.map((piece, c) => {
                const isSelected = selected?.r === r && selected?.c === c;
                const isValid = validMoves.some(m => m.r === r && m.c === c);
                const movable = !selected && isMovable(r, c);
                const winningIndex = gameState.winningLine?.findIndex(l => l.r === r && l.c === c) ?? -1;
                const isWinningPiece = winningIndex !== -1;
                
                return (
                  <div 
                    key={`${r}-${c}`}
                    onClick={() => handleSquareClick(r, c)}
                    className={`
                      w-12 h-12 sm:w-16 sm:h-16 rounded-lg flex items-center justify-center cursor-pointer transition-all relative
                      ${(r + c) % 2 === 0 ? 'bg-[#decba4]' : 'bg-[#bc9d6a]'}
                      ${(isValid || movable) ? 'ring-4 ring-green-400 ring-inset bg-green-100/50' : ''}
                      ${isSelected ? 'ring-4 ring-[#8b4513] ring-inset' : ''}
                      ${isWinningPiece ? 'ring-4 ring-orange-500 ring-inset bg-orange-100/30' : ''}
                    `}
                  >
                    <AnimatePresence mode="popLayout">
                      {piece !== '0' && (
                        <motion.div
                          layoutId={`piece-${r}-${c}`}
                          initial={{ scale: 0 }}
                          animate={{ 
                            scale: 1,
                            y: isWinningPiece ? [0, -15, 0] : 0,
                            boxShadow: isWinningPiece 
                              ? "0 0 25px 8px rgba(255, 165, 0, 0.6)" 
                              : "0 10px 15px -3px rgba(0, 0, 0, 0.1)"
                          }}
                          exit={{ scale: 0 }}
                          transition={{ 
                            type: 'spring', 
                            stiffness: 300, 
                            damping: 20,
                            y: isWinningPiece ? {
                              duration: 0.8,
                              repeat: Infinity,
                              repeatDelay: 1.5,
                              delay: winningIndex * 0.15,
                              ease: "easeInOut"
                            } : { type: 'spring' }
                          }}
                          className={`
                            w-10 h-10 sm:w-12 sm:h-12 rounded-full border-b-4 relative
                            ${piece === 'W' ? 'bg-white border-gray-300' : 'bg-[#2a2a2a] border-black'}
                            ${isWinningPiece ? 'ring-4 ring-orange-500 ring-offset-2' : ''}
                          `}
                        />
                      )}
                    </AnimatePresence>
                  </div>
                );
              })
            ))}
          </div>
        </div>

        <div className="mt-12 flex gap-4">
          <button 
            onClick={() => { setView('lobby'); setIsLocal(false); setShowWinScreen(false); }}
            className="px-8 py-3 bg-white border-2 border-[#8b4513] text-[#8b4513] rounded-xl font-bold hover:bg-gray-50 transition-all"
          >
            Quit Game
          </button>
        </div>

        <AnimatePresence>
          {showWinScreen && gameState.winner && !isWinScreenHidden && (
            <div className="fixed inset-0 bg-black/60 backdrop-blur-md flex items-center justify-center p-4 z-50">
              <motion.div 
                initial={{ y: 100, opacity: 0 }}
                animate={{ y: 0, opacity: 1 }}
                exit={{ y: 100, opacity: 0 }}
                className="bg-white p-12 rounded-[3rem] shadow-2xl max-w-md w-full text-center border-b-8 border-[#d2b48c] relative"
              >
                <button 
                  onClick={() => setIsWinScreenHidden(true)}
                  className="absolute top-6 right-6 p-2 text-gray-400 hover:text-[#8b4513] transition-colors"
                  title="Hide Result"
                >
                  <EyeOff className="w-6 h-6" />
                </button>
                <div className="flex justify-center mb-8">
                  <div className={`${isLocal ? 'bg-[#8b4513]' : (isWinner ? 'bg-green-500' : 'bg-red-500')} p-6 rounded-full`}>
                    <Trophy className="w-16 h-16 text-white" />
                  </div>
                </div>
                <h3 className={`text-4xl font-bold mb-4 ${!isLocal && (isWinner ? 'text-green-600' : 'text-red-600')}`}>
                  {isLocal ? 'Game Over!' : (isWinner ? 'Victory!' : 'Defeat...')}
                </h3>
                <p className="text-2xl text-gray-600 mb-6">
                  {gameState.winner === 'W' ? 'White' : 'Black'} has won the match
                </p>
                
                {!isLocal && (
                  <div className="bg-gray-100 p-6 rounded-2xl mb-12">
                    <p className="text-sm text-gray-500 uppercase font-bold tracking-widest mb-2">ELO Change</p>
                    <p className={`text-4xl font-bold ${isWinner ? 'text-green-600' : 'text-red-600'}`}>
                      {isWinner ? '+' : '-'}{gameState.eloChange || 25}
                    </p>
                  </div>
                )}
                
                {!isLocal && (
                  <button 
                    onClick={() => {
                      setView('lobby');
                      setIsLocal(false);
                      setShowWinScreen(false);
                      startPublicMatch();
                    }}
                    className="w-full py-5 bg-green-600 text-white rounded-2xl font-bold text-xl hover:bg-green-700 transition-all shadow-xl shadow-green-600/20 mb-4"
                  >
                    Play Again
                  </button>
                )}
                
                <button 
                  onClick={() => { setView('lobby'); setIsLocal(false); }}
                  className="w-full py-5 bg-[#8b4513] text-white rounded-2xl font-bold text-xl hover:bg-[#6d3610] transition-all shadow-xl shadow-[#8b4513]/20"
                >
                  Return to Lobby
                </button>
              </motion.div>
            </div>
          )}
        </AnimatePresence>

        {showWinScreen && isWinScreenHidden && (
          <motion.button
            initial={{ opacity: 0, y: 20 }}
            animate={{ opacity: 1, y: 0 }}
            onClick={() => setIsWinScreenHidden(false)}
            className="fixed bottom-8 right-8 bg-[#8b4513] text-white px-6 py-4 rounded-full shadow-2xl z-50 flex items-center gap-2 font-bold hover:bg-[#6d3610] transition-all"
          >
            <Trophy className="w-5 h-5" />
            Show Result
          </motion.button>
        )}
      </div>
    );
  }

  return null;
}
