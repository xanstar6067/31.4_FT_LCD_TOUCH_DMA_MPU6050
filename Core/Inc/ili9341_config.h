/*
 * ili9341_config.h
 *
 *  Created on: Mar 18, 2025
 *      Author: pro
 */

#ifndef INC_ILI9341_CONFIG_H_
#define INC_ILI9341_CONFIG_H_

// Определения ориентации экрана (совмещаем с MADCTL)
#define ILI9341_ORIENTATION_PORTRAIT        (ILI9341_MADCTL_MX | ILI9341_MADCTL_BGR)         				// 0°, верх слева, 240x320
#define ILI9341_ORIENTATION_PORTRAIT_UPSIDE (ILI9341_MADCTL_MY | ILI9341_MADCTL_BGR)         				// 180°, верх справа, 240x320
#define ILI9341_ORIENTATION_LANDSCAPE       (ILI9341_MADCTL_MV | ILI9341_MADCTL_MX | ILI9341_MADCTL_BGR) 	// 90°, верх справа, 320x240
#define ILI9341_ORIENTATION_LANDSCAPE_LEFT  (ILI9341_MADCTL_MV | ILI9341_MADCTL_BGR)        				// 270°, верх слева, 320x240

// Глобальные переменные для ориентации и размеров
extern uint8_t ili9341_orientation;       // Текущая ориентация
extern uint16_t ILI9341_SCREEN_WIDTH;     // Ширина экрана
extern uint16_t ILI9341_SCREEN_HEIGHT;    // Высота экрана
extern uint16_t ILI9341_TOUCH_SCALE_X;    // Масштаб тачскрина по X
extern uint16_t ILI9341_TOUCH_SCALE_Y;    // Масштаб тачскрина по Y

// Калибровка тачскрина (первая рабочая версия)
#define ILI9341_TOUCH_MIN_RAW_X 2000  // Минимальное сырое значение X
#define ILI9341_TOUCH_MAX_RAW_X 30000 // Максимальное сырое значение X
#define ILI9341_TOUCH_MIN_RAW_Y 2800  // Минимальное сырое значение Y
#define ILI9341_TOUCH_MAX_RAW_Y 30500 // Максимальное сырое значение Y


// Старые версии калибровки (сохранены для тебя)
/*
// Заводская (по умолчанию)
#define ILI9341_TOUCH_MIN_RAW_X 1500
#define ILI9341_TOUCH_MAX_RAW_X 31000
#define ILI9341_TOUCH_MIN_RAW_Y 3276
#define ILI9341_TOUCH_MAX_RAW_Y 30110
*/
/*
// Вторая с помощью ИИ (оказалась неудачной)
#define ILI9341_TOUCH_MIN_RAW_X 2300  // Минимум ~2343, с небольшим запасом
#define ILI9341_TOUCH_MAX_RAW_X 31000 // Максимум ~30710, с запасом
#define ILI9341_TOUCH_MIN_RAW_Y 2700  // Минимум ~2800, с запасом
#define ILI9341_TOUCH_MAX_RAW_Y 30500 // Максимум ~30500, оставляем как есть
*/

#endif /* INC_ILI9341_CONFIG_H_ */
