import { TestSuite, assertEquals, assert } from "./testUtils";
import { checkWin, getValidMoves, calculateEloChange } from "../gameLogic";

export async function runGameLogicTests() {
  const suite = new TestSuite();

  await suite.test("Elo Calculation: Basic Win", () => {
    const change = calculateEloChange(1000, 1000, 1);
    assertEquals(change, 8, "Expected K/2 = 8 for equal matched players");
  });

  await suite.test("Elo Calculation: Win against strong player", () => {
    const change = calculateEloChange(1000, 1500, 1);
    assert(change > 8, "Winner should gain more than 8 points against stronger opponent");
  });

  await suite.test("Win Check: Horizontal", () => {
    const board = [
      ['W', 'W', 'W', 'W', '0', '0'],
      ['0', '0', '0', '0', '0', '0'],
      ['0', '0', '0', '0', '0', '0'],
      ['0', '0', '0', '0', '0', '0'],
      ['0', '0', '0', '0', '0', '0'],
      ['0', '0', '0', '0', '0', '0'],
    ];
    const win = checkWin(board);
    assert(win !== null, "Should detect win");
    assertEquals(win?.winner, 'W');
  });

  await suite.test("Win Check: Vertical", () => {
    const board = [
      ['B', '0', '0', '0', '0', '0'],
      ['B', '0', '0', '0', '0', '0'],
      ['B', '0', '0', '0', '0', '0'],
      ['B', '0', '0', '0', '0', '0'],
      ['0', '0', '0', '0', '0', '0'],
      ['0', '0', '0', '0', '0', '0'],
    ];
    const win = checkWin(board);
    assert(win !== null, "Should detect win");
    assertEquals(win?.winner, 'B');
  });

  await suite.test("Valid Moves: Sliding to wall", () => {
    const board = [
      ['W', '0', '0', '0', '0', '0'],
      ['0', '0', '0', '0', '0', '0'],
      ['0', '0', '0', '0', '0', '0'],
      ['0', '0', '0', '0', '0', '0'],
      ['0', '0', '0', '0', '0', '0'],
      ['0', '0', '0', '0', '0', '0'],
    ];
    const moves = getValidMoves(board, 0, 0);
    // Right: (0,5), Down: (5,0)
    assertEquals(moves.length, 2);
    assert(moves.some(m => m.r === 0 && m.c === 5), "Should be able to slide to right edge");
    assert(moves.some(m => m.r === 5 && m.c === 0), "Should be able to slide to bottom edge");
  });

  await suite.test("Valid Moves: Blocked by piece", () => {
    const board = [
      ['W', '0', 'B', '0', '0', '0'],
      ['0', '0', '0', '0', '0', '0'],
      ['0', '0', '0', '0', '0', '0'],
      ['0', '0', '0', '0', '0', '0'],
      ['0', '0', '0', '0', '0', '0'],
      ['0', '0', '0', '0', '0', '0'],
    ];
    const moves = getValidMoves(board, 0, 0);
    // Right should be (0,1) because (0,2) is blocked
    assert(moves.some(m => m.r === 0 && m.c === 1), "Should stop before another piece");
    assert(!moves.some(m => m.r === 0 && m.c === 5), "Should not be able to jump over piece");
  });

  suite.report();
}
