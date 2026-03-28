import React, { useState, useEffect, useCallback } from 'react';
import { io, Socket } from 'socket.io-client';
import { INITIAL_BOARD, THEMES } from './constants/game';
import { GameState, UserData, LeaderboardEntry, Turn, Skin, Theme } from './types/game';
import { getValidMoves, checkWin } from './utils/gameLogic';
import { AuthView } from './components/AuthView';
import { LobbyView } from './components/LobbyView';
import { QueueView } from './components/QueueView';
import { GameView } from './components/GameView';
import { CosmeticsView } from './components/CosmeticsView';
import { ProfileView, ResetPasswordView } from './components/ProfileView';
import { CreditsView } from './components/CreditsView';
import { SpectateView } from './components/SpectateView';
import { Mail, CheckCircle, XCircle, Loader2 } from 'lucide-react';
import { motion } from 'motion/react';

const socket: Socket = io();

const VerifyView: React.FC<{ token: string, mode: 'auth' | 'email-change', onDone: () => void }> = ({ token, mode, onDone }) => {
  const [status, setStatus] = useState<'idle' | 'loading' | 'success' | 'error'>('idle');
  const [error, setError] = useState('');

  const handleVerify = async () => {
    setStatus('loading');
    const endpoint = mode === 'auth' ? `/api/verify/${token}` : `/api/profile/verify-email-change/${token}`;
    try {
      const res = await fetch(endpoint, { method: 'POST' });
      if (res.ok) {
        setStatus('success');
      } else {
        const data = await res.json();
        setError(data.error || 'Verification failed');
        setStatus('error');
      }
    } catch (e) {
      setError('Connection error');
      setStatus('error');
    }
  };

  return (
    <div className="min-h-screen bg-[var(--bg)] flex items-center justify-center p-4 font-sans text-[var(--text)] transition-colors duration-500">
      <motion.div 
        initial={{ opacity: 0, scale: 0.9 }}
        animate={{ opacity: 1, scale: 1 }}
        className="bg-[var(--bgLight)] p-10 rounded-[2.5rem] shadow-2xl w-full max-w-md border-b-8 border-[var(--primary)] text-center"
      >
        <div className="flex justify-center mb-8">
          <div className="bg-[var(--primary)] p-5 rounded-3xl shadow-lg">
            <Mail className="w-14 h-14 text-[var(--primaryText)]" />
          </div>
        </div>
        
        <h1 className="text-3xl font-black mb-4">{mode === 'auth' ? 'Email Verification' : 'Email Change Verification'}</h1>
        
        {status === 'idle' && (
          <>
            <p className="opacity-60 mb-4 font-medium text-red-500">If you don't see the email, please check your junk or spam folder!</p>
            <p className="opacity-60 mb-10 font-medium">Ready? Click the button below to verify your email address.</p>
            <button 
              onClick={handleVerify}
              className="w-full py-5 bg-[var(--primary)] text-[var(--primaryText)] rounded-2xl font-black text-xl hover:scale-105 active:scale-95 transition-all shadow-xl shadow-[var(--primary)]/20"
            >
              Verify My Email
            </button>
          </>
        )}

        {status === 'loading' && (
          <div className="flex flex-col items-center py-8">
            <Loader2 className="w-12 h-12 text-[var(--primary)] animate-spin mb-4" />
            <p className="font-bold opacity-60">Verifying...</p>
          </div>
        )}

        {status === 'success' && (
          <>
            <div className="flex justify-center mb-6">
              <CheckCircle className="w-16 h-16 text-green-500" />
            </div>
            <p className="text-xl font-bold mb-8">Verification Successful!</p>
            <button 
              onClick={onDone}
              className="w-full py-4 bg-[var(--primary)] text-[var(--primaryText)] rounded-2xl font-black hover:opacity-90 transition-all"
            >
              Continue
            </button>
          </>
        )}

        {status === 'error' && (
          <>
            <div className="flex justify-center mb-6">
              <XCircle className="w-16 h-16 text-red-500" />
            </div>
            <p className="text-xl font-bold mb-2 text-red-500">Error</p>
            <p className="opacity-60 mb-8">{error}</p>
            <button 
              onClick={onDone}
              className="w-full py-4 bg-[var(--primary)] text-[var(--primaryText)] rounded-2xl font-black hover:opacity-90 transition-all"
            >
              Go Back
            </button>
          </>
        )}
      </motion.div>
    </div>
  );
};

