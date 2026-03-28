import fs from "fs";
import path from "path";
import { db } from "./db";

export function getSkins() {
  const skinsPath = path.join(process.cwd(), 'skins');
  if (!fs.existsSync(skinsPath)) return [];
  
  const skinFolders = fs.readdirSync(skinsPath);
  return skinFolders.map(folder => {
    const p = path.join(skinsPath, folder);
    if (!fs.statSync(p).isDirectory()) return null;
    
    const description = fs.existsSync(path.join(p, 'description.txt')) 
      ? fs.readFileSync(path.join(p, 'description.txt'), 'utf8') 
      : '';
    const requirementJson = fs.existsSync(path.join(p, 'requirement.json'))
      ? fs.readFileSync(path.join(p, 'requirement.json'), 'utf8')
      : '{}';
      
    return {
      id: folder,
      name: folder.charAt(0).toUpperCase() + folder.slice(1),
      description,
      requirementCode: requirementJson // Keep property name for frontend compatibility
    };
  }).filter(Boolean);
}

export function updateUnlockedSkins(userId: number) {
  return new Promise<void>((resolve) => {
    db.all("SELECT id FROM users WHERE is_guest = 0 ORDER BY elo DESC", (err, leaderboard: any[]) => {
      const rank = leaderboard ? leaderboard.findIndex(u => u.id === userId) + 1 : 100;

      db.get("SELECT * FROM users WHERE id = ?", [userId], (err, user: any) => {
        if (err || !user) return resolve();

        let unlockedSkins = JSON.parse(user.unlocked_skins || '["classic"]');
        const allSkins = getSkins();
        let updated = false;

        allSkins.forEach((skin: any) => {
          if (unlockedSkins.includes(skin.id)) return;

          try {
            const req = JSON.parse(skin.requirementCode);
            if (!req.checkUnlock) return;

            const checkUnlock = new Function('user', `return ${req.checkUnlock}`);
            const isUnlocked = checkUnlock({
              elo: user.elo,
              wins: user.wins || 0,
              gamesPlayed: user.games_played || 0,
              games_random_setup: user.games_random_setup || 0,
              games_1min: user.games_1min || 0,
              games_fog_of_war: user.games_fog_of_war || 0,
              spectators_count: user.spectators_count || 0,
              leaderboardRank: rank
            });

            if (isUnlocked) {
              unlockedSkins.push(skin.id);
              updated = true;
            }
          } catch (e) {
            console.error(`Error checking skin ${skin.id}:`, e);
          }
        });

        if (updated) {
          db.run("UPDATE users SET unlocked_skins = ? WHERE id = ?", [JSON.stringify(unlockedSkins), userId], () => resolve());
        } else {
          resolve();
        }
      });
    });
  });
}
