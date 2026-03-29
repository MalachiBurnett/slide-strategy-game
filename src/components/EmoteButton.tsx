import React, { useState } from 'react';
import { motion, AnimatePresence } from 'motion/react';
import { EMOTES, EmoteId } from '../constants/emotes';

interface EmoteButtonProps {
  onEmoteSelect: (emoteId: EmoteId) => void;
  disabled?: boolean;
}

export const EmoteButton: React.FC<EmoteButtonProps> = ({ onEmoteSelect, disabled = false }) => {
  const [isOpen, setIsOpen] = useState(false);

  const handleEmoteClick = (emoteId: EmoteId) => {
    onEmoteSelect(emoteId);
    setIsOpen(false);
  };

  return (
    <div className="relative">
      <motion.button
        onClick={() => setIsOpen(!isOpen)}
        disabled={disabled}
        whileHover={{ scale: 1.05 }}
        whileTap={{ scale: 0.95 }}
        className="px-6 py-3 bg-[var(--primary)] text-[var(--bg)] rounded-xl font-bold hover:opacity-90 transition-all disabled:opacity-50 disabled:cursor-not-allowed text-lg"
      >
        😊 Emotes
      </motion.button>

      <AnimatePresence>
        {isOpen && (
          <motion.div
            initial={{ opacity: 0, y: -10, scale: 0.95 }}
            animate={{ opacity: 1, y: 0, scale: 1 }}
            exit={{ opacity: 0, y: -10, scale: 0.95 }}
            transition={{ duration: 0.2 }}
            className="absolute bottom-full right-0 mb-2 bg-[var(--bgLight)] border-2 border-[var(--primary)] rounded-2xl p-3 shadow-2xl z-50 grid grid-cols-5 gap-2"
          >
            {EMOTES.map((emote) => (
              <motion.button
                key={emote.id}
                onClick={() => handleEmoteClick(emote.id as EmoteId)}
                whileHover={{ scale: 1.15 }}
                whileTap={{ scale: 0.9 }}
                className="text-3xl p-2 rounded-lg hover:bg-[var(--primary)] hover:bg-opacity-10 transition-colors"
                title={emote.label}
              >
                {emote.emoji}
              </motion.button>
            ))}
          </motion.div>
        )}
      </AnimatePresence>
    </div>
  );
};
