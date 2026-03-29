import React, { useState, useEffect, useMemo, useCallback } from 'react';
import { motion, AnimatePresence } from 'motion/react';
import { Trophy, ChevronRight } from 'lucide-react';
import { Board } from '../types/game';
import { Piece as PieceComponent } from './Piece';
import { getValidMoves, checkWin } from '../utils/gameLogic';
import { INITIAL_BOARD } from '../constants/game';

interface TutorialViewProps {
  onComplete: () => void;
}

interface Step {
  text: string;
  action?: 'bot_move' | 'user_move' | 'user_win';
  highlight?: { r: number, c: number }[];
  moveData?: { from: { r: number, c: number }, to: { r: number, c: number } };
  waitCondition?: 'click' | 'move';
}

export const TutorialView: React.FC<TutorialViewProps> = ({ onComplete }) => {
  const [stepIndex, setStepIndex] = useState(0);
  const [board, setBoard] = useState<Board>(INITIAL_BOARD);
  const [selected, setSelected] = useState<{ r: number, c: number } | null>(null);
  const [validMoves, setValidMoves] = useState<{ r: number, c: number }[]>([]);
  const [message, setMessage] = useState('');
  const [isBotMoving, setIsBotMoving] = useState(false);
  const [winningLine, setWinningLine] = useState<{r: number, c: number}[] | null>(null);

  const steps: Step[] = useMemo(() => [
    { text: "Hello! i am Tutorial bot and i am here to teach you how to play slide!", waitCondition: 'click' },
    { text: "This is the board. it is 6 squares wide and 6 squares tall. this is where the game is played.", waitCondition: 'click' },
    { text: "Each player starts with 6 pieces of thier color. you will play with the black pieces in this tutorial.", waitCondition: 'click' },
    { text: "Like in chess, white always goes first.", waitCondition: 'click' },
    { 
      text: "pieces move by sliding in one cardinal direction untill they hit a wall or another piece.", 
      action: 'bot_move', 
      moveData: { from: { r: 0, c: 2 }, to: { r: 4, c: 2 } }, // (2,0) down to (2,4)
      highlight: [{ r: 4, c: 2 }], 
      waitCondition: 'click' 
    },
    { text: "the aim of the game is to get 4 pieces in a row, like connect 4.", waitCondition: 'click' },
    { 
      text: "currently, on my next turn, i can move this piece here and win", 
      highlight: [{ r: 3, c: 5 }, { r: 3, c: 1 }], // (5,3) to (1,3)
      waitCondition: 'click' 
    },
    { 
      text: "it is currently your go, to stop me, you can move this piece here because i cannot jump over it.", 
      highlight: [{ r: 3, c: 0 }, { r: 3, c: 4 }], // (0,3) to (4,3)
      waitCondition: 'click' 
    },
    { 
      text: "to make a move, first click on this piece", 
      highlight: [{ r: 3, c: 0 }], // (0,3)
      action: 'user_move', 
      moveData: { from: { r: 3, c: 0 }, to: { r: 3, c: 4 } },
      waitCondition: 'move' 
    },
    { 
      text: "then click the square you would like to move it to.", 
      highlight: [{ r: 3, c: 4 }], // (4,3)
      action: 'user_move', 
      moveData: { from: { r: 3, c: 0 }, to: { r: 3, c: 4 } },
      waitCondition: 'move' 
    },
    { 
      text: "now i will make my move.", 
      action: 'bot_move', 
      moveData: { from: { r: 4, c: 2 }, to: { r: 4, c: 0 } }, // (2,4) left to (0,4)
      waitCondition: 'click' 
    },
    { 
      text: "try to see if there is a winning move for you here.", 
      action: 'user_win',
      highlight: [{ r: 0, c: 3 }], // (3,0)
      waitCondition: 'move' 
    },
    { 
      text: "contratulations on winning your first game! now go get to the top of the leaderboard!", 
      waitCondition: 'click' 
    },
  ], []);

  const currentStep = steps[stepIndex];

  useEffect(() => {
    setMessage(currentStep.text);
  }, [stepIndex, currentStep]);

  const handleNext = useCallback(() => {
    if (stepIndex < steps.length - 1) {
      if (currentStep.action === 'bot_move' && currentStep.moveData) {
        setIsBotMoving(true);
        setTimeout(() => {
          const newBoard = board.map(row => [...row]);
          const { from, to } = currentStep.moveData!;
          const piece = newBoard[from.r][from.c];
          newBoard[from.r][from.c] = '0';
          newBoard[to.r][to.c] = piece;
          setBoard(newBoard);
          setIsBotMoving(false);
          setStepIndex(stepIndex + 1);
        }, 600);
      } else {
        setStepIndex(stepIndex + 1);
      }
    } else {
      onComplete();
    }
  }, [stepIndex, steps, currentStep, board, onComplete]);

  const handleSquareClick = (r: number, c: number) => {
    if (isBotMoving) return;
    if (currentStep.waitCondition !== 'move') return;

    if (currentStep.action === 'user_move') {
      const { from, to } = currentStep.moveData!;
      if (selected) {
        if (r === to.r && c === to.c) {
          const newBoard = board.map(row => [...row]);
          const piece = newBoard[selected.r][selected.c];
          newBoard[selected.r][selected.c] = '0';
          newBoard[r][c] = piece;
          setBoard(newBoard);
          setSelected(null);
          setValidMoves([]);
          setStepIndex(stepIndex + 1);
        } else {
          setSelected(null);
          setValidMoves([]);
          if (stepIndex === 9) setStepIndex(8);
        }
      } else if (r === from.r && c === from.c) {
        setSelected({ r, c });
        setValidMoves([{ r: to.r, c: to.c }]);
        if (stepIndex === 8) setStepIndex(9);
      }
    } else if (currentStep.action === 'user_win') {
      if (selected) {
        const moves = getValidMoves(board, selected.r, selected.c);
        const isMove = moves.some(m => m.r === r && m.c === c);
        if (isMove) {
          const newBoard = board.map(row => [...row]);
          const piece = newBoard[selected.r][selected.c];
          newBoard[selected.r][selected.c] = '0';
          newBoard[r][c] = piece;
          
          const winResult = checkWin(newBoard);
          if (winResult && winResult.winner === 'B') {
            setBoard(newBoard);
            setWinningLine(winResult.line);
            setStepIndex(stepIndex + 1);
          } else {
            setMessage("there was a better move you could have made here");
            // Highlight what they should have done
            setBoard(board); // Revert move
          }
          setSelected(null);
          setValidMoves([]);
        } else {
          setSelected(null);
          setValidMoves([]);
        }
      } else {
        const piece = board[r][c];
        if (piece === 'B') {
          setSelected({ r, c });
          setValidMoves(getValidMoves(board, r, c));
        }
      }
    }
  };

  return (
    <div className="min-h-screen bg-[var(--bg)] text-[var(--text)] flex flex-col items-center justify-center p-4 font-sans transition-colors duration-500 overflow-hidden">
      <div className="w-full max-w-2xl flex flex-col items-center mb-8">
        <div className="bg-[var(--primary)] bg-opacity-10 p-6 rounded-3xl border-b-4 border-[var(--accent)] w-full mb-8 relative">
          <div className="flex items-center gap-4 mb-2">
            <div className="w-10 h-10 bg-[var(--primary)] rounded-full flex items-center justify-center text-[var(--primaryText)] font-bold">
              Bot
            </div>
            <span className="font-bold text-[var(--primary)]">Tutorial Bot</span>
          </div>
          <p className="text-xl font-bold leading-tight min-h-[3rem]">
            {message}
          </p>
          {currentStep.waitCondition === 'click' && !isBotMoving && (
            <button 
              onClick={handleNext}
              className="absolute bottom-4 right-4 flex items-center gap-2 bg-[var(--primary)] text-[var(--primaryText)] px-4 py-2 rounded-xl font-bold hover:scale-105 transition-all shadow-lg"
            >
              Next <ChevronRight className="w-4 h-4" />
            </button>
          )}
          {stepIndex === steps.length - 1 && (
             <button 
                onClick={onComplete}
                className="absolute bottom-4 right-4 flex items-center gap-2 bg-green-600 text-white px-6 py-2 rounded-xl font-bold hover:scale-105 transition-all shadow-lg"
             >
                Finish <Trophy className="w-4 h-4" />
             </button>
          )}
        </div>

        <div className="relative bg-[var(--boardBorder)] p-4 rounded-3xl shadow-2xl border-b-8 border-[var(--secondary)]">
          <div className="grid grid-cols-6 gap-2 bg-[var(--secondary)] p-2 rounded-xl border-4 border-[var(--secondary)]">
            {board.map((row, r) => (
              row.map((piece, c) => {
                const isSelected = selected?.r === r && selected?.c === c;
                const isValid = validMoves.some(m => m.r === r && m.c === c);
                const isHighlighted = currentStep.highlight?.some(h => h.r === r && h.c === c);
                const isWinningPiece = winningLine?.some(l => l.r === r && l.c === c);

                return (
                  <div 
                    key={`${r}-${c}`}
                    onClick={() => handleSquareClick(r, c)}
                    className={`
                      w-12 h-12 sm:w-16 sm:h-16 rounded-lg flex items-center justify-center cursor-pointer transition-all relative
                      ${(r + c) % 2 === 0 ? 'bg-[var(--boardLight)]' : 'bg-[var(--boardDark)]'}
                      ${isValid ? 'ring-4 ring-green-400 ring-inset bg-green-100/50' : ''}
                      ${isSelected ? 'ring-4 ring-[var(--primary)] ring-inset' : ''}
                      ${isHighlighted ? 'ring-4 ring-orange-500 ring-inset animate-pulse' : ''}
                      ${isWinningPiece ? 'ring-4 ring-yellow-400 ring-inset bg-yellow-100/30' : ''}
                    `}
                  >
                    <AnimatePresence mode="popLayout">
                      {piece !== '0' && (
                        <PieceComponent
                          type={piece as 'W' | 'B'}
                          skin="classic"
                          isWinningPiece={isWinningPiece}
                        />
                      )}
                    </AnimatePresence>
                  </div>
                );
              })
            ))}
          </div>
        </div>
      </div>
    </div>
  );
};
