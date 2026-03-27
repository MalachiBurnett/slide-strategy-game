import React, { useState, useEffect } from 'react';
import { motion, AnimatePresence } from 'motion/react';
import { ArrowLeft, ChevronLeft, ChevronRight, Lock, CheckCircle2, Trophy, ChevronUp, ChevronDown } from 'lucide-react';
import { UserData, Skin, Theme, LeaderboardEntry, SkinData } from '../types/game';
import { THEMES } from '../constants/game';
import { Piece } from './Piece';

interface CosmeticsViewProps {
  user: UserData;
  leaderboard: LeaderboardEntry[];
  onBack: () => void;
  onUpdateCosmetics: (skin: Skin, theme: Theme) => Promise<void>;
}

export const CosmeticsView: React.FC<CosmeticsViewProps> = ({
  user,
  leaderboard,
  onBack,
  onUpdateCosmetics,
}) => {
  const [skins, setSkins] = useState<SkinData[]>([]);
  const [selectedSkinIdx, setSelectedSkinIdx] = useState(0);
  const [selectedThemeIdx, setSelectedThemeIdx] = useState(
    THEMES.findIndex(t => t.id === user.theme)
  );
  const [isUpdating, setIsUpdating] = useState(false);

  useEffect(() => {
    fetch('/api/skins')
      .then(res => res.json())
      .then(data => {
        setSkins(data);
        const currentIdx = data.findIndex((s: SkinData) => s.id === user.skin);
        if (currentIdx !== -1) setSelectedSkinIdx(currentIdx);
      });
  }, [user.skin]);

  const rank = leaderboard.findIndex(e => e.id === user.id) + 1;
  const currentSkin = skins[selectedSkinIdx];
  const currentTheme = THEMES[selectedThemeIdx];

  const getSkinStatus = (skin: SkinData) => {
    const isUnlocked = user.unlockedSkins.includes(skin.id);
    let progressValue = 0;
    let progressMax = 1;
    let unlockDesc = "Unlocked by default";

    try {
      const code = skin.requirementCode.replace(/export const/g, 'const');
      const getExports = new Function(`${code}; return { checkUnlock, progress, unlockRequirement, progressMax };`);
      const exports = getExports();
      
      const userWithRank = { ...user, leaderboardRank: rank };
      if (exports.progress) progressValue = exports.progress(userWithRank);
      if (exports.progressMax) progressMax = exports.progressMax(userWithRank);
      if (exports.unlockRequirement) unlockDesc = exports.unlockRequirement;
      else if (skin.description) unlockDesc = skin.description;
    } catch (e) {
      // Use defaults
    }

    return { isUnlocked, progressValue, progressMax, unlockDesc };
  };

  const currentStatus = currentSkin ? getSkinStatus(currentSkin) : null;

  const handlePrevSkin = () => {
    setSelectedSkinIdx((prev) => (prev > 0 ? prev - 1 : prev));
  };

  const handleNextSkin = () => {
    setSelectedSkinIdx((prev) => (prev < skins.length - 1 ? prev + 1 : prev));
  };

  const handlePrevTheme = () => {
    setSelectedThemeIdx((prev) => (prev > 0 ? prev - 1 : THEMES.length - 1));
  };

  const handleNextTheme = () => {
    setSelectedThemeIdx((prev) => (prev < THEMES.length - 1 ? prev + 1 : 0));
  };

  const handleEquip = async () => {
    if (!currentSkin) return;
    setIsUpdating(true);
    await onUpdateCosmetics(currentSkin.id, currentTheme.id);
    setIsUpdating(false);
  };

  if (!currentSkin) return null;

  return (
    <div className="min-h-screen bg-[var(--bg)] text-[var(--text)] p-4 sm:p-8 font-sans transition-colors duration-500">
      <div className="max-w-4xl mx-auto">
        <header className="flex justify-between items-center mb-12">
          <button 
            onClick={onBack}
            className="p-3 bg-[var(--primary)] rounded-full shadow-lg hover:scale-110 transition-transform"
          >
            <ArrowLeft className="w-6 h-6 text-[var(--bgLight)]" />
          </button>
          <div className="flex gap-6 items-center">
            <div className="text-right">
              <p className="text-sm opacity-60 font-bold uppercase tracking-widest">Your Rating</p>
              <p className="text-2xl font-black text-[var(--primary)]">{user.elo} ELO</p>
            </div>
            <div className="h-10 w-px bg-[var(--accent)] opacity-30"></div>
            <div className="text-right flex flex-col items-end">
              <p className="text-sm opacity-60 font-bold uppercase tracking-widest">Stats</p>
              <p className="text-lg font-black text-[var(--primary)]">{user.wins} Wins / {user.gamesPlayed} Played</p>
            </div>
          </div>
        </header>

        <section className="mb-16">
          <h2 className="text-3xl font-black mb-8 flex items-center gap-3">
            <div className="w-2 h-8 bg-[var(--primary)] rounded-full"></div>
            SKINS
          </h2>
          
          <div className="relative h-64 flex items-center justify-center overflow-hidden mb-8">
            <button 
              onClick={handlePrevSkin}
              disabled={selectedSkinIdx === 0}
              className="absolute left-0 z-20 p-2 rounded-full bg-[var(--primary)] text-[var(--bgLight)] disabled:opacity-20"
            >
              <ChevronLeft className="w-8 h-8" />
            </button>
            
            <div className="flex items-center justify-center gap-4 sm:gap-8">
              {skins.map((skin, idx) => {
                const distance = idx - selectedSkinIdx;
                if (Math.abs(distance) > 2) return null;
                
                return (
                  <motion.div
                    key={skin.id}
                    animate={{ 
                      x: distance * 120,
                      scale: distance === 0 ? 1.25 : (Math.abs(distance) === 1 ? 0.9 : 0.75),
                      opacity: distance === 0 ? 1 : (Math.abs(distance) === 1 ? 0.6 : 0.3),
                      zIndex: 10 - Math.abs(distance)
                    }}
                    className="absolute cursor-pointer"
                    onClick={() => setSelectedSkinIdx(idx)}
                  >
                    <div className="relative p-4 pt-10">
                      {!user.unlockedSkins.includes(skin.id) && (
                        <div className="absolute top-0 left-1/2 -translate-x-1/2 z-20">
                          <Lock className="w-6 h-6 text-[var(--primary)]" />
                        </div>
                      )}
                      <div className="flex gap-2">
                        <div className={`${distance === 0 ? 'flex' : 'relative'}`}>
                          <Piece type="W" skin={skin.id} />
                          <div className={distance === 0 ? "ml-2" : "absolute top-0 left-4"}>
                            <Piece type="B" skin={skin.id} />
                          </div>
                        </div>
                      </div>
                    </div>
                  </motion.div>
                );
              })}
            </div>

            <button 
              onClick={handleNextSkin}
              disabled={selectedSkinIdx === skins.length - 1}
              className="absolute right-0 z-20 p-2 rounded-full bg-[var(--primary)] text-[var(--bgLight)] disabled:opacity-20"
            >
              <ChevronRight className="w-8 h-8" />
            </button>
          </div>

          <div className="text-center">
            <h3 className="text-2xl font-bold mb-2">{currentSkin.name}</h3>
            <p className="opacity-60 mb-6">{currentSkin.description}</p>
            
            {currentStatus && !currentStatus.isUnlocked ? (
              <div className="bg-[var(--bgDark)] p-6 rounded-2xl border border-[var(--primary)] border-opacity-20 inline-block">
                <p className="text-sm font-bold uppercase opacity-60 mb-1">How to unlock</p>
                <p className="font-bold text-lg">{currentStatus.unlockDesc}</p>
                <div className="mt-4 w-64 h-3 bg-[var(--bg)] rounded-full overflow-hidden shadow-inner">
                  <div 
                    className="h-full bg-[var(--primary)] transition-all duration-1000" 
                    style={{ width: `${Math.min(100, (currentStatus.progressValue / currentStatus.progressMax) * 100)}%` }}
                  ></div>
                </div>
                <p className="text-xs opacity-50 mt-2 font-black">{currentStatus.progressValue} / {currentStatus.progressMax}</p>
              </div>
            ) : (
              <button
                onClick={handleEquip}
                disabled={isUpdating || (user.skin === currentSkin.id && user.theme === currentTheme.id)}
                className={`
                  px-12 py-4 rounded-2xl font-black text-xl transition-all shadow-xl
                  ${user.skin === currentSkin.id && user.theme === currentTheme.id
                    ? 'bg-green-500 text-white cursor-default'
                    : 'bg-[var(--primary)] text-[var(--bgLight)] hover:scale-105 active:scale-95 shadow-[var(--primary)]/20'}
                `}
              >
                {user.skin === currentSkin.id && user.theme === currentTheme.id ? (
                  <span className="flex items-center gap-2"><CheckCircle2 /> EQUIPPED</span>
                ) : (
                  'EQUIP SKIN'
                )}
              </button>
            )}
          </div>
        </section>

        <section>
          <h2 className="text-3xl font-black mb-8 flex items-center gap-3">
            <div className="w-2 h-8 bg-[var(--primary)] rounded-full"></div>
            THEMES
          </h2>

          <div className="bg-[var(--bgLight)] p-8 rounded-[3rem] shadow-xl border-2 border-[var(--primary)] border-opacity-5 flex flex-col md:flex-row items-center gap-12">
            {/* Theme Preview - 3x3 Grid */}
            <div 
              className="grid grid-cols-3 gap-1 p-2 rounded-2xl border-4 shadow-2xl transition-colors duration-500"
              style={{ 
                backgroundColor: currentTheme.colors.boardBorder,
                borderColor: currentTheme.colors.boardBorder
              }}
            >
              {[...Array(9)].map((_, i) => (
                <div 
                  key={i}
                  className="w-12 h-12 rounded-md"
                  style={{ 
                    backgroundColor: (Math.floor(i/3) + (i%3)) % 2 === 0 
                      ? currentTheme.colors.boardLight 
                      : currentTheme.colors.boardDark 
                  }}
                />
              ))}
            </div>

            <div className="flex-1 flex flex-col items-center md:items-start text-center md:text-left">
              <div className="flex items-center gap-4 mb-6">
                <div className="flex flex-col gap-1">
                  <button onClick={handlePrevTheme} className="p-1 hover:text-[var(--primary)] transition-colors">
                    <ChevronUp className="w-8 h-8" />
                  </button>
                  <button onClick={handleNextTheme} className="p-1 hover:text-[var(--primary)] transition-colors">
                    <ChevronDown className="w-8 h-8" />
                  </button>
                </div>
                <h3 className="text-4xl font-black tracking-tight">{currentTheme.name}</h3>
              </div>
              
              <button
                onClick={handleEquip}
                disabled={isUpdating || (user.skin === currentSkin.id && user.theme === currentTheme.id)}
                className={`
                  px-12 py-4 rounded-2xl font-black text-xl transition-all shadow-xl w-full md:w-auto
                  ${user.theme === currentTheme.id && user.skin === currentSkin.id
                    ? 'bg-green-500 text-white cursor-default'
                    : 'bg-[var(--primary)] text-[var(--bg)] hover:scale-105 shadow-[var(--primary)]/20'}
                `}
              >
                {user.theme === currentTheme.id && user.skin === currentSkin.id ? (
                  <span className="flex items-center gap-2 justify-center"><CheckCircle2 /> EQUIPPED</span>
                ) : (
                  'APPLY THEME'
                )}
              </button>
            </div>
          </div>
        </section>
      </div>
    </div>
  );
};
