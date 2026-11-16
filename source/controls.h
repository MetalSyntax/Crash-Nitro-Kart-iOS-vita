/**
 * @file controls.h
 * @brief Remappable controls + in-game "PS Vita controls" menu (Start+Select),
 *        same UX as the Zenonia 2 / Carnivores Vita ports.
 */
#pragma once
#include <stdint.h>
#include <vitaGL.h>

/** Physical buttons -> virtual touch at (x, y) in the game's 480x320 landscape space. */
typedef struct {
    const char *key;   /**< config.txt name */
    const char *label; /**< menu label */
    int x, y;          /**< virtual touch position */
    uint32_t def;      /**< default SCE_CTRL_* mask */
    uint32_t mask;     /**< current binding */
    int down;          /**< virtual touch currently held */
} button_action;

enum { ACT_GAS, ACT_BRAKE, ACT_ITEM, ACT_PAUSE, ACT_COUNT };
extern button_action actions[ACT_COUNT];

extern int steer_invert;   /**< 1 = flip left stick steering */
extern int steer_deadzone; /**< left stick deadzone (0..127) */

void controls_init(void); /**< load ux0:data/crashnitro1/config.txt (or write defaults) */

int controls_menu_active(void);
/** Feeds this frame's pad buttons; opens the menu on Start+Select. Returns 1 while open. */
int controls_menu_update(uint32_t buttons);
/** Texture with the menu rendered (480x272, drawn fullscreen over the game). */
GLuint controls_menu_texture(void);
