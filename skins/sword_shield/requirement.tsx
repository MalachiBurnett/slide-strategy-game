export const progress = (user: any) => user.elo || 600;
export const checkUnlock = (user: any) => (user.elo || 0) >= 800;
export const progressMax = (user: any) => 800;