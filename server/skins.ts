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
    const requirementCode = fs.existsSync(path.join(p, 'requirement.tsx'))
      ? fs.readFileSync(path.join(p, 'requirement.tsx'), 'utf8')
      : '';
      
    return {
      id: folder,
      name: folder.charAt(0).toUpperCase() + folder.slice(1),
      description,
      requirementCode
    };
  }).filter(Boolean);
}

export function updateUnlockedSkins(userId: number) {
  return new Promise<void>((resolve) => {
    db.get("SELECT * FROM users WHERE id = ?", [userId], (err, user: any) => {
      if (err || !user) return resolve();

      let unlockedSkins = JSON.parse(user.unlocked_skins || '["classic"]');
      const allSkins = getSkins();
      let updated = false;

      allSkins.forEach((skin: any) => {
        if (unlockedSkins.includes(skin.id)) return;

        try {
          // SECURE: Use a sandbox or a very restricted Function if possible
          // For now, mirroring the existing logic but being aware of "Never trust frontend"
          // This code runs on server, so it's only as secure as the files in skins/ folder.
          const code = skin.requirementCode.replace(/export const/g, 'const');
          const getExports = new Function(`${code}; return { checkUnlock };`);
          const { checkUnlock } = getExports();
          const isUnlocked = checkUnlock({
            elo: user.elo,
            wins: user.wins || 0,
            gamesPlayed: user.games_played || 0
          });

          if (isUnlocked) {
            unlockedSkins.push(skin.id);
            updated = true;
          }
        } catch (e) {
          // Skip
        }
      });

      if (updated) {
        db.run("UPDATE users SET unlocked_skins = ? WHERE id = ?", [JSON.stringify(unlockedSkins), userId], () => resolve());
      } else {
        resolve();
      }
    });
  });
}
