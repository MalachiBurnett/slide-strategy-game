/*
 * auth_store.h — Save/load the persistent auth code on the SD card.
 */
#pragma once

static constexpr const char *AUTHCODE_PATH = "/3ds/slide/authcode.txt";
static constexpr int          AUTHCODE_LEN  = 32;

// Fill `out` (must be ≥ AUTHCODE_LEN+1 bytes) with the stored code.
// Returns true only if a valid 32-char code was found.
bool loadAuthCode(char *out);

// Write `code` to the SD card (creates /3ds/slide/ if needed).
bool saveAuthCode(const char *code);

// Remove the stored auth code (e.g. on logout).
void deleteAuthCode();
