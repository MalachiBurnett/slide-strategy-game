export const progress = (user: any) => user.leaderboardRank || 100;
export const checkUnlock = (user: any) => (user.leaderboardRank || 100) === 1;
export const progressMax = (user: any) => 1;