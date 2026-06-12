/*
 * game_render.h
 *
 *  Created on: Mar 20, 2025
 *      Author: Grok3
 */
#ifndef INC_GAME_RENDER_H_
#define INC_GAME_RENDER_H_

#include "game_logic.h"
#include <stdint.h>  // Для int16_t
#include "ili9341.h" //Драйвер дисплея

#define BALL_BUFFER_SIZE 32

// Функции рендера
void Render_Init(void);
void Render_Frame(const BallState* ball, int16_t accel_x, int16_t accel_y);

#endif /* INC_GAME_RENDER_H_ */
