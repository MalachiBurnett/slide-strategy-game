import { Board, Turn, Skin, Theme } from '../src/types/game';

export interface User {
  id: number;
  username: string;
  password?: string;
  elo: number;
  is_guest: boolean;
  skin: Skin;
  theme: Theme;
  unlocked_skins: string; // JSON string
  wins: number;
  games_played: number;
}

export interface Game {
  id: string;
  board: string; // JSON string
  turn: Turn;
  player_w: string;
  player_b: string;
  status: 'waiting' | 'active' | 'finished';
  winner: Turn | null;
  is_private: boolean;
  code: string | null;
  timer_w: number;
  timer_b: number;
  increment: number;
  last_move_time: number;
  variant: string;
  is_rated: boolean;
}

export interface SkinRequirement {
  id: string;
  name: string;
  description: string;
  requirementCode: string;
}
