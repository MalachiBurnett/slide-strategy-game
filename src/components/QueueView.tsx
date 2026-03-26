import React from 'react';
import { motion } from 'motion/react';
import { Users } from 'lucide-react';

interface QueueViewProps {
  timeControl: string;
  queueCounts: Record<string, number>;
  leaveQueue: () => void;
}

export const QueueView: React.FC<QueueViewProps> = ({
  timeControl,
  queueCounts,
  leaveQueue,
}) => {
  return (
    <div className="min-h-screen bg-[var(--bg)] p-4 sm:p-8 font-sans text-[var(--text)] flex items-center justify-center transition-colors duration-500">
      <div className="max-w-2xl w-full">
        <motion.div 
          initial={{ opacity: 0, y: 20 }}
          animate={{ opacity: 1, y: 0 }}
          className="bg-[var(--bg)] p-8 rounded-3xl shadow-xl border-b-8 border-[var(--accent)] border-opacity-50 text-center"
        >
          <div className="flex justify-center mb-6">
            <div className="bg-[var(--primary)] p-4 rounded-full animate-pulse">
              <Users className="w-12 h-12 text-[var(--bg)]" />
            </div>
          </div>
          <h2 className="text-3xl font-bold mb-2">Searching for Opponent</h2>
          <p className="opacity-60 mb-6">Queueing for <span className="font-bold text-[var(--primary)]">{timeControl === '10|0' ? '10 min' : timeControl === '3|2' ? '3|2' : '1 min'}</span></p>
          
          <div className="flex flex-wrap justify-center gap-4 mb-8">
            {(['10|0', '3|2', '1|0'] as const).map((tc) => (
              <div key={tc} className="bg-[var(--primary)] bg-opacity-5 px-4 py-2 rounded-xl border border-[var(--primary)] border-opacity-10">
                <span className="text-xs opacity-40 font-bold uppercase block">{tc === '10|0' ? '10 min' : tc === '3|2' ? '3|2' : '1 min'}</span>
                <span className="text-lg font-bold text-[var(--primary)]">{queueCounts[tc] || 0} in queue</span>
              </div>
            ))}
          </div>

          <button 
            onClick={leaveQueue}
            className="px-8 py-3 bg-[var(--bg)] border-2 border-red-500 text-red-500 rounded-xl font-bold hover:bg-red-500 hover:text-white transition-all"
          >
            Leave Queue
          </button>
        </motion.div>
      </div>
    </div>
  );
};
