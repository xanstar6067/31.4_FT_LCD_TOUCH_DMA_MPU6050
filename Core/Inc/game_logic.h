/*
 * game_logic.h
 *
 *  Created on: Mar 20, 2025
 *      Author: Grok3
 */

#ifndef INC_GAME_LOGIC_H_
#define INC_GAME_LOGIC_H_

// Настраиваемые параметры физики шарика
#define BALL_RADIUS         15      // Радиус шарика в пикселях
#define GRAVITY_SCALE       0.001f  // Масштаб ускорения от акселерометра (меньше = менее отзывчивый)
#define FRICTION            0.92f   // Коэффициент трения (0.0 = полная остановка, 1.0 = нет трения)
#define BOUNCE_DAMPING      0.8f    // Затухание скорости при отскоке от стенок (0.0 = остановка, 1.0 = без потерь)
#define SCREEN_WIDTH        320     // Ширина экрана в пикселях
#define SCREEN_HEIGHT       240     // Высота экрана в пикселях

// Структура состояния шарика
typedef struct {
    float x;         // Позиция X
    float y;         // Позиция Y
    float vx;        // Скорость по X
    float vy;        // Скорость по Y
} BallState;

// Функции логики игры
void Game_Init(BallState* ball);
void Game_Update(BallState* ball, float accel_x, float accel_y);

#endif /* INC_GAME_LOGIC_H_ */
