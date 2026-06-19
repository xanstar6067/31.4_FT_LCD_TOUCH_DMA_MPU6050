#include "stm32f4xx_hal.h"
#include "main.h"
#include "display_driver.h"

#define READ_X 0xD0
#define READ_Y 0x90

/* Shared XPT2046 reader; the active touch wrapper is selected in display_driver.h. */
#if defined(__ST7789_TOUCH_H__)
#define TOUCH_SELECT           ST7789_TouchSelect
#define TOUCH_UNSELECT         ST7789_TouchUnselect
#define TOUCH_PRESSED          ST7789_TouchPressed
#define TOUCH_GET_COORDINATES  ST7789_TouchGetCoordinates
#define TOUCH_SPI_PORT         ST7789_TOUCH_SPI_PORT
#define TOUCH_CS_GPIO_Port     ST7789_TOUCH_CS_GPIO_Port
#define TOUCH_CS_Pin           ST7789_TOUCH_CS_Pin
#define TOUCH_IRQ_GPIO_Port    ST7789_TOUCH_IRQ_GPIO_Port
#define TOUCH_IRQ_Pin          ST7789_TOUCH_IRQ_Pin
#define TOUCH_RAW_X            raw_x
#define TOUCH_RAW_Y            raw_y
#define TOUCH_X                touch_x
#define TOUCH_Y                touch_y
#define TOUCH_UPDATE_COUNTER   update_counter
#define TOUCH_MIRROR_COORDS    1U

uint32_t raw_x = 0, raw_y = 0;
uint16_t touch_x = 0, touch_y = 0;
uint16_t update_counter = 0;
#elif defined(__ILI9341_TOUCH_H__)
#define TOUCH_SELECT           ILI9341_TouchSelect
#define TOUCH_UNSELECT         ILI9341_TouchUnselect
#define TOUCH_PRESSED          ILI9341_TouchPressed
#define TOUCH_GET_COORDINATES  ILI9341_TouchGetCoordinates
#define TOUCH_SPI_PORT         ILI9341_TOUCH_SPI_PORT
#define TOUCH_CS_GPIO_Port     ILI9341_TOUCH_CS_GPIO_Port
#define TOUCH_CS_Pin           ILI9341_TOUCH_CS_Pin
#define TOUCH_IRQ_GPIO_Port    ILI9341_TOUCH_IRQ_GPIO_Port
#define TOUCH_IRQ_Pin          ILI9341_TOUCH_IRQ_Pin
#define TOUCH_RAW_X            raw_x
#define TOUCH_RAW_Y            raw_y
#define TOUCH_X                touch_x
#define TOUCH_Y                touch_y
#define TOUCH_UPDATE_COUNTER   update_counter
#define TOUCH_MIRROR_COORDS    0U

uint32_t raw_x = 0, raw_y = 0;
uint16_t touch_x = 0, touch_y = 0;
uint16_t update_counter = 0;
#else
#error "No active touch driver selected in display_driver.h"
#endif

static void TOUCH_SELECT(void) {
    HAL_GPIO_WritePin(TOUCH_CS_GPIO_Port, TOUCH_CS_Pin, GPIO_PIN_RESET);
}

void TOUCH_UNSELECT(void) {
    HAL_GPIO_WritePin(TOUCH_CS_GPIO_Port, TOUCH_CS_Pin, GPIO_PIN_SET);
}

bool TOUCH_PRESSED(void) {
    return HAL_GPIO_ReadPin(TOUCH_IRQ_GPIO_Port, TOUCH_IRQ_Pin) ==
           GPIO_PIN_RESET;
}

#if TOUCH_MIRROR_COORDS
static uint16_t TouchMirror(uint16_t value, uint16_t scale) {
    if (scale == 0U) {
        return 0U;
    }
    if (value >= scale) {
        value = (uint16_t)(scale - 1U);
    }
    return (uint16_t)((scale - 1U) - value);
}
#endif

