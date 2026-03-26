import React from 'react';
import { motion } from 'motion/react';
import { Skin } from '../types/game';

interface PieceProps {
  type: 'W' | 'B';
  skin: Skin;
  username?: string;
  isWinningPiece?: boolean;
  winningIndex?: number;
}

export const Piece: React.FC<PieceProps> = ({ type, skin, username, isWinningPiece, winningIndex = 0 }) => {
  const isBigLarge = username === 'BIG LARGE';
  const scaleMultiplier = isBigLarge ? 1.5 : 1;

  const renderSkin = () => {
    if (skin === 'classic') {
      return (
        <div className={`
          w-10 h-10 sm:w-12 sm:h-12 rounded-full border-b-4 relative
          ${type === 'W' ? 'bg-white border-gray-300' : 'bg-[#2a2a2a] border-black'}
          ${isWinningPiece ? 'ring-4 ring-orange-500 ring-offset-2' : ''}
          shadow-lg
        `} />
      );
    }

    const color = type === 'W' ? 'white' : 'black';
    const svgUrl = `/api/skins/${skin}/${color}.svg`;
    
    return (
      <div className={`
        w-10 h-10 sm:w-12 sm:h-12 rounded-full relative overflow-hidden flex items-center justify-center
        ${isWinningPiece ? 'ring-4 ring-orange-500 ring-offset-2' : ''}
        shadow-lg
      `}>
        <img 
          src={svgUrl} 
          alt={`${type} ${skin}`} 
          className="w-full h-full object-contain"
          onError={(e) => {
            (e.target as HTMLImageElement).style.display = 'none';
          }}
        />
        <div className={`
          absolute inset-0 rounded-full border-b-4 -z-10
          ${type === 'W' ? 'bg-white border-gray-300' : 'bg-[#2a2a2a] border-black'}
        `} />
      </div>
    );
  };

  return (
    <motion.div
      initial={{ scale: 0 }}
      animate={{ 
        scale: scaleMultiplier,
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
      className="relative z-10"
    >
      {renderSkin()}
    </motion.div>
  );
};
