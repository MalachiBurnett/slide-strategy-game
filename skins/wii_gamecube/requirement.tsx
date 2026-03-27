export const progress = (user: any) => user.games_fog_of_war || 0;
export const checkUnlock = (user: any) => (user.games_fog_of_war || 0) >= 10;
export const progressMax = (user: any) => 10;