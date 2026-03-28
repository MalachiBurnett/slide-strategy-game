export const progress = (user: any) => user.gamesPlayed || 0;
export const checkUnlock = (user: any) => (user.gamesPlayed || 0) >= 5;
export const progressMax = (user: any) => 5;
