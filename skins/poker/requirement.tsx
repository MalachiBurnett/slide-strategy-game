export const progress = (user: any) => user.games_random_setup || 0;
export const checkUnlock = (user: any) => (user.games_random_setup || 0) >= 10;
export const progressMax = (user: any) => 10;