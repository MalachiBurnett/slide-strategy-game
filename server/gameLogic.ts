export const INITIAL_BOARD = [
  ['B', '0', 'W', 'B', '0', 'W'],
  ['0', '0', '0', '0', '0', '0'],
  ['W', '0', '0', '0', '0', 'B'],
  ['B', '0', '0', '0', '0', 'W'],
  ['0', '0', '0', '0', '0', '0'],
  ['W', '0', 'B', 'W', '0', 'B']
];

export function generateBoard(variant: string) {
  if (variant !== 'random_setup') return INITIAL_BOARD;
  
  let board = Array(6).fill(null).map(() => Array(6).fill('0'));
  let pieces = ['W', 'W', 'W', 'W', 'W', 'W', 'B', 'B', 'B', 'B', 'B', 'B'];
  
  // Place 12 pieces randomly
  let placed = 0;
  while (placed < 12) {
    let r = Math.floor(Math.random() * 6);
    let c = Math.floor(Math.random() * 6);
    if (board[r][c] === '0') {
      board[r][c] = pieces[placed];
      placed++;
    }
  }
  return board;
}

export function checkWin(board: string[][]) {
  const size = 6;
  // Horizontal
  for (let r = 0; r < size; r++) {
    for (let c = 0; c <= size - 4; c++) {
      const p = board[r][c];
      if (p !== '0' && p === board[r][c+1] && p === board[r][c+2] && p === board[r][c+3]) {
        return { winner: p, line: [{r: r, c: c}, {r: r, c: c+1}, {r: r, c: c+2}, {r: r, c: c+3}] };
      }
    }
  }
  // Vertical
  for (let c = 0; c < size; c++) {
    for (let r = 0; r <= size - 4; r++) {
      const p = board[r][c];
      if (p !== '0' && p === board[r+1][c] && p === board[r+2][c] && p === board[r+3][c]) {
        return { winner: p, line: [{r: r, c: c}, {r: r+1, c: c}, {r: r+2, c: c}, {r: r+3, c: c}] };
      }
    }
  }
  // Diagonal Down-Right
  for (let r = 0; r <= size - 4; r++) {
    for (let c = 0; c <= size - 4; c++) {
      const p = board[r][c];
      if (p !== '0' && p === board[r+1][c+1] && p === board[r+2][c+2] && p === board[r+3][c+3]) {
        return { winner: p, line: [{r: r, c: c}, {r: r+1, c: c+1}, {r: r+2, c: c+2}, {r: r+3, c: c+3}] };
      }
    }
  }
  // Diagonal Down-Left
  for (let r = 0; r <= size - 4; r++) {
    for (let c = 3; c < size; c++) {
      const p = board[r][c];
      if (p !== '0' && p === board[r+1][c-1] && p === board[r+2][c-2] && p === board[r+3][c-3]) {
        return { winner: p, line: [{r: r, c: c}, {r: r+1, c: c-1}, {r: r+2, c: c-2}, {r: r+3, c: c-3}] };
      }
    }
  }
  return null;
}

export function getValidMoves(board: string[][], r: number, c: number) {
  const piece = board[r][c];
  if (piece === '0') return [];
  const moves: {r: number, c: number}[] = [];
  const dirs = [[0, 1], [0, -1], [1, 0], [-1, 0]];
  for (const [dr, dc] of dirs) {
    let currR = r + dr;
    let currC = c + dc;
    let lastValidR = r;
    let lastValidC = c;
    while (currR >= 0 && currR < 6 && currC >= 0 && currC < 6 && board[currR][currC] === '0') {
      lastValidR = currR;
      lastValidC = currC;
      currR += dr;
      currC += dc;
    }
    if (lastValidR !== r || lastValidC !== c) {
      moves.push({r: lastValidR, c: lastValidC});
    }
  }
  return moves;
}

export function calculateEloChange(playerElo: number, opponentElo: number, result: number) {
  const K = 16;
  const expectedScore = 1 / (1 + Math.pow(10, (opponentElo - playerElo) / 400));
  return Math.round(K * (result - expectedScore));
}
