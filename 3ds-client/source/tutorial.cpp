/*
 * tutorial.cpp — see tutorial.h.
 */
#include "tutorial.h"
#include "game_logic.h"

#include <cstring>

const TutorialStep TUTORIAL_STEPS[] = {
    { "Hi! I'm the Tutorial Bot. I'll teach you how to play Slide.",
      TutorialAction::None, -1, -1, -1, -1, {-1, -1}, {-1, -1}, 0 },
    { "This is the board - 6 squares wide and 6 squares tall. Everything happens here.",
      TutorialAction::None, -1, -1, -1, -1, {-1, -1}, {-1, -1}, 0 },
    { "Each side starts with 6 pieces. You'll play black; I'll play white.",
      TutorialAction::None, -1, -1, -1, -1, {-1, -1}, {-1, -1}, 0 },
    { "Just like in chess, white always moves first, then we take turns.",
      TutorialAction::None, -1, -1, -1, -1, {-1, -1}, {-1, -1}, 0 },
    { "Pieces slide in a straight line - up, down, left or right - until they hit the edge or another piece. Watch this one go.",
      TutorialAction::BotMove, 0, 2, 4, 2, {4, -1}, {2, -1}, 1 },
    { "The goal is 4 of your pieces in a row - like Connect 4 - in any direction.",
      TutorialAction::None, -1, -1, -1, -1, {-1, -1}, {-1, -1}, 0 },
    { "See these two squares? If I get another turn, I can slide this piece from here to there and win.",
      TutorialAction::None, -1, -1, -1, -1, {3, 3}, {5, 1}, 2 },
    { "Your turn - select this piece to stop me.",
      TutorialAction::UserMove, 3, 0, 3, 4, {3, -1}, {0, -1}, 1 },
    { "Now send it here, right next to my piece, so I can't jump over it.",
      TutorialAction::UserMove, 3, 0, 3, 4, {3, -1}, {4, -1}, 1 },
    { "Nice block! Now watch - I'll make my move.",
      TutorialAction::BotMove, 4, 2, 4, 0, {-1, -1}, {-1, -1}, 0 },
    { "Your turn again. Take a look - is there a move that wins the game for you?",
      TutorialAction::UserWin, -1, -1, -1, -1, {0, -1}, {3, -1}, 1 },
    { "That's four in a row - you win! You're ready to play for real. Good luck out there!",
      TutorialAction::None, -1, -1, -1, -1, {-1, -1}, {-1, -1}, 0 },
};
const int TUTORIAL_STEP_COUNT = sizeof(TUTORIAL_STEPS) / sizeof(TUTORIAL_STEPS[0]);

namespace
{
    // Re-shares out the step's stated text and re-arms selection/turn state
    // for whatever it needs next. Player is always black (who the learner
    // plays); `turn` doubles as the interactivity gate `selectGamePiece`
    // already checks, so narration/bot-move steps simply aren't the
    // player's turn.
    void syncStepState(GameUiState &t, int stepIndex)
    {
        const TutorialStep &step = TUTORIAL_STEPS[stepIndex];
        const bool userTurn = step.action == TutorialAction::UserMove ||
                              step.action == TutorialAction::UserWin;
        t.player = 'B';
        t.turn   = userTurn ? 'B' : 'W';
        t.selectedRow = t.selectedCol = -1;
        t.targetRow = t.targetCol = -1;
        t.cursorRow = t.cursorCol = 0;
        t.pieceSelected = false;
        t.confirmMove = false;
        t.flashTimer = 0;
        t.statusMsg = step.text;
    }
}

void tutorialReset(GameUiState &t, int &stepIndex)
{
    resetGame(t);
    stepIndex = 0;
    syncStepState(t, stepIndex);
}

bool tutorialAdvance(GameUiState &t, int &stepIndex)
{
    if (stepIndex >= TUTORIAL_STEP_COUNT - 1) return true;

    const TutorialStep &step = TUTORIAL_STEPS[stepIndex];
    if (step.action == TutorialAction::BotMove)
    {
        t.board[step.toR][step.toC] = t.board[step.fromR][step.fromC];
        t.board[step.fromR][step.fromC] = '0';
    }
    ++stepIndex;
    syncStepState(t, stepIndex);
    return false;
}

void tutorialSelectCell(GameUiState &t, int stepIndex, int r, int c)
{
    const TutorialStep &step = TUTORIAL_STEPS[stepIndex];
    if (step.action == TutorialAction::UserMove && (r != step.fromR || c != step.fromC))
        return; // only the piece being taught may be picked up
    if (step.action != TutorialAction::UserMove && step.action != TutorialAction::UserWin)
        return;
    selectGamePiece(t, r, c);
    // selectGamePiece sets its own generic status line; keep the bot's actual
    // line on screen instead (the D-pad/A/B hints cover the "how" already).
    t.statusMsg = step.text;
}

bool tutorialConfirmMove(GameUiState &t, int &stepIndex)
{
    const TutorialStep &step = TUTORIAL_STEPS[stepIndex];

    if (step.action == TutorialAction::UserMove)
    {
        if (t.targetRow != step.toR || t.targetCol != step.toC)
        {
            // Aimed somewhere other than the taught square - let them try a
            // different direction instead of silently accepting it.
            t.pieceSelected = false;
            t.selectedRow = t.selectedCol = -1;
            t.targetRow = t.targetCol = -1;
            t.flashTimer = 6;
            return false;
        }

        t.board[t.targetRow][t.targetCol] = t.board[t.selectedRow][t.selectedCol];
        t.board[t.selectedRow][t.selectedCol] = '0';
        if (stepIndex >= TUTORIAL_STEP_COUNT - 1) return true;
        ++stepIndex;
        syncStepState(t, stepIndex);
        return false;
    }

    if (step.action == TutorialAction::UserWin)
    {
        char testBoard[6][6];
        memcpy(testBoard, t.board, sizeof(testBoard));
        testBoard[t.targetRow][t.targetCol] = testBoard[t.selectedRow][t.selectedCol];
        testBoard[t.selectedRow][t.selectedCol] = '0';

        char winner = 0;
        int wr[4], wc[4];
        if (checkWin(testBoard, winner, wr, wc) && winner == 'B')
        {
            memcpy(t.board, testBoard, sizeof(testBoard));
            t.pieceSelected = false;
            t.selectedRow = t.selectedCol = -1;
            t.targetRow = t.targetCol = -1;
            if (stepIndex >= TUTORIAL_STEP_COUNT - 1) return true;
            ++stepIndex;
            syncStepState(t, stepIndex);
            return false;
        }

        // A legal move, but not a winning one - revert it and let them
        // pick again rather than leaving a wrong move on the board.
        t.pieceSelected = false;
        t.selectedRow = t.selectedCol = -1;
        t.targetRow = t.targetCol = -1;
        t.flashTimer = 6;
        t.statusMsg = "That doesn't win the game - try a different move!";
        return false;
    }

    return false;
}
