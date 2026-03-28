import { Board, Skin, Theme } from '../types/game';

export const INITIAL_BOARD: Board = [
  ['B', '0', 'W', 'B', '0', 'W'],
  ['0', '0', '0', '0', '0', '0'],
  ['W', '0', '0', '0', '0', 'B'],
  ['B', '0', '0', '0', '0', 'W'],
  ['0', '0', '0', '0', '0', '0'],
  ['W', '0', 'B', 'W', '0', 'B']
];

export interface SkinData {
  id: Skin;
  name: string;
  description: string;
  requirementCode: string;
}

export interface ThemeData {
  id: Theme;
  name: string;
  colors: {
    bg: string;
    bgLight: string;
    bgDark: string;
    text: string;
    primary: string;
    primaryText: string;
    secondary: string;
    accent: string;
    accentText: string;
    boardLight: string;
    boardDark: string;
    boardBorder: string;
  };
}

export const THEMES: ThemeData[] = [
  { 
    id: 'wooden', 
    name: 'Wooden (Classic)', 
    colors: {
      bg: '#f4f1ea',
      bgLight: '#ffffff',
      bgDark: '#e3d9c6',
      text: '#4a3728',
      primary: '#8b4513',
      primaryText: '#ffffff',
      secondary: '#6d3610',
      accent: '#d2b48c',
      accentText: '#4a3728',
      boardLight: '#decba4',
      boardDark: '#a67c52', // Lighter than #8b4513
      boardBorder: '#5d2e0a'
    }
  },
  { 
    id: 'dark', 
    name: 'Midnight', 
    colors: {
      bg: '#0f172a',
      bgLight: '#1e293b',
      bgDark: '#020617',
      text: '#f8fafc',
      primary: '#38bdf8',
      primaryText: '#0f172a',
      secondary: '#0ea5e9',
      accent: '#94a3b8',
      accentText: '#0f172a',
      boardLight: '#334155',
      boardDark: '#0f172a',
      boardBorder: '#38bdf8'
    }
  },
  { 
    id: 'light', 
    name: 'Panda White', 
    colors: {
      bg: '#f8fafc',
      bgLight: '#ffffff',
      bgDark: '#f1f5f9',
      text: '#0f172a',
      primary: '#0f172a',
      primaryText: '#ffffff',
      secondary: '#334155',
      accent: '#64748b',
      accentText: '#ffffff',
      boardLight: '#f1f5f9',
      boardDark: '#94a3b8',
      boardBorder: '#0f172a'
    }
  },
  { 
    id: 'beach', 
    name: 'Tropical Beach', 
    colors: {
      bg: '#fff9e6',
      bgLight: '#fffdf5',
      bgDark: '#f7edca',
      text: '#2c3e50',
      primary: '#1abc9c',
      primaryText: '#ffffff',
      secondary: '#16a085',
      accent: '#f1c40f',
      accentText: '#2c3e50',
      boardLight: '#ffeaa7',
      boardDark: '#e67e22',
      boardBorder: '#d35400'
    }
  },
  { 
    id: 'connect4', 
    name: 'Connect 4', 
    colors: {
      bg: '#1e40af',
      bgLight: '#2563eb',
      bgDark: '#1e3a8a',
      text: '#ffffff',
      primary: '#facc15',
      primaryText: '#1e3a8a',
      secondary: '#eab308',
      accent: '#dc2626',
      accentText: '#ffffff',
      boardLight: '#3b82f6',
      boardDark: '#1e40af',
      boardBorder: '#1e3a8a'
    }
  },
  { 
    id: 'wii', 
    name: 'Wii Menu', 
    colors: {
      bg: '#ffffff',
      bgLight: '#f8fafc',
      bgDark: '#f1f5f9',
      text: '#4d4d4d',
      primary: '#00adef',
      primaryText: '#ffffff',
      secondary: '#e2e8f0',
      accent: '#00adef',
      accentText: '#ffffff',
      boardLight: '#ffffff',
      boardDark: '#cbd5e1',
      boardBorder: '#94a3b8'
    }
  },
  { 
    id: 'oscar', 
    name: 'Oscar', 
    colors: {
      bg: '#fff1f2',
      bgLight: '#ffffff',
      bgDark: '#ffe4e6',
      text: '#881337',
      primary: '#fb7185',
      primaryText: '#ffffff',
      secondary: '#f43f5e',
      accent: '#e11d48',
      accentText: '#ffffff',
      boardLight: '#fff1f2',
      boardDark: '#f9a8d4', // Lighter pink
      boardBorder: '#9f1239'
    }
  },
  { 
    id: 'sonic', 
    name: 'Green Hill Zone', 
    colors: {
      bg: '#87ceeb', // Sky blue
      bgLight: '#ffffff',
      bgDark: '#5dade2',
      text: '#2c3e50',
      primary: '#228b22', // Forest green
      primaryText: '#ffffff',
      secondary: '#1e6b1e',
      accent: '#32cd32', // Lime green
      accentText: '#ffffff',
      boardLight: '#cd853f', // Peru (brown)
      boardDark: '#8b4513', // Saddle brown (darker brown)
      boardBorder: '#32cd32' // Lime green
    }
  }
];
