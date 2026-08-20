/*
 * game_logic.cpp — see game_logic.h.
 */
#include "game_logic.h"

#include <cstdlib>
#include <cstring>

void resetGame(GameUiState &game, int selectedVariant)
{
    static const char initialBoard[6][6] = {
        {'B', '0', 'W', 'B', '0', 'W'},
        {'0', '0', '0', '0', '0', '0'},
        {'W', '0', '0', '0', '0', 'B'},
        {'B', '0', '0', '0', '0', 'W'},
        {'0', '0', '0', '0', '0', '0'},
        {'W', '0', 'B', 'W', '0', 'B'}
    };
    memcpy(game.board, initialBoard, sizeof(initialBoard));
    if (selectedVariant == 2)
    {
        memset(game.board, '0', sizeof(game.board));
        int placedWhite = 0;
        int placedBlack = 0;
        while (placedWhite < 6 || placedBlack < 6)
        {
            int row = rand() % 6;
            int col = rand() % 6;
            if (game.board[row][col] != '0') continue;
            if (placedWhite < 6)
            {
                game.board[row][col] = 'W';
                ++placedWhite;
            }
            else
            {
                game.board[row][col] = 'B';
                ++placedBlack;
            }
        }
    }
    game.player = 'W';
    game.turn = 'W';
    game.isOnline = false;
    game.selectedRow = game.selectedCol = -1;
    game.targetRow = game.targetCol = -1;
    game.cursorRow = game.cursorCol = 0;
    game.pieceSelected = false;
    game.confirmMove = false;
    game.gameOver = false;
    game.winner = 0;
    game.flashTimer = 0;
    game.statusMsg = "Local game";
}

bool hasLegalDestination(const GameUiState &game, int r, int c)
{
    static const int dirs[][2] = {{0, 1}, {1, 0}, {0, -1}, {-1, 0}};
    for (const auto &dir : dirs)
    {
        int nr = r + dir[0];
        int nc = c + dir[1];
        bool moved = false;
        while (nr >= 0 && nr < 6 && nc >= 0 && nc < 6 && game.board[nr][nc] == '0')
        {
            moved = true;
            nr += dir[0];
            nc += dir[1];
        }
        if (moved) return true;
    }
    return false;
}

bool chooseFirstDestination(GameUiState &game)
{
    static const int dirs[][2] = {{0, 1}, {1, 0}, {0, -1}, {-1, 0}};
    for (const auto &dir : dirs)
    {
        int nr = game.selectedRow + dir[0];
        int nc = game.selectedCol + dir[1];
        int lastR = game.selectedRow;
        int lastC = game.selectedCol;
        while (nr >= 0 && nr < 6 && nc >= 0 && nc < 6 && game.board[nr][nc] == '0')
        {
            lastR = nr;
            lastC = nc;
            nr += dir[0];
            nc += dir[1];
        }
        if (lastR != game.selectedRow || lastC != game.selectedCol)
        {
            game.targetRow = lastR;
            game.targetCol = lastC;
            return true;
        }
    }
    return false;
}

void selectGamePiece(GameUiState &game, int r, int c)
{
    if (game.gameOver || game.turn != game.player || r < 0 || r >= 6 || c < 0 || c >= 6 || game.board[r][c] != game.player ||
        !hasLegalDestination(game, r, c)) return;
    game.selectedRow = r;
    game.selectedCol = c;
    game.cursorRow = r;
    game.cursorCol = c;
    game.pieceSelected = chooseFirstDestination(game);
    game.confirmMove = false;
    game.statusMsg = "Aim a direction with DPAD or touch";
}

bool moveGameCursor(GameUiState &game, int direction)
{
    if (!game.pieceSelected)
    {
        if (direction == 0) game.cursorCol = (game.cursorCol + 5) % 6;
        if (direction == 1) game.cursorRow = (game.cursorRow + 1) % 6;
        if (direction == 2) game.cursorCol = (game.cursorCol + 1) % 6;
        if (direction == 3) game.cursorRow = (game.cursorRow + 5) % 6;
        return true;
    }
    static const int dirs[][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
    const int dr = dirs[direction][0];
    const int dc = dirs[direction][1];
    int nr = game.selectedRow + dr;
    int nc = game.selectedCol + dc;
    int lastR = game.selectedRow;
    int lastC = game.selectedCol;
    while (nr >= 0 && nr < 6 && nc >= 0 && nc < 6 && game.board[nr][nc] == '0')
    {
        lastR = nr;
        lastC = nc;
        nr += dr;
        nc += dc;
    }
    if (lastR != game.selectedRow || lastC != game.selectedCol)
    {
        game.targetRow = lastR;
        game.targetCol = lastC;
        game.cursorRow = lastR;
        game.cursorCol = lastC;
        return true;
    }
    return false;
}

bool trySetDirectionToCell(GameUiState &game, int r, int c)
{
    const int sr = game.selectedRow;
    const int sc = game.selectedCol;
    if (sr < 0 || sc < 0) return false;
    if (r == sr && c == sc) return false;
    int dr = 0, dc = 0;
    if (r == sr) dc = (c > sc) ? 1 : -1;
    else if (c == sc) dr = (r > sr) ? 1 : -1;
    else return false;

    int lastR = sr, lastC = sc;
    bool reached = false;
    for (int nr = sr + dr, nc = sc + dc;
         nr >= 0 && nr < 6 && nc >= 0 && nc < 6 && game.board[nr][nc] == '0';
         nr += dr, nc += dc)
    {
        if (nr == r && nc == c) reached = true;
        lastR = nr;
        lastC = nc;
    }
    if (!reached) return false;
    game.targetRow = lastR;
    game.targetCol = lastC;
    return true;
}

void applyGameMove(GameUiState &game)
{
    game.board[game.targetRow][game.targetCol] = game.player;
    game.board[game.selectedRow][game.selectedCol] = '0';
    game.player = game.player == 'W' ? 'B' : 'W';
    game.turn = game.player;
    game.selectedRow = game.selectedCol = -1;
    game.targetRow = game.targetCol = -1;
    game.pieceSelected = false;
    game.confirmMove = false;
    game.statusMsg = "Move complete";
}

// Apply the move to the board immediately for instant UI feedback, then flag
// the pending server send. The server's authoritative board replaces this
// once the POST completes (see the moveJob block in the main loop).
void confirmOnlineMove(GameUiState &game, bool &sendPending)
{
    game.board[game.targetRow][game.targetCol] = game.player;
    game.board[game.selectedRow][game.selectedCol] = '0';
    game.statusMsg = "Sending move...";
    game.confirmMove = true;
    sendPending = true;
}
