/*
 * game_render.c
 *
 *  Created on: Mar 20, 2025
 *      Author: Grok3
 */
#include "game_render.h"
#include <stdio.h>   // Для snprintf

// Предыдущие координаты шарика
static uint16_t prev_x = SCREEN_WIDTH / 2;
static uint16_t prev_y = SCREEN_HEIGHT / 2;

void Render_Init(void) {
    ILI9341_FillScreen_DMA(ILI9341_BLACK);
    ILI9341_WriteString_DMA(0, 0, "Ball Game", Font_11x18, ILI9341_WHITE, ILI9341_BLACK);
    ILI9341_FillCircle_DMA(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, BALL_RADIUS, ILI9341_RED);
}


void Render_Frame(const BallState *ball, int16_t accel_x, int16_t accel_y) {
	//сли положение не меняется то не стираем шарик
	if (((int16_t)prev_x != (int16_t)ball->x) || ((int16_t)prev_y != (int16_t)ball->y))
	//if (fabs(prev_x - ball->x) > 0.5f || fabs(prev_y - ball->y) > 0.5f)
	{
		// Стираем старый шарик
		while (HAL_SPI_GetState(&ILI9341_SPI_PORT) != HAL_SPI_STATE_READY);
		ILI9341_FillCircle_DMA(prev_x, prev_y, BALL_RADIUS, ILI9341_BLACK);
		// Рисуем новый шарик
		while (HAL_SPI_GetState(&ILI9341_SPI_PORT) != HAL_SPI_STATE_READY);
		ILI9341_FillCircle_DMA((uint16_t) ball->x, (uint16_t) ball->y, BALL_RADIUS, ILI9341_RED);

	}
	// Обновляем предыдущие координаты
	prev_x = (uint16_t) ball->x;
	prev_y = (uint16_t) ball->y;

	/*
	 // Отображаем данные акселерометра
	 char accel_str[32];
	 snprintf(accel_str, sizeof(accel_str), "AX:%6d AY:%6d", accel_x, accel_y);
	 while (HAL_SPI_GetState(&ILI9341_SPI_PORT) != HAL_SPI_STATE_READY);
	 ILI9341_WriteString_DMA(0, 20, accel_str, Font_11x18, ILI9341_WHITE, ILI9341_BLACK);
	 */
}

