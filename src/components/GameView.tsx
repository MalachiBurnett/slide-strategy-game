import React, { useMemo } from 'react';
import { motion, AnimatePresence } from 'motion/react';
import { Trophy, EyeOff } from 'lucide-react';
import { GameState, UserData, Turn } from '../types/game';
import { Piece as PieceComponent } from './Piece';
import { getValidMoves } from '../utils/gameLogic';

interface GameViewProps {
  gameState: GameState;
  isLocal: boolean;
  playerColor: Turn | null;
  user: UserData | null;
  opponentName: string | null;
  timerW: number | null;
  timerB: number | null;
  formatTime: (seconds: number | null) => string;
  handleSquareClick: (r: number, c: number) => void;
  selected: {r: number, c: number} | null;
  validMoves: {r: number, c: number}[];
  isMovable: (r: number, c: number) => boolean;
  handleQuit: () => void;
  showWinScreen: boolean;
  isWinScreenHidden: boolean;
  setIsWinScreenHidden: (hidden: boolean) => void;
  startPublicMatch: () => void;
}

export const GameView: React.FC<GameViewProps> = ({
  gameState,
  isLocal,
  playerColor,
  user,
  opponentName,
  timerW,
  timerB,
  formatTime,
  handleSquareClick,
  selected,
  validMoves,
  isMovable,
  handleQuit,
  showWinScreen,
  isWinScreenHidden,
  setIsWinScreenHidden,
  startPublicMatch,
}) => {
  const isWinner = gameState.winner === playerColor;

  const visibleSquares = useMemo(() => {
    if (gameState.variant !== 'fog_of_war' || gameState.status === 'finished') return null;
    
    const visible = new Set<string>();
    gameState.board.forEach((row, r) => {
      row.forEach((piece, c) => {
        if (piece === playerColor) {
          visible.add(`${r},${c}`);
          const moves = getValidMoves(gameState.board, r, c);
          moves.forEach(m => visible.add(`${m.r},${m.c}`));
        }
      });
    });
    return visible;
  }, [gameState.board, gameState.variant, gameState.status, playerColor]);

  return (
    <div className="min-h-screen bg-[var(--bg)] text-[var(--text)] flex flex-col items-center justify-center p-4 font-sans transition-colors duration-500">
      <div className="w-full max-w-2xl flex justify-between items-center mb-8">
        <div className={`p-4 rounded-2xl transition-all ${gameState.turn === 'W' ? 'bg-[var(--primary)] bg-opacity-10 shadow-lg scale-110 border-b-4 border-[var(--accent)]' : 'opacity-50'}`}>
          <div className="flex flex-col gap-1">
            <div className="flex items-center gap-3">
              <div className="w-6 h-6 bg-white border-2 border-gray-300 rounded-full shadow-inner"></div>
              <span className="font-bold">
                {isLocal ? 'White (P1)' : (playerColor === 'W' ? (user?.username || 'You') : (opponentName || 'White'))}
              </span>
            </div>
            {!isLocal && (
              <div className={`text-2xl font-mono font-bold text-center ${gameState.turn === 'W' && (timerW || 0) < 10 ? 'text-red-500 animate-pulse' : ''}`}>
                {formatTime(timerW)}
              </div>
            )}
          </div>
        </div>
        <div className="text-center">
          <h2 className="text-xl font-bold text-[var(--primary)]">{isLocal ? 'LOCAL' : 'VS'}</h2>
          <div className="text-xs font-bold opacity-40 mt-1 uppercase tracking-widest">
            {gameState.status === 'active' ? 'In Progress' : 'Game Over'}
          </div>
        </div>
        <div className={`p-4 rounded-2xl transition-all ${gameState.turn === 'B' ? 'bg-[var(--primary)] bg-opacity-10 shadow-lg scale-110 border-b-4 border-[var(--accent)]' : 'opacity-50'}`}>
          <div className="flex flex-col gap-1">
            <div className="flex items-center gap-3">
              <div className="w-6 h-6 bg-[#2a2a2a] rounded-full shadow-lg"></div>
              <span className="font-bold">
                {isLocal ? 'Black (P2)' : (playerColor === 'B' ? (user?.username || 'You') : (opponentName || 'Black'))}
              </span>
            </div>
            {!isLocal && (
              <div className={`text-2xl font-mono font-bold text-center ${gameState.turn === 'B' && (timerB || 0) < 10 ? 'text-red-500 animate-pulse' : ''}`}>
                {formatTime(timerB)}
              </div>
            )}
          </div>
        </div>
      </div>

      <div className="relative bg-[var(--boardBorder)] p-4 rounded-3xl shadow-2xl border-b-8 border-[var(--secondary)]">
        <div className="grid grid-cols-6 gap-2 bg-[var(--secondary)] p-2 rounded-xl border-4 border-[var(--secondary)]">
          {gameState.board.map((row, r) => (
            row.map((piece, c) => {
              const isSelected = selected?.r === r && selected?.c === c;
              const isValid = validMoves.some(m => m.r === r && m.c === c);
              const movable = !selected && isMovable(r, c);
              const winningIndex = gameState.winningLine?.findIndex(l => l.r === r && l.c === c) ?? -1;
              const isWinningPiece = winningIndex !== -1;
              const isFogged = visibleSquares && !visibleSquares.has(`${r},${c}`);
              
              const skin = piece === 'W' ? (gameState.skinW || 'classic') : (gameState.skinB || 'classic');
              const pieceUsername = piece === 'W' 
                ? (playerColor === 'W' ? user?.username : opponentName)
                : (playerColor === 'B' ? user?.username : opponentName);

              return (
                <div 
                  key={`${r}-${c}`}
                  onClick={() => !isFogged && handleSquareClick(r, c)}
                  className={`
                    w-12 h-12 sm:w-16 sm:h-16 rounded-lg flex items-center justify-center cursor-pointer transition-all relative
                    ${(r + c) % 2 === 0 ? 'bg-[var(--boardLight)]' : 'bg-[var(--boardDark)]'}
                    ${(isValid || movable) ? 'ring-4 ring-green-400 ring-inset bg-green-100/50' : ''}
                    ${isSelected ? 'ring-4 ring-[var(--primary)] ring-inset' : ''}
                    ${isWinningPiece ? 'ring-4 ring-orange-500 ring-inset bg-orange-100/30' : ''}
                    ${isFogged ? 'opacity-20 grayscale pointer-events-none' : 'opacity-100'}
                  `}
                >
                  {isFogged && (
                    <EyeOff className="w-6 h-6 text-gray-400 opacity-50 absolute" />
                  )}
                  <AnimatePresence mode="popLayout">
                    {piece !== '0' && !isFogged && (
                      <PieceComponent
                        type={piece as 'W' | 'B'}
                        skin={skin}
                        username={pieceUsername || undefined}
                        isWinningPiece={isWinningPiece}
                        winningIndex={winningIndex}
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
          onClick={handleQuit}
          className="px-8 py-3 bg-[var(--bg)] border-2 border-[var(--primary)] text-[var(--primary)] rounded-xl font-bold hover:bg-[var(--primary)] hover:text-[var(--bg)] transition-all"
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
              className="bg-[var(--bg)] p-12 rounded-[3rem] shadow-2xl max-w-md w-full text-center border-b-8 border-[var(--accent)] relative"
            >
              <button 
                onClick={() => setIsWinScreenHidden(true)}
                className="absolute top-6 right-6 p-2 opacity-40 hover:text-[var(--primary)] transition-colors"
                title="Hide Result"
              >
                <EyeOff className="w-6 h-6" />
              </button>
              <div className="flex justify-center mb-8">
                <div className={`${isLocal ? 'bg-[var(--primary)]' : (isWinner ? 'bg-green-500' : 'bg-red-500')} p-6 rounded-full`}>
                  <Trophy className="w-16 h-16 text-white" />
                </div>
              </div>
              <h3 className={`text-4xl font-bold mb-4 ${!isLocal && (isWinner ? 'text-green-600' : 'text-red-600')}`}>
                {isLocal ? 'Game Over!' : (isWinner ? 'Victory!' : 'Defeat...')}
              </h3>
              <p className="text-2xl opacity-60 mb-6">
                {gameState.winner === 'W' ? 'White' : 'Black'} has won the match
              </p>
              
              {!isLocal && (
                <div className="bg-[var(--primary)] bg-opacity-5 p-6 rounded-2xl mb-12">
                  <p className="text-sm opacity-40 uppercase font-bold tracking-widest mb-2">ELO Change</p>
                  <p className={`text-4xl font-bold ${isWinner ? 'text-green-600' : 'text-red-600'}`}>
                    {isWinner ? '+' : '-'}{gameState.eloChange || 25}
                  </p>
                </div>
              )}
              
              {!isLocal && (
                <button 
                  onClick={() => {
                    handleQuit();
                    startPublicMatch();
                  }}
                  className="w-full py-5 bg-green-600 text-white rounded-2xl font-bold text-xl hover:bg-green-700 transition-all shadow-xl shadow-green-600/20 mb-4"
                >
                  Play Again
                </button>
              )}
              
              <button 
                onClick={handleQuit}
                className="w-full py-5 bg-[var(--primary)] text-[var(--bg)] rounded-2xl font-bold text-xl hover:opacity-90 transition-all shadow-xl shadow-[var(--primary)]/20"
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
          className="fixed bottom-8 right-8 bg-[var(--primary)] text-[var(--bg)] px-6 py-4 rounded-full shadow-2xl z-50 flex items-center gap-2 font-bold hover:scale-110 transition-all"
        >
          <Trophy className="w-5 h-5" />
          Show Result
        </motion.button>
      )}
    </div>
  );
};
