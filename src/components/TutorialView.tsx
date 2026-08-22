import React, { useState, useMemo, useCallback, useEffect } from 'react';
import { motion } from 'motion/react';
import { Trophy, ChevronRight } from 'lucide-react';
import { Board, Piece } from '../types/game';
import { Piece as PieceComponent } from './Piece';
import { getValidMoves, checkWin } from '../utils/gameLogic';
import { INITIAL_BOARD } from '../constants/game';

interface TutorialViewProps {
  onComplete: () => void;
}

interface TutorialPiece {
  id: number;
  type: 'W' | 'B';
  r: number;
  c: number;
}

interface Step {
  text: string;
  action?: 'bot_move' | 'user_move' | 'user_win';
  highlight?: { r: number; c: number }[];
  moveData?: { from: { r: number; c: number }; to: { r: number; c: number } };
  waitCondition: 'click' | 'move';
}

// How long the slide animation takes to settle, plus a short pause so the
// player has a moment to register the result before the next line appears.
// Text only advances once both have elapsed, so the caption on screen always
// matches what the board is doing.
const ANIMATION_MS = 550;
const SETTLE_MS = 450;

const buildInitialPieces = (): TutorialPiece[] => {
  const pieces: TutorialPiece[] = [];
  let id = 0;
  INITIAL_BOARD.forEach((row, r) => {
    row.forEach((cell, c) => {
      if (cell !== '0') pieces.push({ id: id++, type: cell as 'W' | 'B', r, c });
    });
  });
  return pieces;
};

const STEPS: Step[] = [
  { text: "Hi! I'm the Tutorial Bot, and I'm here to teach you how to play Slide.", waitCondition: 'click' },
  { text: "This is the board — 6 squares wide and 6 squares tall. Everything happens here.", waitCondition: 'click' },
  { text: "Each side starts with 6 pieces. You'll play the black pieces in this tutorial, and I'll play white.", waitCondition: 'click' },
  { text: "Just like in chess, white always moves first, then we take turns.", waitCondition: 'click' },
  {
    text: "Pieces slide in a straight line — up, down, left or right — until they hit the edge or another piece. Watch this one go.",
    action: 'bot_move',
    moveData: { from: { r: 0, c: 2 }, to: { r: 4, c: 2 } },
    highlight: [{ r: 4, c: 2 }],
    waitCondition: 'click',
  },
  { text: "The goal is to get 4 of your pieces in a row — like Connect 4 — horizontally, vertically or diagonally.", waitCondition: 'click' },
  {
    text: "See these two squares? If I get another turn, I can slide this piece from here to there and win instantly.",
    highlight: [{ r: 3, c: 5 }, { r: 3, c: 1 }],
    waitCondition: 'click',
  },
  {
    text: "It's your turn — click this piece to select it and stop me.",
    highlight: [{ r: 3, c: 0 }],
    action: 'user_move',
    moveData: { from: { r: 3, c: 0 }, to: { r: 3, c: 4 } },
    waitCondition: 'move',
  },
  {
    text: "Now slide it here, right next to my piece, so I can't jump over it.",
    highlight: [{ r: 3, c: 4 }],
    action: 'user_move',
    moveData: { from: { r: 3, c: 0 }, to: { r: 3, c: 4 } },
    waitCondition: 'move',
  },
  {
    text: "Nice block! Now watch — I'll make my move.",
    action: 'bot_move',
    moveData: { from: { r: 4, c: 2 }, to: { r: 4, c: 0 } },
    waitCondition: 'click',
  },
  {
    text: "Your turn again. Take a look — is there a move that wins the game for you?",
    action: 'user_win',
    highlight: [{ r: 0, c: 3 }],
    waitCondition: 'move',
  },
  {
    text: "That's four in a row — you win! You're ready to play for real. Good luck out there!",
    waitCondition: 'click',
  },
];

