import React, { useState } from 'react';
import { motion, AnimatePresence } from 'motion/react';
import { Play, Trophy, LogOut, Hash, Users, ClipboardList, Zap, User, Heart, Eye } from 'lucide-react';
import { UserData, LeaderboardEntry, Turn } from '../types/game';
import { Socket } from 'socket.io-client';

interface LobbyViewProps {
  user: UserData | null;
  onlineCount: number;
  timeControl: '0.25|3' | '1|0' | '3|2';
  setTimeControl: (tc: '0.25|3' | '1|0' | '3|2') => void;
  variant: string;
  setVariant: (v: string) => void;
  queueCounts: Record<string, Record<string, number>>;
  showLeaderboard: boolean;
  setShowLeaderboard: (show: boolean) => void;
  leaderboard: LeaderboardEntry[];
  fetchLeaderboard: () => void;
  showTutorial: boolean;
  setShowTutorial: (show: boolean) => void;
  setUser: (user: UserData | null) => void;
  setView: (view: 'auth' | 'lobby' | 'game' | 'queue' | 'cosmetics' | 'profile' | 'credits' | 'spectate') => void;
  handleLogout: () => void;
  startPublicMatch: () => void;
  startLocalMatch: () => void;
  createPrivateMatch: () => void;
  joinPrivateMatch: () => void;
  joinCode: string;
  setJoinCode: (code: string) => void;
  privateCode: string | null;
  setPrivateCode: (code: string | null) => void;
  setGameId: (id: string | null) => void;
  setPlayerColor: (color: Turn | null) => void;
  error: string;
  isRated: boolean;
  toggleRated: () => void;
  socket: Socket;
}

