export const progress = (user: any) => user.spectators_count || 0;
export const checkUnlock = (user: any) => (user.spectators_count || 0) >= 5;
export const progressMax = (user: any) => 5;