export const TutorialView: React.FC<TutorialViewProps> = ({ onComplete }) => {
  const [stepIndex, setStepIndex] = useState(0);
  const [pieces, setPieces] = useState<TutorialPiece[]>(buildInitialPieces);
  const [selected, setSelected] = useState<{ r: number; c: number } | null>(null);
  const [validMoves, setValidMoves] = useState<{ r: number; c: number }[]>([]);
  const [message, setMessage] = useState(STEPS[0].text);
  const [isBotMoving, setIsBotMoving] = useState(false);
  const [winningLine, setWinningLine] = useState<{ r: number; c: number }[] | null>(null);

  const currentStep = STEPS[stepIndex];

  // The board is derived from piece positions rather than the other way
  // round, so every piece keeps a stable identity (and can animate a real
  // slide) as it moves from cell to cell.
  const board: Board = useMemo(() => {
    const b: Board = Array.from({ length: 6 }, () => Array(6).fill('0') as Piece[]);
    pieces.forEach(p => { b[p.r][p.c] = p.type; });
    return b;
  }, [pieces]);

  const movePiece = useCallback((from: { r: number; c: number }, to: { r: number; c: number }) => {
    setPieces(prev => prev.map(p => (p.r === from.r && p.c === from.c) ? { ...p, r: to.r, c: to.c } : p));
  }, []);

  useEffect(() => {
    setMessage(currentStep.text);
  }, [stepIndex, currentStep]);

  const turnLabel = currentStep.action === 'bot_move' ? "Bot's turn"
    : (currentStep.action === 'user_move' || currentStep.action === 'user_win') ? 'Your turn'
    : null;

  const handleNext = useCallback(() => {
    if (stepIndex >= STEPS.length - 1) {
      onComplete();
      return;
    }
    const step = STEPS[stepIndex];
    if (step.action === 'bot_move' && step.moveData) {
      // Move the piece straight away so the slide plays out while this
      // step's caption is still on screen — the text only changes once the
      // motion has actually finished.
      setIsBotMoving(true);
      movePiece(step.moveData.from, step.moveData.to);
      setTimeout(() => {
        setIsBotMoving(false);
        setStepIndex(i => i + 1);
      }, ANIMATION_MS + SETTLE_MS);
    } else {
      setStepIndex(i => i + 1);
    }
  }, [stepIndex, movePiece, onComplete]);

  const handleSquareClick = (r: number, c: number) => {
    if (isBotMoving || currentStep.waitCondition !== 'move') return;

    if (currentStep.action === 'user_move' && currentStep.moveData) {
      const { from, to } = currentStep.moveData;
      if (selected) {
        if (r === to.r && c === to.c) {
          movePiece(selected, to);
          setSelected(null);
          setValidMoves([]);
          setStepIndex(i => i + 1);
        } else {
          // Wrong square: deselect and drop back to the "select the piece"
          // line rather than leaving the caption stuck on a step whose
          // instructions no longer match what's on screen.
          setSelected(null);
          setValidMoves([]);
          setStepIndex(i => i - 1);
        }
      } else if (r === from.r && c === from.c) {
        setSelected({ r, c });
        setValidMoves([to]);
        setStepIndex(i => i + 1);
      }
    } else if (currentStep.action === 'user_win') {
      if (selected) {
        const moves = getValidMoves(board, selected.r, selected.c);
        const isMove = moves.some(m => m.r === r && m.c === c);
        if (isMove) {
          const newBoard = board.map(row => [...row]);
          newBoard[selected.r][selected.c] = '0';
          newBoard[r][c] = 'B';
          const winResult = checkWin(newBoard);
          if (winResult && winResult.winner === 'B') {
            movePiece(selected, { r, c });
            setWinningLine(winResult.line);
            setSelected(null);
            setValidMoves([]);
            setStepIndex(i => i + 1);
          } else {
            setMessage("That doesn't win the game — try a different move!");
            setSelected(null);
            setValidMoves([]);
          }
        } else {
          setSelected(null);
          setValidMoves([]);
        }
      } else if (board[r][c] === 'B') {
        setSelected({ r, c });
        setValidMoves(getValidMoves(board, r, c));
      }
    }
  };

  return (
    <div className="min-h-screen bg-[var(--bg)] text-[var(--text)] flex flex-col items-center justify-center p-4 font-sans transition-colors duration-500 overflow-hidden">
      <div className="w-full max-w-2xl flex flex-col items-center mb-8">
        <div className="bg-[var(--primary)] bg-opacity-10 p-6 rounded-3xl border-b-4 border-[var(--accent)] w-full mb-8 relative">
          <div className="flex items-center gap-4 mb-2 flex-wrap">
            <div className="w-10 h-10 bg-[var(--primary)] rounded-full flex items-center justify-center text-[var(--primaryText)] font-bold">
              Bot
            </div>
            <span className="font-bold text-[var(--primary)]">Tutorial Bot</span>
            {turnLabel && (
              <span className="text-xs font-black uppercase tracking-wide px-2 py-1 rounded-full bg-[var(--primary)] text-[var(--primaryText)] opacity-80">
                {turnLabel}
              </span>
            )}
          </div>
          <p className="text-xl font-bold leading-tight min-h-[3rem] text-[var(--primaryText)] pr-28">
            {message}
          </p>
          {currentStep.waitCondition === 'click' && !isBotMoving && stepIndex < STEPS.length - 1 && (
            <button
              onClick={handleNext}
              className="absolute bottom-4 right-4 flex items-center gap-2 bg-[var(--primary)] text-[var(--primaryText)] px-4 py-2 rounded-xl font-bold hover:scale-105 transition-all shadow-lg"
            >
              Next <ChevronRight className="w-4 h-4" />
            </button>
          )}
          {stepIndex === STEPS.length - 1 && (
             <button
                onClick={onComplete}
                className="absolute bottom-4 right-4 flex items-center gap-2 bg-green-600 text-white px-6 py-2 rounded-xl font-bold hover:scale-105 transition-all shadow-lg"
             >
                Finish <Trophy className="w-4 h-4" />
             </button>
          )}
        </div>

        <div className="relative bg-[var(--boardBorder)] p-4 rounded-3xl shadow-2xl border-b-8 border-[var(--secondary)]">
          <div className="relative bg-[var(--secondary)] rounded-xl border-4 border-[var(--secondary)] w-[336px] h-[336px] max-w-[80vw] max-h-[80vw]">
            {/* Background squares handle clicks and highlight rings. */}
            <div className="absolute inset-1 grid grid-cols-6 grid-rows-6">
              {board.map((row, r) => row.map((_, c) => {
                const isSelected = selected?.r === r && selected?.c === c;
                const isValid = validMoves.some(m => m.r === r && m.c === c);
                const isHighlightedRaw = currentStep.highlight?.some(h => h.r === r && h.c === c);
                const isHighlighted = isHighlightedRaw && !isValid && !isSelected;

                return (
                  <div key={`${r}-${c}`} onClick={() => handleSquareClick(r, c)} className="relative cursor-pointer">
                    <div
                      className={`
                        absolute inset-[3px] rounded-lg transition-all
                        ${(r + c) % 2 === 0 ? 'bg-[var(--boardLight)]' : 'bg-[var(--boardDark)]'}
                        ${isValid ? 'ring-4 ring-green-400 ring-inset bg-green-100/50' : ''}
                        ${isSelected ? 'ring-4 ring-[var(--primary)] ring-inset' : ''}
                        ${isHighlighted ? 'ring-4 ring-orange-500 ring-inset animate-pulse' : ''}
                      `}
                    />
                  </div>
                );
              }))}
            </div>

            {/* Pieces are a flat, stably-keyed overlay so a move is a real
                slide across the board rather than a pop in a new cell. */}
            <div className="absolute inset-1 pointer-events-none">
              {pieces.map(p => {
                const isWinningPiece = winningLine?.some(l => l.r === p.r && l.c === p.c) ?? false;
                return (
                  <motion.div
                    key={p.id}
                    layout
                    transition={{ type: 'spring', stiffness: 120, damping: 18 }}
                    className="absolute flex items-center justify-center"
                    style={{
                      width: `${100 / 6}%`,
                      height: `${100 / 6}%`,
                      left: `${(p.c * 100) / 6}%`,
                      top: `${(p.r * 100) / 6}%`,
                    }}
                  >
                    <PieceComponent type={p.type} skin="classic" isWinningPiece={isWinningPiece} />
                  </motion.div>
                );
              })}
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};
