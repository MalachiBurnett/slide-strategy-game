export const progress = (user: any) => user.games_1min || 0;
export const checkUnlock = (user: any) => (user.games_1min || 0) >= 10;
export const progressMax = (user: any) => 10;