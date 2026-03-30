export type Piece = 'W' | 'B' | '0';
export type Board = Piece[][];
export type Turn = 'W' | 'B';
export type Status = 'active' | 'waiting' | 'finished';

export type Skin = string;
export type Theme = 'dark' | 'light' | 'beach' | 'wooden' | 'connect4' | 'wii' | 'oscar' | 'sonic';

export interface SkinData {
  id: string;
  name: string;
  description: string;
  requirementCode: string;
}

export interface GameState {
  board: Board;
  turn: Turn;
  status: Status;
  winner: Piece | null;
  winningLine?: {r: number, c: number}[] | null;
  eloChange?: number;
  timerW?: number;
  timerB?: number;
  skinW?: Skin;
  skinB?: Skin;
  variant?: string;
  isRated?: boolean;
}

export interface UserData {
  id: number;
  username: string;
  elo: number;
  wins: number;
  gamesPlayed: number;
  gamesRandomSetup: number;
  games1min: number;
  gamesFogOfWar: number;
  skin: Skin;
  theme: Theme;
  unlockedSkins: Skin[];
  is_guest?: boolean;
}

export interface LeaderboardEntry {
  id: number;
  username: string;
  elo: number;
}
