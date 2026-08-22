/*
 * tutorial.h — the scripted "learn to play" match: a fixed sequence of steps
 * that narrate a rule, then (for the interactive ones) wait for the player to
 * carry it out on the board before moving on.
 *
 * This reuses GameUiState/game_logic.cpp for board storage and the
 * select-piece/aim-direction/confirm flow the real game already has — a step
 * only adds script-specific gating (which piece may be picked up, whether a
 * move actually matches what was taught) on top of that.
 */
#ifndef TUTORIAL_H
#define TUTORIAL_H

#include "ui.h"

enum class TutorialAction { None, BotMove, UserMove, UserWin };

struct TutorialStep
{
    const char    *text;
    TutorialAction action;
    int fromR, fromC, toR, toC;    // scripted move; -1 when the step has none
    int hlR[2], hlC[2], hlCount;   // squares to ring for guidance (0-2)
};

extern const TutorialStep TUTORIAL_STEPS[];
extern const int          TUTORIAL_STEP_COUNT;

// Sets up the same opening position as a real match and shows step 0.
void tutorialReset(GameUiState &t, int &stepIndex);

// "Next" was pressed on a narration or bot-move step: plays the scripted
// move (if any), then advances. Returns true once the last step has already
// been acknowledged, telling the caller to leave the tutorial.
bool tutorialAdvance(GameUiState &t, int &stepIndex);

// A board cell was tapped or cursor-confirmed while a step is waiting on the
// player. Delegates to game_logic's selectGamePiece, but only for the piece
// the current step is teaching (or, during the win challenge, any of the
// player's own pieces).
void tutorialSelectCell(GameUiState &t, int stepIndex, int r, int c);

// The player confirmed the aimed move (A, or tapping the resolved target).
// Validates it against the step before committing:
//   - UserMove: only the taught destination is accepted; anything else
//     bounces the cursor back so they can aim again.
//   - UserWin: any move is allowed, but only advances if it actually makes
//     four in a row for black; otherwise it reverts and flashes.
// Returns true once the last step has already been acknowledged, telling the
// caller to leave the tutorial.
bool tutorialConfirmMove(GameUiState &t, int &stepIndex);

#endif // TUTORIAL_H
