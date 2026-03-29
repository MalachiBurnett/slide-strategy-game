import React from 'react';
import { motion } from 'motion/react';
import { EMOTES } from '../constants/emotes';

interface EmoteDisplayProps {
  emoteId: string;
  onComplete: () => void;
}

export const EmoteDisplay: React.FC<EmoteDisplayProps> = ({ emoteId, onComplete }) => {
  const emote = EMOTES.find(e => e.id === emoteId);

  if (!emote) return null;

  return (
    <motion.div
      initial={{ opacity: 0, scale: 0.2 }}
      animate={{ 
        opacity: [0, 1, 1, 0], 
        scale: [0.2, 1.2, 1.0, 8.0] 
      }}
      transition={{ 
        duration: 3,
        times: [0, 0.1, 0.2, 1],
        ease: "easeOut"
      }}
      onAnimationComplete={onComplete}
      className={`text-6xl font-bold pointer-events-none select-none ${emote.isText ? 'bg-gradient-to-r from-[var(--primary)] to-[var(--accent)] bg-clip-text text-transparent' : ''}`}
    >
      {emote.emoji}
    </motion.div>
  );
};