bool TOUCH_GET_COORDINATES(uint16_t* x, uint16_t* y) {
    static const uint8_t cmd_read_x[] = { READ_X };
    static const uint8_t cmd_read_y[] = { READ_Y };
    static const uint8_t zeroes_tx[] = { 0x00, 0x00 };
    uint32_t avg_x = 0U;
    uint32_t avg_y = 0U;

    TOUCH_SELECT();

    for (uint8_t i = 0U; i < 16U; i++) {
        uint8_t y_raw[2];
        uint8_t x_raw[2];

        HAL_SPI_Transmit(&TOUCH_SPI_PORT,
                         (uint8_t*)cmd_read_y,
                         sizeof(cmd_read_y),
                         HAL_MAX_DELAY);
        HAL_SPI_TransmitReceive(&TOUCH_SPI_PORT,
                                (uint8_t*)zeroes_tx,
                                y_raw,
                                sizeof(y_raw),
                                HAL_MAX_DELAY);

        HAL_SPI_Transmit(&TOUCH_SPI_PORT,
                         (uint8_t*)cmd_read_x,
                         sizeof(cmd_read_x),
                         HAL_MAX_DELAY);
        HAL_SPI_TransmitReceive(&TOUCH_SPI_PORT,
                                (uint8_t*)zeroes_tx,
                                x_raw,
                                sizeof(x_raw),
                                HAL_MAX_DELAY);

        avg_x += (((uint16_t)x_raw[0]) << 8) | ((uint16_t)x_raw[1]);
        avg_y += (((uint16_t)y_raw[0]) << 8) | ((uint16_t)y_raw[1]);
    }

    TOUCH_UNSELECT();

    avg_x /= 16U;
    avg_y /= 16U;

    TOUCH_RAW_X = avg_x;
    TOUCH_RAW_Y = avg_y;

    if (avg_x < ILI9341_TOUCH_MIN_RAW_X) avg_x = ILI9341_TOUCH_MIN_RAW_X;
    if (avg_x > ILI9341_TOUCH_MAX_RAW_X) avg_x = ILI9341_TOUCH_MAX_RAW_X;
    if (avg_y < ILI9341_TOUCH_MIN_RAW_Y) avg_y = ILI9341_TOUCH_MIN_RAW_Y;
    if (avg_y > ILI9341_TOUCH_MAX_RAW_Y) avg_y = ILI9341_TOUCH_MAX_RAW_Y;

    if (ili9341_orientation == ILI9341_ORIENTATION_LANDSCAPE) {
        *x = ILI9341_TOUCH_SCALE_X -
             ((avg_y - ILI9341_TOUCH_MIN_RAW_Y) *
              ILI9341_TOUCH_SCALE_X /
              (ILI9341_TOUCH_MAX_RAW_Y - ILI9341_TOUCH_MIN_RAW_Y));
        *y = (avg_x - ILI9341_TOUCH_MIN_RAW_X) *
             ILI9341_TOUCH_SCALE_Y /
             (ILI9341_TOUCH_MAX_RAW_X - ILI9341_TOUCH_MIN_RAW_X);
    } else if (ili9341_orientation == ILI9341_ORIENTATION_LANDSCAPE_LEFT) {
        *x = ILI9341_TOUCH_SCALE_X -
             ((avg_y - ILI9341_TOUCH_MIN_RAW_Y) *
              ILI9341_TOUCH_SCALE_X /
              (ILI9341_TOUCH_MAX_RAW_Y - ILI9341_TOUCH_MIN_RAW_Y));
        *y = ILI9341_TOUCH_SCALE_Y -
             ((avg_x - ILI9341_TOUCH_MIN_RAW_X) *
              ILI9341_TOUCH_SCALE_Y /
              (ILI9341_TOUCH_MAX_RAW_X - ILI9341_TOUCH_MIN_RAW_X));
    } else if (ili9341_orientation == ILI9341_ORIENTATION_PORTRAIT) {
        *x = (avg_x - ILI9341_TOUCH_MIN_RAW_X) *
             ILI9341_TOUCH_SCALE_X /
             (ILI9341_TOUCH_MAX_RAW_X - ILI9341_TOUCH_MIN_RAW_X);
        *y = ILI9341_TOUCH_SCALE_Y -
             ((avg_y - ILI9341_TOUCH_MIN_RAW_Y) *
              ILI9341_TOUCH_SCALE_Y /
              (ILI9341_TOUCH_MAX_RAW_Y - ILI9341_TOUCH_MIN_RAW_Y));
    } else if (ili9341_orientation == ILI9341_ORIENTATION_PORTRAIT_UPSIDE) {
        *x = ILI9341_TOUCH_SCALE_X -
             ((avg_x - ILI9341_TOUCH_MIN_RAW_X) *
              ILI9341_TOUCH_SCALE_X /
              (ILI9341_TOUCH_MAX_RAW_X - ILI9341_TOUCH_MIN_RAW_X));
        *y = (avg_y - ILI9341_TOUCH_MIN_RAW_Y) *
             ILI9341_TOUCH_SCALE_Y /
             (ILI9341_TOUCH_MAX_RAW_Y - ILI9341_TOUCH_MIN_RAW_Y);
    }

#if TOUCH_MIRROR_COORDS
    *x = TouchMirror(*x, ILI9341_TOUCH_SCALE_X);
    *y = TouchMirror(*y, ILI9341_TOUCH_SCALE_Y);
#endif

    TOUCH_X = *x;
    TOUCH_Y = *y;

    return true;
}
