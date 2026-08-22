/*
 * game_logic.h — Slide board rules: setup, legal-move checks, cursor/piece
 * selection, and move application. Pure logic over GameUiState — no
 * rendering, no networking.
 */
#ifndef GAME_LOGIC_H
#define GAME_LOGIC_H

#include "ui.h"

// Resets `game` to a fresh local match. selectedVariant follows the lobby's
// variant indices (2 = random setup); all others use the standard start.
void resetGame(GameUiState &game, int selectedVariant = 0);

// True if the piece at (r, c) has at least one open sliding direction.
bool hasLegalDestination(const GameUiState &game, int r, int c);

// Picks the first available slide direction from the selected piece and
// sets targetRow/targetCol to the far end of that line.
bool chooseFirstDestination(GameUiState &game);

// Selects the piece at (r, c) as the active piece for the current player,
// if it belongs to them and has a legal move.
void selectGamePiece(GameUiState &game, int r, int c);

// Moves the cursor one step in `direction` (0=left,1=down,2=right,3=up).
// Before a piece is selected this pans a free cursor; after selection it
// walks the sliding line, snapping the target to the far reachable cell.
// Returns false (and the caller should flash) if the move had no effect.
bool moveGameCursor(GameUiState &game, int direction);

// If (r, c) lies along an open line from the selected piece, aims the move
// in that direction (target = far end) and returns true.
bool trySetDirectionToCell(GameUiState &game, int r, int c);

// Applies the currently targeted move to the board for a local (offline) game
// and advances turn/player.
void applyGameMove(GameUiState &game);

// Applies the currently targeted move optimistically for an online game and
// flags `sendPending` so the caller submits it to the server.
void confirmOnlineMove(GameUiState &game, bool &sendPending);

// Checks the board for 4-in-a-row (any of the 4 directions). On a win,
// returns true, writes the winning colour to `winner`, and fills outR/outC
// with the 4 cells of the line. Online games get their result from the
// server instead; the tutorial is the one local caller that needs this.
bool checkWin(const char board[6][6], char &winner, int outR[4], int outC[4]);

#endif // GAME_LOGIC_H
