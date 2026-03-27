export const progress = (user: any) => user.wins || 0;
export const checkUnlock = (user: any) => (user.wins || 0) >= 20;
export const progressMax = (user: any) => 20;