/*
 * game_logic.c
 *
 *  Created on: Mar 20, 2025
 *      Author: Grok3
 */

#include "game_logic.h"

void Game_Init(BallState* ball) {
    ball->x = SCREEN_WIDTH / 2.0f;  // Начальная позиция в центре
    ball->y = SCREEN_HEIGHT / 2.0f;
    ball->vx = 0.0f;                // Начальная скорость 0
    ball->vy = 0.0f;
}

void Game_Update(BallState* ball, float accel_x, float accel_y) {
    // Применяем ускорение от акселерометра
    ball->vx += accel_x * GRAVITY_SCALE;
    ball->vy += accel_y * GRAVITY_SCALE;

    // Применяем трение
    ball->vx *= FRICTION;
    ball->vy *= FRICTION;

    // Обновляем позицию
    ball->x += ball->vx;
    ball->y += ball->vy;

    // Проверка границ и отскок
    if (ball->x - BALL_RADIUS < 0) {
        ball->x = BALL_RADIUS;
        ball->vx = -ball->vx * BOUNCE_DAMPING;
    }
    if (ball->x + BALL_RADIUS > SCREEN_WIDTH) {
        ball->x = SCREEN_WIDTH - BALL_RADIUS;
        ball->vx = -ball->vx * BOUNCE_DAMPING;
    }
    if (ball->y - BALL_RADIUS < 0) {
        ball->y = BALL_RADIUS;
        ball->vy = -ball->vy * BOUNCE_DAMPING;
    }
    if (ball->y + BALL_RADIUS > SCREEN_HEIGHT) {
        ball->y = SCREEN_HEIGHT - BALL_RADIUS;
        ball->vy = -ball->vy * BOUNCE_DAMPING;
    }
}

