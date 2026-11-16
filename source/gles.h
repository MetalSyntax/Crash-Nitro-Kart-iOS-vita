/**
 * @file gles.h
 * @brief OpenGL ES 1.1 / 2.0 graphics pipeline over vitaGL with SoftFP calling convention.
 */
#pragma once
#include <stdint.h>
#include <vitaGL.h>

/**
 * @brief Initializes the OpenGL ES pipeline and off-screen virtual FBO.
 */
void gles_init(void);

/**
 * @brief Presents current frame: blits and scales iOS FBO to PS Vita 960x544 screen.
 */
void gles_present(void);

/**
 * @brief Returns the total number of draw calls dispatched this session.
 */
uint32_t gles_get_draw_calls(void);

/** @brief Rectángulo destino en pantalla (x, ancho); el táctil lo usa para mapear a 480x320. */
extern int gles_dst_x, gles_dst_w;

/** @brief Textura opaca dibujada a pantalla completa sobre el juego (0 = ninguna). */
extern GLuint gles_overlay_tex;

/** @brief 1 = estirar a 960x544 (pantalla completa), 0 = conservar proporción 3:2. */
void gles_set_stretch(int on);
