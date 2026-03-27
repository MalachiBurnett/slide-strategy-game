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
      boardDark: '#bc9d6a',
      boardBorder: '#8b4513'
    }
  },
  { 
    id: 'dark', 
    name: 'Midnight', 
    colors: {
      bg: '#1a1a1a',
      bgLight: '#2a2a2a',
      bgDark: '#0a0a0a',
      text: '#e0e0e0',
      primary: '#3a3a3a',
      primaryText: '#ffffff',
      secondary: '#2a2a2a',
      accent: '#4a4a4a',
      accentText: '#ffffff',
      boardLight: '#3d3d3d',
      boardDark: '#242424',
      boardBorder: '#000000'
    }
  },
  { 
    id: 'light', 
    name: 'Clean White', 
    colors: {
      bg: '#ffffff',
      bgLight: '#f0f0f0',
      bgDark: '#e0e0e0',
      text: '#000000',
      primary: '#2a2a2a',
      primaryText: '#ffffff',
      secondary: '#4a4a4a',
      accent: '#6a6a6a',
      accentText: '#ffffff',
      boardLight: '#f8f8f8',
      boardDark: '#e8e8e8',
      boardBorder: '#cccccc'
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
      boardDark: '#fab1a0',
      boardBorder: '#e67e22'
    }
  },
  { 
    id: 'connect4', 
    name: 'Connect 4', 
    colors: {
      bg: '#2980b9',
      bgLight: '#3498db',
      bgDark: '#1c5980',
      text: '#ffffff',
      primary: '#2c3e50',
      primaryText: '#ffffff',
      secondary: '#34495e',
      accent: '#f1c40f',
      accentText: '#2c3e50',
      boardLight: '#3498db',
      boardDark: '#2980b9',
      boardBorder: '#2c3e50'
    }
  },
  { 
    id: 'wii', 
    name: 'Wii Menu', 
    colors: {
      bg: '#ffffff',
      bgLight: '#f5f5f5',
      bgDark: '#ebebeb',
      text: '#4d4d4d',
      primary: '#00adef',
      primaryText: '#ffffff',
      secondary: '#e0e0e0',
      accent: '#00adef',
      accentText: '#ffffff',
      boardLight: '#ffffff',
      boardDark: '#f0f0f0',
      boardBorder: '#ebebeb'
    }
  },
  { 
    id: 'oscar', 
    name: 'Oscar', 
    colors: {
      bg: '#fff0f5',
      bgLight: '#fffafa',
      bgDark: '#ffe4e1',
      text: '#4a3728',
      primary: '#ff69b4',
      primaryText: '#ffffff',
      secondary: '#ffb6c1',
      accent: '#db7093',
      accentText: '#ffffff',
      boardLight: '#ffffff',
      boardDark: '#ffe4e1',
      boardBorder: '#ff69b4'
    }
  }
];
