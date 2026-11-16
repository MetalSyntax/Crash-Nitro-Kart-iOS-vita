/**
 * @file patch.h
 * @brief Game-specific hooks, memory patches, and per-frame logic callbacks.
 */
#pragma once
#include "imports.h"

/**
 * @brief Initializes game-specific memory patches, trampolines, and binary hooks.
 */
void patch_init(void);

/**
 * @brief Starts the Polarbit Fuse 3D engine (game_init).
 */
void patch_start_game(void);

/**
 * @brief Per-frame callback invoked from the main loop for custom game logic.
 */
void patch_frame(void);
