import React, { useEffect } from 'react';
import { motion } from 'motion/react';
import { EMOTES } from '../constants/emotes';

interface EmoteDisplayProps {
  emoteId: string;
  onComplete: () => void;
}

export const EmoteDisplay: React.FC<EmoteDisplayProps> = ({ emoteId, onComplete }) => {
  const emote = EMOTES.find(e => e.id === emoteId);

  useEffect(() => {
    const timer = setTimeout(onComplete, 2000);
    return () => clearTimeout(timer);
  }, [onComplete]);

  if (!emote) return null;

  return (
    <motion.div
      initial={{ opacity: 0, y: -50, scale: 0.5 }}
      animate={{ opacity: 1, y: 0, scale: 1 }}
      exit={{ opacity: 0, y: 50, scale: 0.5 }}
      transition={{
        opacity: { duration: 2 },
        y: { duration: 0.4 },
        scale: { duration: 0.4 }
      }}
      className={`text-6xl font-bold pointer-events-none ${emote.isText ? 'bg-gradient-to-r from-[var(--primary)] to-[var(--accent)] bg-clip-text text-transparent' : ''}`}
    >
      {emote.emoji}
    </motion.div>
  );
};
