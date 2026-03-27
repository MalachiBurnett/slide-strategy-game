import React from 'react';
import { motion } from 'motion/react';
import { Users } from 'lucide-react';

interface QueueViewProps {
  timeControl: string;
  variant: string;
  queueCounts: Record<string, Record<string, number>>;
  leaveQueue: () => void;
}

export const QueueView: React.FC<QueueViewProps> = ({
  timeControl,
  variant,
  queueCounts,
  leaveQueue,
}) => {
  const variants = ['classic', 'fog_of_war', 'random_setup', 'schizophrenic'];
  const timeControls = ['0.25|3', '1|0', '3|2'];

  const formatTC = (tc: string) => tc === '0.25|3' ? '15s|3s' : tc === '3|2' ? '3|2' : '1 min';
  const formatVariant = (v: string) => v.split('_').map(word => word.charAt(0).toUpperCase() + word.slice(1)).join(' ');

  return (
    <div className="min-h-screen bg-[var(--bg)] p-4 sm:p-8 font-sans text-[var(--text)] flex items-center justify-center transition-colors duration-500">
      <div className="max-w-4xl w-full">
        <motion.div 
          initial={{ opacity: 0, y: 20 }}
          animate={{ opacity: 1, y: 0 }}
          className="bg-[var(--bgLight)] p-8 rounded-3xl shadow-xl border-b-8 border-[var(--accent)] border-opacity-50 text-center"
        >
          <div className="flex justify-center mb-6">
            <div className="bg-[var(--primary)] p-4 rounded-full animate-pulse">
              <Users className="w-12 h-12 text-[var(--bg)]" />
            </div>
          </div>
          <h2 className="text-3xl font-bold mb-2">Searching for Opponent</h2>
          <p className="opacity-60 mb-8">Queueing for <span className="font-bold text-[var(--primary)]">{formatVariant(variant)}</span> at <span className="font-bold text-[var(--primary)]">{formatTC(timeControl)}</span></p>
          
          <div className="overflow-x-auto mb-8">
            <table className="w-full border-collapse">
              <thead>
                <tr>
                  <th className="p-3 text-left opacity-40 text-xs font-bold uppercase">Variant \ Time</th>
                  {timeControls.map(tc => (
                    <th key={tc} className="p-3 text-center opacity-40 text-xs font-bold uppercase">{formatTC(tc)}</th>
                  ))}
                </tr>
              </thead>
              <tbody>
                {variants.map(v => (
                  <tr key={v} className="border-t border-[var(--primary)] border-opacity-10">
                    <td className="p-3 text-left font-bold text-sm">{formatVariant(v)}</td>
                    {timeControls.map(tc => (
                      <td key={tc} className="p-3 text-center">
                        <div className={`inline-block px-3 py-1 rounded-full text-xs font-bold ${variant === v && timeControl === tc ? 'bg-[var(--primary)] text-[var(--primaryText)]' : 'bg-[var(--primary)] bg-opacity-5 text-[var(--primary)]'}`}>
                          {queueCounts[v]?.[tc] || 0}
                        </div>
                      </td>
                    ))}
                  </tr>
                ))}
              </tbody>
            </table>
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
