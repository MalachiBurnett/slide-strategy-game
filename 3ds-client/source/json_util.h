/*
 * json_util.h — Tiny JSON field extractor for flat API responses.
 * No heap allocation, no full parser — just enough to read the handful of
 * scalar/string fields and one fixed-shape board array the server sends.
 */
#ifndef JSON_UTIL_H
#define JSON_UTIL_H

// Extracts the string or scalar value for `key` in a flat JSON object.
// Returns true and fills outVal[maxVal] (null-terminated) on success.
bool jsonExtract(const char *json, const char *key, char *outVal, int maxVal);

// Extracts the 6x6 "board" array of 'W'/'B'/'0' cells from a game-state
// JSON response. Returns true only if all 36 cells were found.
bool parseBoard(const char *json, char board[6][6]);

#endif // JSON_UTIL_H
