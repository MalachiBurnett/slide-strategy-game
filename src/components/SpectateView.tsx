import React, { useState, useEffect } from 'react';
import { motion, AnimatePresence } from 'motion/react';
import { ArrowLeft, Eye } from 'lucide-react';
import { GameState, UserData, Turn, Skin } from '../types/game';
import { Piece as PieceComponent } from './Piece';
import { Socket } from 'socket.io-client';

interface SpectateViewProps {
  user: UserData | null;
  setView: (view: 'auth' | 'lobby' | 'game' | 'queue' | 'cosmetics' | 'spectate' | 'profile' | 'credits') => void;
  socket: Socket;
  formatTime: (seconds: number | null) => string;
}

export const SpectateView: React.FC<SpectateViewProps> = ({
  user,
  setView,
  socket,
  formatTime,
}) => {
  const [inputUsername, setInputUsername] = useState('');
  const [status, setStatus] = useState<'input' | 'loading' | 'waiting' | 'spectating' | 'error'>('input');
  const [spectatingUsername, setSpectatingUsername] = useState('');
  const [gameState, setGameState] = useState<GameState | null>(null);
  const [playerWName, setPlayerWName] = useState('');
  const [playerBName, setPlayerBName] = useState('');
  const [timerW, setTimerW] = useState<number | null>(null);
  const [timerB, setTimerB] = useState<number | null>(null);
  const [error, setError] = useState('');

  useEffect(() => {
    // Check if there's a username in localStorage to spectate
    const storedUsername = localStorage.getItem('spectateUsername');
    if (storedUsername) {
      setInputUsername(storedUsername);
      localStorage.removeItem('spectateUsername');
      // Trigger the spectate action with the stored username
      setTimeout(() => {
        handleSpectateWithUsername(storedUsername);
      }, 100);
    }
  }, []);

  useEffect(() => {
    socket.on('game_update', (data: GameState) => {
      setGameState(data);
      if (data.timerW !== undefined) setTimerW(data.timerW);
      if (data.timerB !== undefined) setTimerB(data.timerB);
    });

    socket.on('timer_update', (data: { timerW: number, timerB: number }) => {
      setTimerW(data.timerW);
      setTimerB(data.timerB);
    });

    return () => {
      socket.off('game_update');
      socket.off('timer_update');
    };
  }, [socket]);

  const handleSpectateWithUsername = (username: string) => {
    if (!username.trim()) {
      setError('Please enter a username');
      return;
    }

    setStatus('loading');
    setError('');
    setSpectatingUsername(username);

    // Check if player is in a game
    socket.emit('get_player_status', { username }, (response: { inGame: boolean, gameId?: string }) => {
      if (response.inGame) {
        // Try to spectate
        socket.emit('spectate', { spectatorId: user?.id, targetUsername: username }, (response: { success: boolean, message?: string, gameData?: any }) => {
          if (response.success) {
            setGameState(response.gameData);
            setPlayerWName(response.gameData.playerWUsername || 'Player W');
            setPlayerBName(response.gameData.playerBUsername || 'Player B');
            setTimerW(response.gameData.timerW);
            setTimerB(response.gameData.timerB);
            setStatus('spectating');
          } else {
            setError(response.message || 'Failed to spectate');
            setStatus('waiting');
            socket.emit('start_spectating_wait', { spectatorId: user?.id, targetUsername: username });
          }
        });
      } else {
        // Player is not in game, wait for them
        setStatus('waiting');
        socket.emit('start_spectating_wait', { spectatorId: user?.id, targetUsername: username });
      }
    });
  };

  const handleSpectate = () => {
    handleSpectateWithUsername(inputUsername);
  };

  const handleBack = () => {
    if (status !== 'input') {
      socket.emit('stop_spectating', { spectatorId: user?.id });
    }
    setStatus('input');
    setInputUsername('');
    setSpectatingUsername('');
    setGameState(null);
    setError('');
    setView('lobby');
  };

  if (status === 'input') {
    return (
      <div className="min-h-screen bg-[var(--bg)] text-[var(--text)] flex flex-col items-center justify-center p-4 font-sans transition-colors duration-500">
        <motion.div
          initial={{ opacity: 0, scale: 0.9 }}
          animate={{ opacity: 1, scale: 1 }}
          className="w-full max-w-md"
        >
          <div className="bg-[var(--bgLight)] p-8 rounded-3xl shadow-2xl border-b-8 border-[var(--primary)]">
            <div className="flex items-center gap-3 mb-8">
              <Eye className="w-8 h-8 text-[var(--primary)]" />
              <h1 className="text-3xl font-black">Spectate</h1>
            </div>

            <div className="space-y-4">
              <input
                type="text"
                placeholder="Enter player username"
                value={inputUsername}
                onChange={(e) => setInputUsername(e.target.value)}
                onKeyPress={(e) => e.key === 'Enter' && handleSpectate()}
                className="w-full px-4 py-3 bg-[var(--bg)] text-[var(--text)] rounded-xl border-2 border-[var(--primary)] focus:outline-none focus:border-[var(--accent)] transition-colors"
              />

              {error && (
                <p className="text-red-500 text-sm font-bold">{error}</p>
              )}

              <button
                onClick={handleSpectate}
                className="w-full py-3 bg-[var(--primary)] text-[var(--primaryText)] rounded-xl font-black text-lg hover:scale-105 active:scale-95 transition-all shadow-lg"
              >
                Find and Spectate
              </button>

              <button
                onClick={() => setView('lobby')}
                className="w-full py-3 bg-[var(--bgLight)] text-[var(--text)] rounded-xl font-bold border-2 border-[var(--primary)] hover:opacity-80 transition-all"
              >
                Back to Lobby
              </button>
            </div>
          </div>
        </motion.div>
      </div>
    );
  }

  if (status === 'waiting') {
    return (
      <div className="min-h-screen bg-[var(--bg)] text-[var(--text)] flex flex-col items-center justify-center p-4 font-sans transition-colors duration-500">
        <motion.div
          initial={{ opacity: 0, scale: 0.9 }}
          animate={{ opacity: 1, scale: 1 }}
          className="w-full max-w-md text-center"
        >
          <div className="bg-[var(--bgLight)] p-8 rounded-3xl shadow-2xl border-b-8 border-[var(--primary)]">
            <h1 className="text-3xl font-black mb-4">{spectatingUsername}</h1>
            <p className="text-lg opacity-60 mb-8">is not playing a match right now</p>

            <div className="mb-8 p-6 bg-[var(--bg)] rounded-2xl">
              <div className="animate-spin inline-block mb-4">
                <Eye className="w-12 h-12 text-[var(--primary)]" />
              </div>
              <p className="font-bold text-[var(--primary)]">Waiting for them to start a game...</p>
            </div>

            <button
              onClick={handleBack}
              className="w-full py-3 bg-red-500 text-white rounded-xl font-black text-lg hover:scale-105 active:scale-95 transition-all shadow-lg"
            >
              Stop Waiting
            </button>
          </div>
        </motion.div>
      </div>
    );
  }

  if (status === 'spectating' && gameState) {
    return (
      <div className="min-h-screen bg-[var(--bg)] text-[var(--text)] flex flex-col items-center justify-center p-4 font-sans transition-colors duration-500">
        <div className="w-full max-w-2xl">
          <button
            onClick={handleBack}
            className="mb-6 flex items-center gap-2 px-4 py-2 bg-[var(--primary)] text-[var(--primaryText)] rounded-xl font-bold hover:opacity-90 transition-all"
          >
            <ArrowLeft className="w-5 h-5" />
            Back
          </button>

          <div className="flex justify-between items-center mb-8">
            <div className={`p-4 rounded-2xl transition-all ${gameState.turn === 'W' ? 'bg-[var(--primary)] bg-opacity-10 shadow-lg scale-110 border-b-4 border-[var(--accent)]' : 'opacity-50'}`}>
              <div className="flex flex-col gap-1">
                <div className="flex items-center gap-3">
                  <div className="w-6 h-6 bg-white border-2 border-gray-300 rounded-full shadow-inner"></div>
                  <span className="font-bold">{playerWName}</span>
                </div>
                <div className={`text-2xl font-mono font-bold text-center ${gameState.turn === 'W' && (timerW || 0) < 10 ? 'text-red-500 animate-pulse' : ''}`}>
                  {formatTime(timerW)}
                </div>
              </div>
            </div>
            <div className="text-center">
              <h2 className="text-xl font-bold text-[var(--primary)]">SPECTATING</h2>
              <div className="text-xs font-bold opacity-40 mt-1 uppercase tracking-widest">
                {gameState.status === 'active' ? 'In Progress' : 'Game Over'}
              </div>
            </div>
            <div className={`p-4 rounded-2xl transition-all ${gameState.turn === 'B' ? 'bg-[var(--primary)] bg-opacity-10 shadow-lg scale-110 border-b-4 border-[var(--accent)]' : 'opacity-50'}`}>
              <div className="flex flex-col gap-1">
                <div className="flex items-center gap-3">
                  <div className="w-6 h-6 bg-[#2a2a2a] rounded-full shadow-lg"></div>
                  <span className="font-bold">{playerBName}</span>
                </div>
                <div className={`text-2xl font-mono font-bold text-center ${gameState.turn === 'B' && (timerB || 0) < 10 ? 'text-red-500 animate-pulse' : ''}`}>
                  {formatTime(timerB)}
                </div>
              </div>
            </div>
          </div>

          <div className="bg-[var(--bgLight)] rounded-3xl p-8 shadow-2xl">
            <div className="grid grid-cols-6 gap-2 mb-8">
              {gameState.board.map((row, r) =>
                row.map((piece, c) => (
                  <div
                    key={`${r}-${c}`}
                    className="aspect-square bg-[var(--boardBg)] rounded-xl border-2 border-[var(--primary)] shadow-md flex items-center justify-center cursor-default"
                  >
                    <PieceComponent piece={piece} skin={piece === 'W' ? gameState.skinW : gameState.skinB} />
                  </div>
                ))
              )}
            </div>
          </div>
        </div>
      </div>
    );
  }

  return null;
};