export const LobbyView: React.FC<LobbyViewProps> = ({
  user,
  onlineCount,
  timeControl,
  setTimeControl,
  variant,
  setVariant,
  queueCounts,
  showLeaderboard,
  setShowLeaderboard,
  leaderboard,
  fetchLeaderboard,
  showTutorial,
  setShowTutorial,
  setUser,
  setView,
  handleLogout,
  startPublicMatch,
  startLocalMatch,
  createPrivateMatch,
  joinPrivateMatch,
  joinCode,
  setJoinCode,
  privateCode,
  setPrivateCode,
  setGameId,
  setPlayerColor,
  error,
  isRated,
  toggleRated,
  socket,
}) => {
  const [showPatchNotes, setShowPatchNotes] = useState(false);
  const [playersInGame, setPlayersInGame] = useState<Set<string>>(new Set());

  React.useEffect(() => {
    if (showLeaderboard) {
      // Check each player on the leaderboard to see if they're in a game
      const newPlayersInGame = new Set<string>();
      leaderboard.forEach(entry => {
        socket.emit('get_player_status', { username: entry.username }, (response: { inGame: boolean, gameId?: string }) => {
          if (response.inGame) {
            newPlayersInGame.add(entry.username);
          }
          setPlayersInGame(new Set(newPlayersInGame));
        });
      });
    }
  }, [showLeaderboard, leaderboard, socket]);
  
  const variants = [
    { id: 'classic', name: 'Classic', desc: 'The original rules.' },
    { id: 'fog_of_war', name: 'Fog of War', desc: 'Only see squares you can move to.' },
    { id: 'random_setup', name: 'Random Setup', desc: 'Pieces start in random positions.' },
    { id: 'schizophrenic', name: 'Schizophrenic', desc: 'A random square changes every turn!' },
  ];

  const getQueueCountForVariant = (vId: string): number => {
    if (!queueCounts || !queueCounts[vId]) return 0;
    return (Object.values(queueCounts[vId]) as number[]).reduce((a, b) => a + b, 0);
  };

  const VariantSelector = () => (
    <div className="flex flex-wrap gap-2 mb-6">
      {variants.map((v) => {
        const count = getQueueCountForVariant(v.id);
        return (
          <button
            key={v.id}
            onClick={() => setVariant(v.id)}
            className={`px-4 py-2 rounded-xl text-xs font-black transition-all border-2 flex items-center gap-2 ${variant === v.id ? 'bg-[var(--primary)] text-[var(--primaryText)] shadow-lg' : 'bg-[var(--bg)] text-[var(--text)] border-[var(--primary)] border-opacity-30 hover:border-opacity-100'}`}
            title={v.desc}
          >
            {v.name}
            {count > 0 && (
              <span className="bg-red-500 text-white px-1.5 py-0.5 rounded-full text-[10px]">
                {count}
              </span>
            )}
            {count === 0 && (
               <span className="opacity-20 text-[10px]">0</span>
            )}
          </button>
        );
      })}
    </div>
  );

  return (
    <div className="min-h-screen bg-[var(--bg)] p-4 sm:p-8 font-sans text-[var(--text)] transition-colors duration-500">
      <div className="max-w-4xl mx-auto">
        <header className="flex flex-col sm:flex-row justify-between items-center gap-6 mb-8 sm:mb-12">
          <div className="text-center sm:text-left">
            <h1 className="text-4xl font-black tracking-tight text-[var(--primary)]">SLIDE</h1>
            <div className="flex items-center justify-center sm:justify-start gap-2 mt-2">
              <div className="w-2 h-2 bg-green-500 rounded-full animate-pulse"></div>
              <span className="text-sm opacity-80 font-bold">{onlineCount} Online</span>
            </div>
          </div>
          <div className="flex flex-col items-center gap-4">
            <div className="bg-[var(--primary)] bg-opacity-10 p-1 text-[var(--primaryText)] rounded-xl shadow-md border-b-2 border-[var(--primary)] border-opacity-20 flex gap-1">
              {(['0.25|3', '3|2', '1|0'] as const).map((tc) => (
                <button
                  key={tc}
                  onClick={() => setTimeControl(tc)}
                  className={`px-4 py-2 rounded-lg text-sm font-black transition-all ${timeControl === tc ? 'bg-[var(--primary)] text-[var(--primaryText)] shadow-lg' : 'text-[var(--primaryText)] opacity-60 hover:opacity-100 hover:text-[var(--primaryText)]'}`}
                >
                  {tc === '0.25|3' ? '15s|3s' : tc === '3|2' ? '3|2' : '1 min'}
                </button>
              ))}
            </div>
          </div>
          <div className="flex items-center gap-4 sm:gap-6">
            <button 
              onClick={() => { fetchLeaderboard(); setShowLeaderboard(true); }}
              className="p-2 sm:p-3 bg-[var(--bgLight)] rounded-full shadow-md hover:shadow-lg transition-all text-[var(--primary)] border-2 border-[var(--primary)] border-opacity-20"
              title="Leaderboard"
            >
              <Trophy className="w-5 h-5 sm:w-6 h-6" />
            </button>
            <button
              onClick={() => setView('cosmetics')}
              className="px-6 py-3 bg-[var(--primary)] text-[var(--primaryText)] rounded-xl shadow-lg hover:scale-105 transition-all font-black"
            >
              Skins
            </button>
            <button 
              onClick={() => setView('credits')}
              className="p-2 sm:p-3 bg-[var(--bgLight)] rounded-full shadow-md hover:shadow-lg transition-all text-[var(--primary)] border-2 border-[var(--primary)] border-opacity-20"
              title="Credits"
            >
              <Heart className="w-5 h-5 sm:w-6 h-6" />
            </button>
            <div className="text-right flex flex-col items-end">
              <button 
                onClick={() => setView('profile')}
                className="font-black text-lg sm:text-xl hover:text-[var(--primary)] transition-colors flex items-center gap-2"
              >
                {user?.username}
                <User className="w-5 h-5" />
              </button>
              <p className="text-xs sm:text-sm text-[var(--primary)] font-black">ELO: {user?.elo}</p>
            </div>
            <button
              onClick={handleLogout}
              className="p-2 sm:p-3 bg-[var(--bgLight)] rounded-full shadow-md hover:shadow-lg transition-all text-gray-400 hover:text-red-500 border-2 border-red-500 border-opacity-10"
            >
              <LogOut className="w-5 h-5 sm:w-6 h-6" />
            </button>
          </div>
          </header>

          <div className="bg-[var(--bgLight)] p-6 rounded-3xl shadow-lg mb-8 border-b-4 border-[var(--primary)] border-opacity-20">
          <h3 className="text-lg font-bold mb-4 flex items-center gap-2">
            <Zap className="w-5 h-5 text-[var(--primary)]" />
            Select Variant
          </h3>
          <VariantSelector />
          </div>

          <div className="grid grid-cols-1 md:grid-cols-3 gap-6 sm:gap-8">
          <motion.div 
            whileHover={{ scale: 1.02 }}
            className="bg-[var(--bgLight)] p-6 sm:p-8 rounded-3xl shadow-xl border-b-8 border-[var(--accent)] border-opacity-50"
          >
            <div className="bg-[var(--primary)] w-14 h-14 sm:w-16 sm:h-16 rounded-2xl flex items-center justify-center mb-4 sm:mb-6">
              <Play className="w-7 h-7 sm:w-8 h-8 text-[var(--primaryText)] fill-current" />
            </div>
            <h2 className="text-xl sm:text-2xl font-bold mb-3 sm:mb-4">Public Match</h2>
            <p className="text-sm sm:text-base opacity-60 mb-6 leading-relaxed">Match against players within ±200 ELO range.</p>
            <button 
              onClick={startPublicMatch}
              className="w-full py-3 sm:py-4 bg-[var(--primary)] text-[var(--primaryText)] rounded-2xl font-bold text-base sm:text-lg hover:opacity-90 transition-all shadow-lg shadow-[var(--primary)]/20"
            >
              Find Match
            </button>
          </motion.div>

          <motion.div 
            whileHover={{ scale: 1.02 }}
            className="bg-[var(--bgLight)] p-6 sm:p-8 rounded-3xl shadow-xl border-b-8 border-[var(--accent)] border-opacity-50"
          >
            <div className="bg-[var(--accent)] w-14 h-14 sm:w-16 sm:h-16 rounded-2xl flex items-center justify-center mb-4 sm:mb-6">
              <Users className="w-7 h-7 sm:w-8 h-8 text-[var(--accentText)]" />
            </div>
            <h2 className="text-xl sm:text-2xl font-bold mb-3 sm:mb-4">Local Play</h2>
            <p className="text-sm sm:text-base opacity-60 mb-6 leading-relaxed">Play with a friend on the same machine.</p>
            <div className="h-[52px]"></div>
            <button 
              onClick={startLocalMatch}
              className="w-full py-3 sm:py-4 bg-[var(--accent)] text-[var(--accentText)] rounded-2xl font-bold text-base sm:text-lg hover:opacity-90 transition-all shadow-lg shadow-[var(--accent)]/20"
            >
              Start Local Game
            </button>
          </motion.div>

          <motion.div 
            whileHover={{ scale: 1.02 }}
            className="bg-[var(--bgLight)] p-6 sm:p-8 rounded-3xl shadow-xl border-b-8 border-purple-500 border-opacity-50"
          >
            <div className="bg-purple-500 w-14 h-14 sm:w-16 sm:h-16 rounded-2xl flex items-center justify-center mb-4 sm:mb-6">
              <Eye className="w-7 h-7 sm:w-8 h-8 text-white" />
            </div>
            <h2 className="text-xl sm:text-2xl font-bold mb-3 sm:mb-4">Spectate</h2>
            <p className="text-sm sm:text-base opacity-60 mb-6 leading-relaxed">Watch other players in action.</p>
            <div className="h-[52px]"></div>
            <button 
              onClick={() => setView('spectate')}
              className="w-full py-3 sm:py-4 bg-purple-500 text-white rounded-2xl font-bold text-base sm:text-lg hover:opacity-90 transition-all shadow-lg shadow-purple-500/20"
            >
              Start Spectating
            </button>
          </motion.div>

          <motion.div 
            whileHover={{ scale: 1.02 }}
            className="bg-[var(--bg)] p-6 sm:p-8 rounded-3xl shadow-xl border-b-8 border-[var(--accent)] border-opacity-50 md:col-span-3"
          >
            <div className="bg-[var(--primary)] bg-opacity-10 w-14 h-14 sm:w-16 sm:h-16 rounded-2xl flex items-center justify-center mb-4 sm:mb-6">
              <Hash className="w-7 h-7 sm:w-8 h-8 text-[var(--primaryText)]" />
            </div>
            <h2 className="text-xl sm:text-2xl font-bold mb-3 sm:mb-4">Private Match</h2>
            <p className="text-sm sm:text-base opacity-60 mb-6 sm:mb-8 leading-relaxed">Create a room or join a friend using a secret code.</p>

            <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
              <button 
                onClick={createPrivateMatch}
                className="w-full py-3 sm:py-4 bg-[var(--bg)] border-2 border-[var(--primary)] text-[var(--primary)] rounded-2xl font-bold text-base sm:text-lg hover:bg-[var(--primary)] hover:text-[var(--primaryText)] transition-all"
              >
                Create Room
              </button>
              <div className="flex flex-col sm:flex-row gap-2">
                <input 
                  type="text" 
                  placeholder="Enter Code" 
                  className="flex-1 px-4 py-3 sm:py-4 bg-[var(--primary)] bg-opacity-80 border border-[var(--primary)] border-opacity-10 rounded-2xl outline-none focus:ring-2 focus:ring-[var(--primary)] font-mono text-center uppercase text-base sm:text-lg text-[var(--primaryText)] placeholder:text-[var(--primaryText)] placeholder:opacity-60 shadow-inner"
                  value={joinCode}
                  onChange={(e) => setJoinCode(e.target.value)}
                />
                <button 
                  onClick={joinPrivateMatch}
                  className="w-full sm:w-auto px-8 py-3 sm:py-4 bg-[var(--primary)] text-[var(--primaryText)] rounded-2xl font-bold hover:opacity-90 transition-all"
                >
                  Join
                </button>
              </div>
            </div>
          </motion.div>
          </div>

          {error && (
          <motion.div 
            initial={{ opacity: 0, y: 10 }}
            animate={{ opacity: 1, y: 0 }}
            className="mt-8 p-4 bg-red-500 bg-opacity-10 border border-red-500 border-opacity-20 rounded-2xl text-center text-red-500 font-medium"
          >
            {error}
          </motion.div>
          )}

          {privateCode && (
          <div className="fixed inset-0 bg-black/50 backdrop-blur-sm flex items-center justify-center p-4 z-50">
            <motion.div 
              initial={{ scale: 0.9, opacity: 0 }}
              animate={{ scale: 1, opacity: 1 }}
              className="bg-[var(--bgLight)] p-8 rounded-3xl shadow-2xl max-w-sm w-full text-center relative border-b-8 border-[var(--primary)]"
            >
              <button 
                onClick={() => { setPrivateCode(null); setGameId(null); setPlayerColor(null); }}
                className="absolute top-4 right-4 p-2 text-gray-400 hover:text-red-500 transition-colors"
              >
                <LogOut className="w-5 h-5" />
              </button>
              <h3 className="text-2xl font-black mb-2">Room Created</h3>
              <p className="opacity-60 mb-6">Share this code with your friend</p>
              <div className="bg-[var(--primary)] text-[var(--primaryText)] p-6 rounded-2xl text-4xl font-mono font-black tracking-widest mb-6 shadow-xl">
                {privateCode}
              </div>
              <div className="mb-8 p-4 bg-[var(--primary)] bg-opacity-10 rounded-2xl border-2 border-[var(--primary)] border-opacity-20">
                <div className="flex items-center justify-between gap-4">
                  <div className="text-left">
                    <p className="font-black text-sm text-[var(--primaryText)]">Match Type</p>
                    <p className="opacity-60 text-xs text-[var(--primaryText)]">Rated games affect ELO</p>
                  </div>
                  <button 
                    onClick={toggleRated}
                    className={`relative w-14 h-8 rounded-full transition-colors duration-200 focus:outline-none ${isRated ? 'bg-[var(--boardLight)]' : 'bg-[var(--boardDark)]'}`}
                  >
                    <div className={`absolute top-1 left-1 bg-white w-6 h-6 rounded-full transition-transform duration-200 transform ${isRated ? 'translate-x-6' : 'translate-x-0'}`} />
                  </button>
                </div>
                <div className="mt-2 text-center">
                   <span className={`font-black text-lg uppercase tracking-wider text-[var(--primaryText)]`}>{isRated ? 'Rated' : 'Unrated'}</span>
                </div>
              </div>

              <p className="text-sm text-[var(--primary)] font-bold animate-pulse mb-4">Waiting for opponent...</p>
              <button 
                onClick={() => { setPrivateCode(null); setGameId(null); setPlayerColor(null); }}
                className="text-sm font-black text-[var(--primary)] hover:underline"
              >
                Cancel
              </button>
            </motion.div>
          </div>
        )}

        {showLeaderboard && (
          <div className="fixed inset-0 bg-black/50 backdrop-blur-sm flex items-center justify-center p-4 z-50">
            <motion.div 
              initial={{ scale: 0.9, opacity: 0 }}
              animate={{ scale: 1, opacity: 1 }}
              className="bg-[var(--bg)] p-6 sm:p-8 rounded-3xl shadow-2xl max-w-md w-full relative flex flex-col max-h-[90vh] text-[var(--text)] border border-[var(--primary)] border-opacity-10"
            >
              <button 
                onClick={() => setShowLeaderboard(false)}
                className="absolute top-4 right-4 p-2 text-gray-400 hover:text-red-500 transition-colors z-10"
              >
                <LogOut className="w-5 h-5" />
              </button>
              <h3 className="text-2xl font-bold mb-6 text-center shrink-0">Top Players</h3>
              <div className="space-y-3 overflow-y-auto pr-2 custom-scrollbar">
                {leaderboard.map((entry, idx) => (
                  <div key={entry.id} className="flex items-center justify-between p-4 bg-[var(--primary)] bg-opacity-10 rounded-2xl border-b-2 border-[var(--primary)] border-opacity-10">
                    <div className="flex items-center gap-4">
                      <span className="font-black opacity-40 w-6 text-[var(--primaryText)]">#{idx + 1}</span>
                      <span className="font-black text-[var(--primaryText)]">{entry.username}</span>
                    </div>
                    <div className="flex items-center gap-2">
                      {playersInGame.has(entry.username) && (
                        <button
                          onClick={() => {
                            setShowLeaderboard(false);
                            // Store the username to spectate in localStorage so SpectateView can pick it up
                            localStorage.setItem('spectateUsername', entry.username);
                            setView('spectate');
                          }}
                          className="px-3 py-1 bg-purple-500 text-white rounded-lg text-sm font-bold hover:opacity-90 transition-all flex items-center gap-1"
                        >
                          <Eye className="w-4 h-4" />
                          Spectate
                        </button>
                      )}
                      <span className="font-black text-[var(--primaryText)] min-w-fit">{entry.elo} ELO</span>
                    </div>
                  </div>
                ))}
              </div>
            </motion.div>
          </div>
        )}

        {showTutorial && (
          <div className="fixed inset-0 bg-black/50 backdrop-blur-sm flex items-center justify-center p-4 z-50">
            <motion.div 
              initial={{ scale: 0.9, opacity: 0 }}
              animate={{ scale: 1, opacity: 1 }}
              className="bg-[var(--bg)] p-8 rounded-3xl shadow-2xl max-w-lg w-full relative max-h-[80vh] overflow-y-auto text-[var(--text)] border border-[var(--primary)] border-opacity-10"
            >
              <button 
                onClick={() => setShowTutorial(false)}
                className="absolute top-4 right-4 p-2 text-gray-400 hover:text-red-500 transition-colors"
              >
                <LogOut className="w-5 h-5" />
              </button>
              <h3 className="text-2xl font-bold mb-6 text-center">How to Play</h3>
              <div className="space-y-6 opacity-80">
                <div>
                  <h4 className="font-bold text-[var(--primary)] mb-2">Objective</h4>
                  <p>Be the first to align 4 pieces of your color in a row (horizontally, vertically, or diagonally).</p>
                </div>
                <div>
                  <h4 className="font-bold text-[var(--primary)] mb-2">Movement</h4>
                  <p>Pieces move like chess Rooks—they slide in any of the four cardinal directions (up, down, left, right) until they hit the board edge or another piece.</p>
                </div>
                <div>
                  <h4 className="font-bold text-[var(--primary)] mb-2">Turns</h4>
                  <p>White always moves first. Players take turns sliding one piece at a time.</p>
                </div>
                <button 
                  onClick={() => setShowTutorial(false)}
                  className="w-full py-3 bg-[var(--primary)] text-[var(--bg)] rounded-xl font-bold mt-4"
                >
                  Got it!
                </button>
              </div>
            </motion.div>
          </div>
        )}

        {showPatchNotes && (
          <div className="fixed inset-0 bg-black/50 backdrop-blur-sm flex items-center justify-center p-4 z-50">
            <motion.div 
              initial={{ scale: 0.9, opacity: 0 }}
              animate={{ scale: 1, opacity: 1 }}
              className="bg-[var(--bg)] p-8 rounded-3xl shadow-2xl max-w-lg w-full relative max-h-[80vh] overflow-y-auto text-[var(--text)] border border-[var(--primary)] border-opacity-10"
            >
              <button 
                onClick={() => setShowPatchNotes(false)}
                className="absolute top-4 right-4 p-2 text-gray-400 hover:text-red-500 transition-colors"
              >
                <LogOut className="w-5 h-5" />
              </button>
              <h3 className="text-2xl font-bold mb-6 text-center flex items-center justify-center gap-2">
                <Zap className="text-yellow-500" />
                Patch Notes
              </h3>
              <div className="space-y-8">
                <section>
                  <h4 className="font-bold text-[var(--primary)] mb-3 flex items-center gap-2 border-b border-[var(--primary)] border-opacity-10 pb-2">
                    <span className="p-1 bg-[var(--primary)] bg-opacity-10 rounded-lg">🎮</span>
                    New Variants
                  </h4>
                  <ul className="space-y-2 text-sm opacity-80">
                    <li><span className="font-bold">Fog of War:</span> Only see squares you can move to! Strategic vision is key.</li>
                    <li><span className="font-bold">Random Setup:</span> Pieces start in chaotic, randomized positions.</li>
                    <li><span className="font-bold">Schizophrenic:</span> A random square changes every turn! Watch your step.</li>
                  </ul>
                </section>

                <section>
                  <h4 className="font-bold text-[var(--primary)] mb-3 flex items-center gap-2 border-b border-[var(--primary)] border-opacity-10 pb-2">
                    <span className="p-1 bg-[var(--primary)] bg-opacity-10 rounded-lg">🎨</span>
                    UI Overhaul
                  </h4>
                  <ul className="space-y-2 text-sm opacity-80">
                    <li><span className="font-bold">High Contrast:</span> Improved visibility for better gameplay across all devices.</li>
                    <li><span className="font-bold">Cosmetics:</span> Redesigned and enlarged Cosmetics button for easier access.</li>
                    <li><span className="font-bold">Leaderboard:</span> Added smooth scroll for mobile users to browse rankings.</li>
                  </ul>
                </section>

                <section>
                  <h4 className="font-bold text-[var(--primary)] mb-3 flex items-center gap-2 border-b border-[var(--primary)] border-opacity-10 pb-2">
                    <span className="p-1 bg-[var(--primary)] bg-opacity-10 rounded-lg">🔒</span>
                    Private Rooms
                  </h4>
                  <ul className="space-y-2 text-sm opacity-80">
                    <li><span className="font-bold">Rated Toggle:</span> Added a new toggle for private matches to set them as Rated or Unrated.</li>
                  </ul>
                </section>

                <section>
                  <h4 className="font-bold text-[var(--primary)] mb-3 flex items-center gap-2 border-b border-[var(--primary)] border-opacity-10 pb-2">
                    <span className="p-1 bg-[var(--primary)] bg-opacity-10 rounded-lg">📊</span>
                    Stats Tracking
                  </h4>
                  <ul className="space-y-2 text-sm opacity-80">
                    <li><span className="font-bold">Enhanced Stats:</span> Now tracking Total Wins and Games Played on your profile.</li>
                  </ul>
                </section>

                <button 
                  onClick={() => setShowPatchNotes(false)}
                  className="w-full py-3 bg-[var(--primary)] text-[var(--bg)] rounded-xl font-bold mt-4"
                >
                  Close
                </button>
              </div>
            </motion.div>
          </div>
        )}

        <motion.div 
          initial={{ opacity: 0, y: 20 }}
          animate={{ opacity: 1, y: 0 }}
          className="mt-12 bg-[var(--primary)] p-8 rounded-3xl shadow-xl border-b-8 border-[var(--secondary)] text-[var(--bg)]"
        >
          <h3 className="text-2xl font-bold mb-4 flex items-center gap-2">
            <Trophy className="w-6 h-6" />
            Join the Slide Bot Challenge!
          </h3>
          <div className="space-y-4 text-sm sm:text-base leading-relaxed opacity-90">
            <p>Are you ready to prove your coding prowess? We are hosting a competition to find the official AI for Slide! We’re looking for a bot that can master the momentum-based movement and sharp tactical lines of our 6x6 grid.</p>
            <p>The challenge is simple: build a Python-based bot that can outsmart the rest in a full round-robin tournament. The creator of the winning bot will not only see their code integrated as the site’s official "Play vs. Bot" mode but will also walk away with a £5 prize.</p>
            <p>Whether you’re a Minimax master or just love a good logic puzzle, we want to see what you can build!</p>
            <div className="pt-4">
              <p className="font-bold mb-2">Ready to start?</p>
              <p>Download the DevKit below to get the game engine, a Tkinter testing UI, and full submission instructions:</p>
              <a 
                href="https://github.com/MalachiBurnett/bot-comp/" 
                target="_blank" 
                rel="noopener noreferrer"
                className="inline-flex items-center gap-2 mt-4 px-6 py-3 bg-[var(--bg)] text-[var(--primary)] rounded-xl font-bold hover:opacity-90 transition-all"
              >
                👉 Download the Slide DevKit on GitHub
              </a>
            </div>
          </div>
        </motion.div>
      </div>
    </div>
  );
};
