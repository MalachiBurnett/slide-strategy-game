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

const socket: Socket = io();

export default function App() {
  console.log("App rendering...");
  const [user, setUser] = useState<UserData | null>(null);
  const [view, setView] = useState<'auth' | 'lobby' | 'game' | 'queue' | 'cosmetics'>('auth');
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
  const [isRated, setIsRated] = useState(true);
  const [joinCode, setJoinCode] = useState('');
  const [isLocal, setIsLocal] = useState(false);
  const [showWinScreen, setShowWinScreen] = useState(false);
  const [isWinScreenHidden, setIsWinScreenHidden] = useState(false);
  const [showLeaderboard, setShowLeaderboard] = useState(false);
  const [leaderboard, setLeaderboard] = useState<LeaderboardEntry[]>([]);
  const [showTutorial, setShowTutorial] = useState(false);
  const [onlineCount, setOnlineCount] = useState(0);
  const [queueCounts, setQueueCounts] = useState<Record<string, number>>({ '10|0': 0, '1|0': 0, '3|2': 0 });
  const [timeControl, setTimeControl] = useState<'10|0' | '1|0' | '3|2'>('10|0');
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
      if (view === 'auth') setView('lobby');
    }
  }, [view]);

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

  const onUpdateCosmetics = async (skin: Skin, theme: Theme) => {
    const res = await fetch('/api/cosmetics', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ skin, theme })
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
      skinB: Skin
    }) => {
      setGameId(data.gameId);
      setPlayerColor(data.color);
      setOpponentName(data.opponentName || 'Opponent');
      setTimerW(data.timerW || null);
      setTimerB(data.timerB || null);
      socket.emit('join_game', data.gameId);
      setGameState({ 
        board: INITIAL_BOARD, 
        turn: 'W', 
        status: 'active', 
        winner: null,
        skinW: data.skinW,
        skinB: data.skinB
      });
      setView('game');
      setPrivateCode(null);
    });

    socket.on('private_created', (data: { code: string, gameId: string, timerW?: number, timerB?: number }) => {
      setPrivateCode(data.code);
      setGameId(data.gameId);
      setPlayerColor('W');
      setTimerW(data.timerW || null);
      setTimerB(data.timerB || null);
      socket.emit('join_game', data.gameId);
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

    socket.on('queue_counts', (counts: Record<string, number>) => {
      setQueueCounts(counts);
    });

    socket.on('error', (msg: string) => {
      setError(msg);
    });

    return () => {
      socket.off('match_found');
      socket.off('private_created');
      socket.off('game_update');
      socket.off('timer_update');
      socket.off('online_count');
      socket.off('queue_counts');
      socket.off('error');
    };
  }, []);

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
    setTimerW(null);
    setTimerB(null);
    socket.emit('join_queue', { userId: user.id, elo: user.elo, timeControl });
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
    socket.emit('create_private', { userId: user.id, timeControl });
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
    setGameState({ board: INITIAL_BOARD, turn: 'W', status: 'active', winner: null });
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
    setView('lobby');
    setIsLocal(false);
    setShowWinScreen(false);
  };

  const isMovable = (r: number, c: number) => {
    if (!gameState || gameState.status !== 'active') return false;
    const piece = gameState.board[r][c];
    if (piece !== gameState.turn) return false;
    if (!isLocal && piece !== playerColor) return false;
    return getValidMoves(gameState.board, r, c).length > 0;
  };

  if (view === 'auth') {
    return (
      <AuthView 
        authMode={authMode}
        setAuthMode={setAuthMode}
        username={username}
        setUsername={setUsername}
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
        showLeaderboard={showLeaderboard}
        setShowLeaderboard={setShowLeaderboard}
        leaderboard={leaderboard}
        fetchLeaderboard={fetchLeaderboard}
        showTutorial={showTutorial}
        setShowTutorial={setShowTutorial}
        setUser={setUser}
        setView={setView}
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

  if (view === 'queue') {
    return (
      <QueueView 
        timeControl={timeControl}
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

  return null;
}