export default function App() {
  const [user, setUser] = useState<UserData | null>(null);
  const [view, setView] = useState<'auth' | 'lobby' | 'game' | 'queue' | 'cosmetics' | 'verify' | 'profile' | 'reset-password' | 'credits' | 'spectate'>('auth');
  const [authMode, setAuthMode] = useState<'login' | 'register'>('login');
  const [username, setUsername] = useState('');
  const [email, setEmail] = useState('');
  const [password, setPassword] = useState('');
  const [error, setError] = useState('');
  const [verifyToken, setVerifyToken] = useState<string | null>(null);
  const [verifyMode, setVerifyMode] = useState<'auth' | 'email-change'>('auth');
  const [resetToken, setResetToken] = useState<string | null>(null);

  useEffect(() => {
    const path = window.location.pathname;
    if (path.startsWith('/verify/')) {
      const token = path.split('/verify/')[1];
      if (token) {
        setVerifyToken(token);
        setVerifyMode('auth');
        setView('verify');
      }
    } else if (path.startsWith('/verify-email-change/')) {
      const token = path.split('/verify-email-change/')[1];
      if (token) {
        setVerifyToken(token);
        setVerifyMode('email-change');
        setView('verify');
      }
    } else if (path.startsWith('/reset-password/')) {
      const token = path.split('/reset-password/')[1];
      if (token) {
        setResetToken(token);
        setView('reset-password');
      }
    }
  }, []);
  
  const [gameId, setGameId] = useState<string | null>(null);
  const [gameState, setGameState] = useState<GameState | null>(null);
  const [playerColor, setPlayerColor] = useState<Turn | null>(null);
  const [opponentName, setOpponentName] = useState<string | null>(null);
  const [selected, setSelected] = useState<{r: number, c: number} | null>(null);
  const [validMoves, setValidMoves] = useState<{r: number, c: number}[]>([]);
  const [privateCode, setPrivateCode] = useState<string | null>(null);
  const [isRated, setIsRated] = useState(true);
  const [joinCode, setJoinCode] = useState('');
  const [isLocal, setIsLocal] = useState(false);
  const [showWinScreen, setShowWinScreen] = useState(false);
  const [isWinScreenHidden, setIsWinScreenHidden] = useState(false);
  const [showLeaderboard, setShowLeaderboard] = useState(false);
  const [leaderboard, setLeaderboard] = useState<LeaderboardEntry[]>([]);
  const [showTutorial, setShowTutorial] = useState(false);
  const [onlineCount, setOnlineCount] = useState(0);
  const [queueCounts, setQueueCounts] = useState<Record<string, Record<string, number>>>({});
  const [timeControl, setTimeControl] = useState<'0.25|3' | '1|0' | '3|2'>('0.25|3');
  const [variant, setVariant] = useState<string>('classic');
  const [timerW, setTimerW] = useState<number | null>(null);
  const [timerB, setTimerB] = useState<number | null>(null);

  // Apply theme
  useEffect(() => {
    const themeId = user?.theme || 'wooden';
    const theme = THEMES.find(t => t.id === themeId) || THEMES[0];
    const root = document.documentElement;
    Object.entries(theme.colors).forEach(([key, value]) => {
      root.style.setProperty(`--${key}`, value);
    });
  }, [user?.theme]);

  const checkReconnect = useCallback((userId: number) => {
    socket.emit('check_reconnect', { userId }, (response: any) => {
      if (response.inGame) {
        const data = response.gameData;
        setGameId(data.gameId);
        setPlayerColor(data.color);
        setOpponentName(data.opponentName);
        setTimerW(data.timerW);
        setTimerB(data.timerB);
        setGameState({
          board: data.board,
          turn: data.turn,
          status: 'active',
          winner: null,
          skinW: data.skinW,
          skinB: data.skinB,
          variant: data.variant
        });
        setView('game');
      }
    });
  }, []);

  // Auth Functions
  const handleAuth = async (e: React.FormEvent) => {
    e.preventDefault();
    const endpoint = authMode === 'login' ? '/api/login' : '/api/register';
    const body = authMode === 'login' 
      ? { username, password } 
      : { username, email, password };
      
    const res = await fetch(endpoint, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body)
    });
    const data = await res.json();
    if (res.ok) {
      if (authMode === 'login') {
        setUser(data);
        setView('lobby');
        checkReconnect(data.id);
      } else {
        setAuthMode('login');
        setError('Registration successful! Please check your email (and junk/spam folder) to verify your account.');
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
      checkReconnect(data.id);
    }
  };

  const handleLogout = async () => {
    await fetch('/api/logout', { method: 'POST' });
    setUser(null);
    setView('auth');
  };

  const checkMe = useCallback(async () => {
    const res = await fetch('/api/me');
    if (res.ok) {
      const data = await res.json();
      setUser(data);
      if (view === 'auth') setView('lobby');
      checkReconnect(data.id);
    } else {
      setUser(null);
      if (view !== 'auth' && view !== 'verify') setView('auth');
    }
  }, [view, checkReconnect]);

  useEffect(() => {
    checkMe();
  }, []); 

  const fetchLeaderboard = async () => {
    const res = await fetch('/api/leaderboard');
    if (res.ok) {
      const data = await res.json();
      setLeaderboard(data);
    }
  };

  const onUpdateCosmetics = async (skin: Skin, theme: Theme) => {
    const body: any = {};
    if (skin !== user?.skin) body.skin = skin;
    if (theme !== user?.theme) body.theme = theme;
    if (Object.keys(body).length === 0) return;

    const res = await fetch('/api/cosmetics', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body)
    });
    if (res.ok) {
      setUser(prev => prev ? { ...prev, skin, theme } : null);
    }
  };

  // Game Functions
  useEffect(() => {
    socket.on('match_found', (data: { 
      gameId: string, 
      color: Turn, 
      opponentName?: string, 
      timerW?: number, 
      timerB?: number,
      skinW: Skin,
      skinB: Skin,
      variant: string,
      board: string[][]
    }) => {
      setGameId(data.gameId);
      setPlayerColor(data.color);
      setOpponentName(data.opponentName || 'Opponent');
      setTimerW(data.timerW || null);
      setTimerB(data.timerB || null);
      socket.emit('join_game', data.gameId);
      setGameState({ 
        board: data.board, 
        turn: 'W', 
        status: 'active', 
        winner: null,
        skinW: data.skinW,
        skinB: data.skinB,
        variant: data.variant
      });
      setView('game');
      setPrivateCode(null);
    });

    socket.on('private_created', (data: { code: string, gameId: string, timerW?: number, timerB?: number, variant: string }) => {
      setPrivateCode(data.code);
      setGameId(data.gameId);
      setPlayerColor('W');
      setTimerW(data.timerW || null);
      setTimerB(data.timerB || null);
      socket.emit('join_game', data.gameId);
    });

    socket.on('join_private_success', (data: { 
      gameId: string, 
      opponentName: string, 
      skinW: Skin, 
      skinB: Skin, 
      variant: string,
      board: string[][],
      timerW?: number,
      timerB?: number
    }) => {
      setOpponentName(data.opponentName);
      setTimerW(data.timerW || null);
      setTimerB(data.timerB || null);
      setGameState({ 
        board: data.board, 
        turn: 'W', 
        status: 'active', 
        winner: null,
        skinW: data.skinW,
        skinB: data.skinB,
        variant: data.variant,
        timerW: data.timerW,
        timerB: data.timerB
      });
      setView('game');
      setPrivateCode(null);
    });

    socket.on('private_rated_updated', (isRated: boolean) => {
      setIsRated(isRated);
    });

    socket.on('game_update', (data: GameState) => {
      setGameState(data);
      if (data.timerW !== undefined) setTimerW(data.timerW);
      if (data.timerB !== undefined) setTimerB(data.timerB);
    });

    socket.on('timer_update', (data: { timerW: number, timerB: number }) => {
      setTimerW(data.timerW);
      setTimerB(data.timerB);
    });

    socket.on('online_count', (count: number) => {
      setOnlineCount(count);
    });

    socket.on('queue_counts', (counts: Record<string, Record<string, number>>) => {
      setQueueCounts(counts);
    });

    socket.on('error', (msg: string) => {
      setError(msg);
    });

    socket.on('player_disconnected', (data: { userId: number, timeout: number }) => {
      if (data.userId !== user?.id) {
        setError(`Opponent disconnected. They have ${Math.ceil(data.timeout)}s to reconnect.`);
      }
    });

    socket.on('player_reconnected', (data: { userId: number }) => {
      if (data.userId !== user?.id) {
        setError('');
      }
    });

    return () => {
      socket.off('match_found');
      socket.off('private_created');
      socket.off('game_update');
      socket.off('timer_update');
      socket.off('online_count');
      socket.off('queue_counts');
      socket.off('error');
      socket.off('player_disconnected');
      socket.off('player_reconnected');
    };
  }, [user?.id]);

  useEffect(() => {
    if (gameState?.status === 'finished' && user && !isLocal) {
      checkMe();
    }
  }, [gameState?.status, user, isLocal, checkMe]);

  useEffect(() => {
    if (gameState?.winner) {
      const timer = setTimeout(() => {
        setShowWinScreen(true);
      }, 800);
      return () => clearTimeout(timer);
    } else {
      setShowWinScreen(false);
      setIsWinScreenHidden(false);
    }
  }, [gameState?.winner]);

  const handleSquareClick = (r: number, c: number) => {
    if (!gameState || gameState.status !== 'active') return;
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
          
          if (gameState.variant === 'schizophrenic') {
            const randR = Math.floor(Math.random() * 6);
            const randC = Math.floor(Math.random() * 6);
            newBoard[randR][randC] = ['0', 'W', 'B'][Math.floor(Math.random() * 3)];
          }

          const winResult = checkWin(newBoard);
          const winner = winResult ? winResult.winner : null;
          const winningLine = winResult ? winResult.line : null;
          
          setGameState({
            ...gameState,
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
    setTimerW(null);
    setTimerB(null);
    socket.emit('join_queue', { userId: user.id, elo: user.elo, timeControl, variant });
    setView('queue');
    setError('');
  };

  const leaveQueue = () => {
    if (!user) return;
    socket.emit('leave_queue', { userId: user.id });
    setView('lobby');
  };

  const createPrivateMatch = () => {
    if (!user) return;
    socket.emit('create_private', { userId: user.id, timeControl, variant });
  };

  const joinPrivateMatch = () => {
    if (!user || !joinCode) return;
    socket.emit('join_private', { userId: user.id, code: joinCode.toUpperCase() });
  };

  const toggleRated = () => {
    if (!gameId) return;
    socket.emit('toggle_private_rated', { gameId, isRated: !isRated });
  };

  const startLocalMatch = () => {
    setIsLocal(true);
    setGameId('local');
    setPlayerColor('W');
    
    let board = INITIAL_BOARD;
    if (variant === 'random_setup') {
      const b = Array(6).fill(null).map(() => Array(6).fill('0'));
      const pieces = ['W', 'W', 'W', 'W', 'W', 'W', 'B', 'B', 'B', 'B', 'B', 'B'];
      let placed = 0;
      while (placed < 12) {
        const r = Math.floor(Math.random() * 6);
        const c = Math.floor(Math.random() * 6);
        if (b[r][c] === '0') {
          b[r][c] = pieces[placed];
          placed++;
        }
      }
      board = b;
    }

    setGameState({ board, turn: 'W', status: 'active', winner: null, variant });
    setView('game');
  };

  const formatTime = (seconds: number | null) => {
    if (seconds === null) return '--:--';
    const mins = Math.floor(Math.max(0, seconds) / 60);
    const secs = Math.max(0, seconds) % 60;
    return `${mins}:${secs.toString().padStart(2, '0')}`;
  };

  const handleQuit = () => {
    if (gameState?.status === 'active' && !isLocal) {
      socket.emit('forfeit', { gameId, userId: user?.id });
    }
    setGameId(null);
    setGameState(null);
    setOpponentName(null);
    setSelected(null);
    setValidMoves([]);
    setPrivateCode(null);
    setView('lobby');
    setIsLocal(false);
    setShowWinScreen(false);
    setError('');
  };

  const isMovable = (r: number, c: number) => {
    if (!gameState || gameState.status !== 'active') return false;
    const piece = gameState.board[r][c];
    if (piece !== gameState.turn) return false;
    if (!isLocal && piece !== playerColor) return false;
    return getValidMoves(gameState.board, r, c).length > 0;
  };

  if (view === 'verify' && verifyToken) {
    return <VerifyView token={verifyToken} mode={verifyMode} onDone={() => {
      window.history.replaceState({}, '', '/');
      setVerifyToken(null);
      if (verifyMode === 'auth') {
        setView('auth');
        setAuthMode('login');
      } else {
        setView('lobby');
      }
    }} />;
  }

  if (view === 'reset-password' && resetToken) {
    return <ResetPasswordView token={resetToken} onDone={() => {
      window.history.replaceState({}, '', '/');
      setResetToken(null);
      setView('auth');
      setAuthMode('login');
    }} />;
  }

  if (view === 'auth') {
    return (
      <AuthView 
        authMode={authMode}
        setAuthMode={setAuthMode}
        username={username}
        setUsername={setUsername}
        email={email}
        setEmail={setEmail}
        password={password}
        setPassword={setPassword}
        error={error}
        handleAuth={handleAuth}
        handleGuest={handleGuest}
      />
    );
  }

  if (view === 'lobby') {
    return (
      <LobbyView 
        user={user}
        onlineCount={onlineCount}
        timeControl={timeControl}
        setTimeControl={setTimeControl}
        variant={variant}
        setVariant={setVariant}
        queueCounts={queueCounts}
        showLeaderboard={showLeaderboard}
        setShowLeaderboard={setShowLeaderboard}
        leaderboard={leaderboard}
        fetchLeaderboard={fetchLeaderboard}
        showTutorial={showTutorial}
        setShowTutorial={setShowTutorial}
        setUser={setUser}
        setView={setView}
        handleLogout={handleLogout}
        startPublicMatch={startPublicMatch}
        startLocalMatch={startLocalMatch}
        createPrivateMatch={createPrivateMatch}
        joinPrivateMatch={joinPrivateMatch}
        joinCode={joinCode}
        setJoinCode={setJoinCode}
        privateCode={privateCode}
        setPrivateCode={setPrivateCode}
        setGameId={setGameId}
        setPlayerColor={setPlayerColor}
        error={error}
        isRated={isRated}
        toggleRated={toggleRated}
        socket={socket}
      />
    );
  }

  if (view === 'cosmetics' && user) {
    return (
      <CosmeticsView 
        user={user}
        leaderboard={leaderboard}
        onBack={() => setView('lobby')}
        onUpdateCosmetics={onUpdateCosmetics}
      />
    );
  }

  if (view === 'profile' && user) {
    return (
      <ProfileView 
        user={user}
        onBack={() => setView('lobby')}
        onUpdateUser={(updated) => setUser(prev => prev ? { ...prev, ...updated } : null)}
      />
    );
  }

  if (view === 'queue') {
    return (
      <QueueView 
        timeControl={timeControl}
        variant={variant}
        queueCounts={queueCounts}
        leaveQueue={leaveQueue}
      />
    );
  }

  if (view === 'game' && gameState) {
    return (
      <GameView 
        gameState={gameState}
        isLocal={isLocal}
        playerColor={playerColor}
        user={user}
        opponentName={opponentName}
        timerW={timerW}
        timerB={timerB}
        formatTime={formatTime}
        handleSquareClick={handleSquareClick}
        selected={selected}
        validMoves={validMoves}
        isMovable={isMovable}
        handleQuit={handleQuit}
        showWinScreen={showWinScreen}
        isWinScreenHidden={isWinScreenHidden}
        setIsWinScreenHidden={setIsWinScreenHidden}
        startPublicMatch={startPublicMatch}
      />
    );
  }

  if (view === 'credits') {
    return (
      <CreditsView 
        onBack={() => setView('lobby')}
      />
    );
  }

  if (view === 'spectate' && user) {
    return (
      <SpectateView
        user={user}
        setView={setView}
        socket={socket}
        formatTime={formatTime}
      />
    );
  }

  return null;
}
