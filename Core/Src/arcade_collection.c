#include "arcade_collection.h"

#if ARCADE_ENABLE_BALANCE_TOWER
#include "balance_tower_render.h"
#endif
#if ARCADE_ENABLE_BIPLANE_DUEL
#include "biplane_duel_render.h"
#endif
#include "comet_catch_render.h"
#include "display_driver.h"
#include "gravity_pong_render.h"
#include "life_game_render.h"
#include "marble_maze_render.h"
#include "micro_colony_render.h"
#include "orb_hunt_render.h"
#include "render_scratch.h"
#include "shake_flight_render.h"
#include "space_dodger_render.h"
#include "tetris_render.h"
#include "tilt_breaker_render.h"

#include <string.h>

uint8_t render_scratch_buffer[RENDER_SCRATCH_BUFFER_SIZE]
    __attribute__((aligned(4)));

#define ARCADE_MENU_SCREEN_W          320U
#define ARCADE_MENU_HEADER_H          36U
#define ARCADE_MENU_TAB_Y             42U
#define ARCADE_MENU_TAB_H             30U
#define ARCADE_MENU_TAB_W             148U
#define ARCADE_MENU_TEST_TAB_X        8U
#define ARCADE_MENU_GAME_TAB_X        164U
#define ARCADE_MENU_LIST_X            8U
#define ARCADE_MENU_LIST_Y            78U
#define ARCADE_MENU_LIST_W            286U
#define ARCADE_MENU_ITEM_H            32U
#define ARCADE_MENU_VISIBLE_ITEMS     4U
#define ARCADE_MENU_LIST_H            (ARCADE_MENU_ITEM_H * \
                                      ARCADE_MENU_VISIBLE_ITEMS)
#define ARCADE_MENU_SCROLL_X          302U
#define ARCADE_MENU_SCROLL_W          10U
#define ARCADE_MENU_TAP_SLOP          14U
#define ARCADE_MENU_DRAG_STEP         18U

#define ARCADE_MENU_BG                DISPLAY_COLOR565(5, 9, 15)
#define ARCADE_MENU_HEADER            DISPLAY_COLOR565(10, 24, 36)
#define ARCADE_MENU_PANEL             DISPLAY_COLOR565(13, 21, 30)
#define ARCADE_MENU_PANEL_ALT         DISPLAY_COLOR565(17, 27, 37)
#define ARCADE_MENU_ACCENT            DISPLAY_COLOR565(35, 151, 164)
#define ARCADE_MENU_ACCENT_DARK       DISPLAY_COLOR565(18, 78, 90)
#define ARCADE_MENU_BORDER            DISPLAY_COLOR565(75, 105, 122)
#define ARCADE_MENU_SCROLL_TRACK      DISPLAY_COLOR565(20, 32, 42)
#define ARCADE_MENU_MODAL_BG          DISPLAY_COLOR565(21, 28, 36)
#define ARCADE_MENU_MODAL_SHADOW      DISPLAY_COLOR565(1, 3, 6)
#define ARCADE_MENU_WARN              DISPLAY_COLOR565(245, 183, 74)

#define ARCADE_MENU_MODAL_X           32U
#define ARCADE_MENU_MODAL_Y           50U
#define ARCADE_MENU_MODAL_W           256U
#define ARCADE_MENU_MODAL_H           140U
#define ARCADE_MENU_NO_X              48U
#define ARCADE_MENU_YES_X             176U
#define ARCADE_MENU_CONFIRM_BUTTON_Y  148U
#define ARCADE_MENU_CONFIRM_BUTTON_W  96U
#define ARCADE_MENU_CONFIRM_BUTTON_H  34U

typedef enum {
    ARCADE_MENU_TOUCH_NONE = 0,
    ARCADE_MENU_TOUCH_TESTS_TAB,
    ARCADE_MENU_TOUCH_GAMES_TAB,
    ARCADE_MENU_TOUCH_LIST,
    ARCADE_MENU_TOUCH_SCROLLBAR,
    ARCADE_MENU_TOUCH_CONFIRM_NO,
    ARCADE_MENU_TOUCH_CONFIRM_YES
} ArcadeMenuTouchRegion;

typedef struct {
    const char *label;
    ArcadePageId page;
} ArcadeMenuItem;

static const ArcadeMenuItem arcade_menu_tests[] = {
    { "SYSTEM INFO", ARCADE_PAGE_SYSTEM_INFO },
    { "MPU6000", ARCADE_PAGE_MPU6000 },
    { "TOUCH TEST", ARCADE_PAGE_TOUCH_TEST }
};

static const ArcadeMenuItem arcade_menu_games[] = {
    { "COMET CATCH", ARCADE_PAGE_COMET_CATCH },
    { "ORB HUNT", ARCADE_PAGE_ORB_HUNT },
    { "GRAVITY PONG", ARCADE_PAGE_GRAVITY_PONG },
    { "TILT BREAKER", ARCADE_PAGE_TILT_BREAKER },
    { "MARBLE MAZE", ARCADE_PAGE_MARBLE_MAZE },
    { "SPACE DODGER", ARCADE_PAGE_SPACE_DODGER },
#if ARCADE_ENABLE_BALANCE_TOWER
    { "BALANCE TOWER", ARCADE_PAGE_BALANCE_TOWER },
#endif
    { "SHAKE FLIGHT", ARCADE_PAGE_SHAKE_FLIGHT },
#if ARCADE_ENABLE_BIPLANE_DUEL
    { "BIPLANE DUEL", ARCADE_PAGE_BIPLANE_DUEL },
#endif
    { "LIFE GAME", ARCADE_PAGE_LIFE_GAME },
    { "MICRO COLONY", ARCADE_PAGE_MICRO_COLONY },
    { "TETRIS", ARCADE_PAGE_TETRIS }
};

static const ArcadeMenuItem *ArcadeMenu_Items(
    ArcadeMenuCategory category,
    uint8_t *count) {
    if (category == ARCADE_MENU_CATEGORY_GAMES) {
        *count =
            (uint8_t)(sizeof(arcade_menu_games) /
                      sizeof(arcade_menu_games[0]));
        return arcade_menu_games;
    }

    *count =
        (uint8_t)(sizeof(arcade_menu_tests) /
                  sizeof(arcade_menu_tests[0]));
    return arcade_menu_tests;
}

static const ArcadeMenuItem *ArcadeMenu_ItemAt(
    ArcadeMenuCategory category,
    uint8_t index) {
    uint8_t count;
    const ArcadeMenuItem *items =
        ArcadeMenu_Items(category, &count);

    if (index >= count) {
        return NULL;
    }
    return &items[index];
}

static uint8_t ArcadeMenu_MaxScroll(
    ArcadeMenuCategory category) {
    uint8_t count;

    (void)ArcadeMenu_Items(category, &count);
    return (count > ARCADE_MENU_VISIBLE_ITEMS) ?
           (uint8_t)(count - ARCADE_MENU_VISIBLE_ITEMS) : 0U;
}

static void ArcadeMenu_ClampScroll(ArcadeMenuState *menu) {
    uint8_t max_scroll =
        ArcadeMenu_MaxScroll(menu->category);

    if (menu->scroll_index[menu->category] > max_scroll) {
        menu->scroll_index[menu->category] = max_scroll;
    }
}

static uint16_t ArcadeMenu_AbsDiff(uint16_t a, uint16_t b) {
    return (a > b) ? (uint16_t)(a - b) : (uint16_t)(b - a);
}

static void ArcadeMenu_DrawCenteredText(uint16_t x,
                                        uint16_t y,
                                        uint16_t width,
                                        uint16_t height,
                                        const char *text,
                                        FontDef *preferred_font,
                                        uint16_t color,
                                        uint16_t background) {
    FontDef *font = preferred_font;
    uint16_t text_width;
    uint16_t text_x;
    uint16_t text_y;

    if ((text == NULL) || (text[0] == '\0')) {
        return;
    }

    text_width = (uint16_t)(strlen(text) * font->width);
    if ((text_width + 4U) > width) {
        font = &Font_7x10;
        text_width = (uint16_t)(strlen(text) * font->width);
    }

    text_x = (uint16_t)(x + ((width > text_width) ?
             ((width - text_width) / 2U) : 2U));
    text_y = (uint16_t)(y + ((height > font->height) ?
             ((height - font->height) / 2U) : 1U));

    DISPLAY_WriteString_DMA(text_x,
                            text_y,
                            text,
                            *font,
                            color,
                            background);
}

static void ArcadeMenu_DrawTab(uint16_t x,
                               const char *label,
                               uint8_t active) {
    uint16_t fill =
        (active != 0U) ? ARCADE_MENU_ACCENT_DARK : ARCADE_MENU_PANEL;
    uint16_t border =
        (active != 0U) ? ARCADE_MENU_ACCENT : ARCADE_MENU_BORDER;
    uint16_t text =
        (active != 0U) ? DISPLAY_WHITE : DISPLAY_GRAY;

    DISPLAY_FillRectangle_DMA(x,
                              ARCADE_MENU_TAB_Y,
                              ARCADE_MENU_TAB_W,
                              ARCADE_MENU_TAB_H,
                              fill);
    DISPLAY_DrawRect(border,
                     x,
                     ARCADE_MENU_TAB_Y,
                     (uint16_t)(x + ARCADE_MENU_TAB_W - 1U),
                     (uint16_t)(ARCADE_MENU_TAB_Y +
                                ARCADE_MENU_TAB_H - 1U));
    ArcadeMenu_DrawCenteredText(x,
                                ARCADE_MENU_TAB_Y,
                                ARCADE_MENU_TAB_W,
                                ARCADE_MENU_TAB_H,
                                label,
                                &Font_11x18,
                                text,
                                fill);
}

static void ArcadeMenu_DrawScrollbar(const ArcadeMenuState *menu,
                                     uint8_t item_count) {
    uint8_t max_scroll =
        ArcadeMenu_MaxScroll(menu->category);
    uint16_t thumb_h = ARCADE_MENU_LIST_H;
    uint16_t thumb_y = ARCADE_MENU_LIST_Y;

    DISPLAY_FillRectangle_DMA(ARCADE_MENU_SCROLL_X,
                              ARCADE_MENU_LIST_Y,
                              ARCADE_MENU_SCROLL_W,
                              ARCADE_MENU_LIST_H,
                              ARCADE_MENU_SCROLL_TRACK);

    if (max_scroll != 0U) {
        uint16_t travel;

        thumb_h =
            (uint16_t)((ARCADE_MENU_LIST_H *
                       ARCADE_MENU_VISIBLE_ITEMS) /
                       item_count);
        if (thumb_h < 22U) {
            thumb_h = 22U;
        }
        travel = (uint16_t)(ARCADE_MENU_LIST_H - thumb_h);
        thumb_y = (uint16_t)(ARCADE_MENU_LIST_Y +
                  (((uint32_t)menu->scroll_index[menu->category] *
                    travel) / max_scroll));
    }

    DISPLAY_FillRectangle_DMA((uint16_t)(ARCADE_MENU_SCROLL_X + 2U),
                              thumb_y,
                              (uint16_t)(ARCADE_MENU_SCROLL_W - 4U),
                              thumb_h,
                              (max_scroll != 0U) ?
                              ARCADE_MENU_ACCENT :
                              ARCADE_MENU_BORDER);
}

static void ArcadeMenu_DrawList(const ArcadeMenuState *menu) {
    uint8_t count;
    const ArcadeMenuItem *items =
        ArcadeMenu_Items(menu->category, &count);
    uint8_t scroll = menu->scroll_index[menu->category];

    DISPLAY_FillRectangle_DMA(ARCADE_MENU_LIST_X,
                              ARCADE_MENU_LIST_Y,
                              ARCADE_MENU_LIST_W,
                              ARCADE_MENU_LIST_H,
                              ARCADE_MENU_PANEL);

    for (uint8_t slot = 0U;
         slot < ARCADE_MENU_VISIBLE_ITEMS;
         slot++) {
        uint8_t index = (uint8_t)(scroll + slot);
        uint16_t y = (uint16_t)(ARCADE_MENU_LIST_Y +
                     (slot * ARCADE_MENU_ITEM_H));
        uint16_t fill =
            ((slot & 1U) != 0U) ?
            ARCADE_MENU_PANEL_ALT : ARCADE_MENU_PANEL;

        DISPLAY_FillRectangle_DMA(ARCADE_MENU_LIST_X,
                                  y,
                                  ARCADE_MENU_LIST_W,
                                  ARCADE_MENU_ITEM_H,
                                  fill);
        DISPLAY_FillRectangle_DMA(ARCADE_MENU_LIST_X,
                                  (uint16_t)(y +
                                             ARCADE_MENU_ITEM_H - 1U),
                                  ARCADE_MENU_LIST_W,
                                  1U,
                                  ARCADE_MENU_BG);

        if (index >= count) {
            continue;
        }

        DISPLAY_FillRectangle_DMA((uint16_t)(ARCADE_MENU_LIST_X + 7U),
                                  (uint16_t)(y + 7U),
                                  4U,
                                  18U,
                                  ARCADE_MENU_ACCENT);
        DISPLAY_WriteString_DMA((uint16_t)(ARCADE_MENU_LIST_X + 18U),
                                (uint16_t)(y + 7U),
                                items[index].label,
                                Font_11x18,
                                DISPLAY_WHITE,
                                fill);
        DISPLAY_WriteString_DMA((uint16_t)(ARCADE_MENU_LIST_X +
                                           ARCADE_MENU_LIST_W - 20U),
                                (uint16_t)(y + 11U),
                                ">",
                                Font_7x10,
                                DISPLAY_CYAN,
                                fill);
    }

    ArcadeMenu_DrawScrollbar(menu, count);
}

static const char *ArcadeMenu_PendingLabel(
    const ArcadeMenuState *menu) {
    const ArcadeMenuItem *item =
        ArcadeMenu_ItemAt(menu->category, menu->pending_item);

    return (item != NULL) ? item->label : "ITEM";
}

static void ArcadeMenu_DrawConfirmButton(uint16_t x,
                                         const char *label,
                                         uint16_t fill,
                                         uint16_t border) {
    DISPLAY_FillRectangle_DMA(x,
                              ARCADE_MENU_CONFIRM_BUTTON_Y,
                              ARCADE_MENU_CONFIRM_BUTTON_W,
                              ARCADE_MENU_CONFIRM_BUTTON_H,
                              fill);
    DISPLAY_DrawRect(border,
                     x,
                     ARCADE_MENU_CONFIRM_BUTTON_Y,
                     (uint16_t)(x +
                                ARCADE_MENU_CONFIRM_BUTTON_W - 1U),
                     (uint16_t)(ARCADE_MENU_CONFIRM_BUTTON_Y +
                                ARCADE_MENU_CONFIRM_BUTTON_H - 1U));
    ArcadeMenu_DrawCenteredText(x,
                                ARCADE_MENU_CONFIRM_BUTTON_Y,
                                ARCADE_MENU_CONFIRM_BUTTON_W,
                                ARCADE_MENU_CONFIRM_BUTTON_H,
                                label,
                                &Font_11x18,
                                DISPLAY_WHITE,
                                fill);
}

static void ArcadeMenu_DrawConfirm(const ArcadeMenuState *menu) {
    DISPLAY_FillRectangle_DMA((uint16_t)(ARCADE_MENU_MODAL_X + 4U),
                              (uint16_t)(ARCADE_MENU_MODAL_Y + 4U),
                              ARCADE_MENU_MODAL_W,
                              ARCADE_MENU_MODAL_H,
                              ARCADE_MENU_MODAL_SHADOW);
    DISPLAY_FillRectangle_DMA(ARCADE_MENU_MODAL_X,
                              ARCADE_MENU_MODAL_Y,
                              ARCADE_MENU_MODAL_W,
                              ARCADE_MENU_MODAL_H,
                              ARCADE_MENU_MODAL_BG);
    DISPLAY_DrawRect(ARCADE_MENU_WARN,
                     ARCADE_MENU_MODAL_X,
                     ARCADE_MENU_MODAL_Y,
                     (uint16_t)(ARCADE_MENU_MODAL_X +
                                ARCADE_MENU_MODAL_W - 1U),
                     (uint16_t)(ARCADE_MENU_MODAL_Y +
                                ARCADE_MENU_MODAL_H - 1U));
    DISPLAY_WriteString_DMA(58U,
                            62U,
                            "CONFIRM START",
                            Font_16x26,
                            DISPLAY_WHITE,
                            ARCADE_MENU_MODAL_BG);
    ArcadeMenu_DrawCenteredText(48U,
                                96U,
                                224U,
                                20U,
                                ArcadeMenu_PendingLabel(menu),
                                &Font_11x18,
                                DISPLAY_CYAN,
                                ARCADE_MENU_MODAL_BG);
    ArcadeMenu_DrawCenteredText(48U,
                                120U,
                                224U,
                                14U,
                                "RUN THIS ITEM?",
                                &Font_7x10,
                                DISPLAY_GRAY,
                                ARCADE_MENU_MODAL_BG);
    ArcadeMenu_DrawConfirmButton(ARCADE_MENU_NO_X,
                                 "NO",
                                 DISPLAY_COLOR565(56, 35, 40),
                                 DISPLAY_RED);
    ArcadeMenu_DrawConfirmButton(ARCADE_MENU_YES_X,
                                 "YES",
                                 DISPLAY_COLOR565(30, 65, 48),
                                 DISPLAY_GREEN);
}

static void ArcadeMenu_Render(ArcadeMenuState *menu) {
    ArcadeMenu_ClampScroll(menu);

    DISPLAY_FillScreen_DMA(ARCADE_MENU_BG);
    DISPLAY_FillRectangle_DMA(0U,
                              0U,
                              ARCADE_MENU_SCREEN_W,
                              ARCADE_MENU_HEADER_H,
                              ARCADE_MENU_HEADER);
    DISPLAY_WriteString_DMA(8U,
                            5U,
                            "MAIN MENU",
                            Font_16x26,
                            DISPLAY_WHITE,
                            ARCADE_MENU_HEADER);
    DISPLAY_WriteString_DMA(246U,
                            14U,
                            "TOUCH",
                            Font_7x10,
                            DISPLAY_CYAN,
                            ARCADE_MENU_HEADER);

    ArcadeMenu_DrawTab(
        ARCADE_MENU_TEST_TAB_X,
        "TESTS",
        (menu->category == ARCADE_MENU_CATEGORY_TESTS) ? 1U : 0U);
    ArcadeMenu_DrawTab(
        ARCADE_MENU_GAME_TAB_X,
        "GAMES",
        (menu->category == ARCADE_MENU_CATEGORY_GAMES) ? 1U : 0U);
    ArcadeMenu_DrawList(menu);

    DISPLAY_WriteString_DMA(
        8U,
        222U,
        "TAP ITEM, SWIPE LIST   UKEY:TAB",
        Font_7x10,
        DISPLAY_GRAY,
        ARCADE_MENU_BG);

    if (menu->mode == ARCADE_MENU_MODE_CONFIRM) {
        ArcadeMenu_DrawConfirm(menu);
    }

    menu->needs_redraw = 0U;
}

static void ArcadeMenu_Init(ArcadeMenuState *menu) {
    memset(menu, 0, sizeof(*menu));
    menu->category = ARCADE_MENU_CATEGORY_TESTS;
    menu->mode = ARCADE_MENU_MODE_LIST;
    menu->pending_page = ARCADE_PAGE_SYSTEM_INFO;
    menu->needs_redraw = 1U;
}

static ArcadeMenuTouchRegion ArcadeMenu_HitConfirm(uint16_t x,
                                                   uint16_t y) {
    if ((y >= ARCADE_MENU_CONFIRM_BUTTON_Y) &&
        (y < (ARCADE_MENU_CONFIRM_BUTTON_Y +
              ARCADE_MENU_CONFIRM_BUTTON_H))) {
        if ((x >= ARCADE_MENU_NO_X) &&
            (x < (ARCADE_MENU_NO_X +
                  ARCADE_MENU_CONFIRM_BUTTON_W))) {
            return ARCADE_MENU_TOUCH_CONFIRM_NO;
        }
        if ((x >= ARCADE_MENU_YES_X) &&
            (x < (ARCADE_MENU_YES_X +
                  ARCADE_MENU_CONFIRM_BUTTON_W))) {
            return ARCADE_MENU_TOUCH_CONFIRM_YES;
        }
    }
    return ARCADE_MENU_TOUCH_NONE;
}

static ArcadeMenuTouchRegion ArcadeMenu_HitList(uint16_t x,
                                                uint16_t y) {
    if ((y >= ARCADE_MENU_TAB_Y) &&
        (y < (ARCADE_MENU_TAB_Y + ARCADE_MENU_TAB_H))) {
        if ((x >= ARCADE_MENU_TEST_TAB_X) &&
            (x < (ARCADE_MENU_TEST_TAB_X + ARCADE_MENU_TAB_W))) {
            return ARCADE_MENU_TOUCH_TESTS_TAB;
        }
        if ((x >= ARCADE_MENU_GAME_TAB_X) &&
            (x < (ARCADE_MENU_GAME_TAB_X + ARCADE_MENU_TAB_W))) {
            return ARCADE_MENU_TOUCH_GAMES_TAB;
        }
    }

    if ((y >= ARCADE_MENU_LIST_Y) &&
        (y < (ARCADE_MENU_LIST_Y + ARCADE_MENU_LIST_H))) {
        if ((x >= ARCADE_MENU_LIST_X) &&
            (x < (ARCADE_MENU_LIST_X + ARCADE_MENU_LIST_W))) {
            return ARCADE_MENU_TOUCH_LIST;
        }
        if ((x >= ARCADE_MENU_SCROLL_X) &&
            (x < (ARCADE_MENU_SCROLL_X + ARCADE_MENU_SCROLL_W))) {
            return ARCADE_MENU_TOUCH_SCROLLBAR;
        }
    }

    return ARCADE_MENU_TOUCH_NONE;
}

static uint8_t ArcadeMenu_ItemIndexFromY(
    const ArcadeMenuState *menu,
    uint16_t y) {
    uint8_t count;
    uint8_t slot;
    uint8_t index;

    (void)ArcadeMenu_Items(menu->category, &count);
    if ((y < ARCADE_MENU_LIST_Y) ||
        (y >= (ARCADE_MENU_LIST_Y + ARCADE_MENU_LIST_H))) {
        return 0xFFU;
    }

    slot = (uint8_t)((y - ARCADE_MENU_LIST_Y) /
                     ARCADE_MENU_ITEM_H);
    if (slot >= ARCADE_MENU_VISIBLE_ITEMS) {
        return 0xFFU;
    }

    index = (uint8_t)(menu->scroll_index[menu->category] + slot);
    return (index < count) ? index : 0xFFU;
}

static void ArcadeMenu_SetCategory(ArcadeMenuState *menu,
                                   ArcadeMenuCategory category) {
    if (menu->category == category) {
        return;
    }

    menu->category = category;
    menu->mode = ARCADE_MENU_MODE_LIST;
    ArcadeMenu_ClampScroll(menu);
    menu->needs_redraw = 1U;
}

static void ArcadeMenu_ToggleCategory(ArcadeMenuState *menu) {
    if (menu->mode == ARCADE_MENU_MODE_CONFIRM) {
        menu->mode = ARCADE_MENU_MODE_LIST;
        menu->needs_redraw = 1U;
        return;
    }

    ArcadeMenu_SetCategory(
        menu,
        (menu->category == ARCADE_MENU_CATEGORY_TESTS) ?
        ARCADE_MENU_CATEGORY_GAMES : ARCADE_MENU_CATEGORY_TESTS);
}

static void ArcadeMenu_SetScrollFromY(ArcadeMenuState *menu,
                                      uint16_t y) {
    uint8_t max_scroll =
        ArcadeMenu_MaxScroll(menu->category);
    uint8_t next_scroll;

    if (max_scroll == 0U) {
        return;
    }

    if (y <= ARCADE_MENU_LIST_Y) {
        next_scroll = 0U;
    } else if (y >= (ARCADE_MENU_LIST_Y + ARCADE_MENU_LIST_H)) {
        next_scroll = max_scroll;
    } else {
        next_scroll =
            (uint8_t)(((uint32_t)(y - ARCADE_MENU_LIST_Y) *
                       (max_scroll + 1U)) /
                      ARCADE_MENU_LIST_H);
        if (next_scroll > max_scroll) {
            next_scroll = max_scroll;
        }
    }

    if (menu->scroll_index[menu->category] != next_scroll) {
        menu->scroll_index[menu->category] = next_scroll;
        menu->needs_redraw = 1U;
    }
}

static void ArcadeMenu_HandleListDrag(ArcadeMenuState *menu,
                                      uint16_t x,
                                      uint16_t y) {
    uint8_t max_scroll =
        ArcadeMenu_MaxScroll(menu->category);
    int16_t dy =
        (int16_t)y - (int16_t)menu->last_touch_y;

    if ((ArcadeMenu_AbsDiff(menu->touch_start_y, y) >
         ARCADE_MENU_TAP_SLOP) ||
        (ArcadeMenu_AbsDiff(menu->touch_start_x, x) >
         ARCADE_MENU_TAP_SLOP)) {
        menu->drag_started = 1U;
    }

    if (max_scroll == 0U) {
        return;
    }

    while ((dy >= (int16_t)ARCADE_MENU_DRAG_STEP) &&
           (menu->scroll_index[menu->category] > 0U)) {
        menu->scroll_index[menu->category]--;
        menu->last_touch_y =
            (uint16_t)(menu->last_touch_y +
                       ARCADE_MENU_DRAG_STEP);
        dy = (int16_t)(dy - (int16_t)ARCADE_MENU_DRAG_STEP);
        menu->needs_redraw = 1U;
    }

    while ((dy <= -(int16_t)ARCADE_MENU_DRAG_STEP) &&
           (menu->scroll_index[menu->category] < max_scroll)) {
        menu->scroll_index[menu->category]++;
        menu->last_touch_y =
            (uint16_t)(menu->last_touch_y -
                       ARCADE_MENU_DRAG_STEP);
        dy = (int16_t)(dy + (int16_t)ARCADE_MENU_DRAG_STEP);
        menu->needs_redraw = 1U;
    }
}

static void ArcadeMenu_OpenConfirm(ArcadeMenuState *menu,
                                   uint8_t item_index) {
    const ArcadeMenuItem *item =
        ArcadeMenu_ItemAt(menu->category, item_index);

    if (item == NULL) {
        return;
    }

    menu->pending_item = item_index;
    menu->pending_page = item->page;
    menu->mode = ARCADE_MENU_MODE_CONFIRM;
    menu->ignore_until_release = 1U;
    menu->needs_redraw = 1U;
}

static void ArcadeMenu_HandleRelease(ArcadeMenuState *menu,
                                     uint16_t x,
                                     uint16_t y,
                                     ArcadePageId *requested_page) {
    ArcadeMenuTouchRegion release_region;

    if (menu->ignore_until_release != 0U) {
        menu->ignore_until_release = 0U;
        return;
    }

    if (menu->mode == ARCADE_MENU_MODE_CONFIRM) {
        release_region = ArcadeMenu_HitConfirm(x, y);
        if (release_region != menu->touch_region) {
            return;
        }
        if (release_region == ARCADE_MENU_TOUCH_CONFIRM_NO) {
            menu->mode = ARCADE_MENU_MODE_LIST;
            menu->needs_redraw = 1U;
        } else if (release_region == ARCADE_MENU_TOUCH_CONFIRM_YES) {
            *requested_page = menu->pending_page;
            menu->mode = ARCADE_MENU_MODE_LIST;
            menu->needs_redraw = 1U;
        }
        return;
    }

    if (menu->drag_started != 0U) {
        return;
    }

    release_region = ArcadeMenu_HitList(x, y);
    if (release_region != menu->touch_region) {
        return;
    }

    if (release_region == ARCADE_MENU_TOUCH_TESTS_TAB) {
        ArcadeMenu_SetCategory(menu, ARCADE_MENU_CATEGORY_TESTS);
    } else if (release_region == ARCADE_MENU_TOUCH_GAMES_TAB) {
        ArcadeMenu_SetCategory(menu, ARCADE_MENU_CATEGORY_GAMES);
    } else if (release_region == ARCADE_MENU_TOUCH_LIST) {
        uint8_t item_index =
            ArcadeMenu_ItemIndexFromY(menu, y);

        if (item_index != 0xFFU) {
            ArcadeMenu_OpenConfirm(menu, item_index);
        }
    }
}

static void ArcadeMenu_Update(ArcadeMenuState *menu,
                              uint8_t touch_pressed,
                              uint16_t touch_x,
                              uint16_t touch_y,
                              ArcadePageId *requested_page) {
    *requested_page = ARCADE_PAGE_MENU;

    if (touch_pressed == 0U) {
        if (menu->touch_was_pressed != 0U) {
            ArcadeMenu_HandleRelease(menu,
                                     menu->last_touch_x,
                                     menu->last_touch_raw_y,
                                     requested_page);
        } else if (menu->ignore_until_release != 0U) {
            menu->ignore_until_release = 0U;
        }
        menu->touch_was_pressed = 0U;
        menu->touch_region = ARCADE_MENU_TOUCH_NONE;
        menu->drag_started = 0U;
        return;
    }

    if (menu->touch_was_pressed == 0U) {
        menu->touch_start_x = touch_x;
        menu->touch_start_y = touch_y;
        menu->last_touch_x = touch_x;
        menu->last_touch_raw_y = touch_y;
        menu->last_touch_y = touch_y;
        menu->drag_started = 0U;
        menu->touch_region =
            (menu->mode == ARCADE_MENU_MODE_CONFIRM) ?
            ArcadeMenu_HitConfirm(touch_x, touch_y) :
            ArcadeMenu_HitList(touch_x, touch_y);

        if ((menu->mode == ARCADE_MENU_MODE_LIST) &&
            (menu->touch_region == ARCADE_MENU_TOUCH_SCROLLBAR)) {
            menu->drag_started = 1U;
            ArcadeMenu_SetScrollFromY(menu, touch_y);
        }
    } else if (menu->ignore_until_release == 0U) {
        if ((menu->mode == ARCADE_MENU_MODE_LIST) &&
            (menu->touch_region == ARCADE_MENU_TOUCH_SCROLLBAR)) {
            menu->drag_started = 1U;
            ArcadeMenu_SetScrollFromY(menu, touch_y);
        } else if ((menu->mode == ARCADE_MENU_MODE_LIST) &&
                   (menu->touch_region == ARCADE_MENU_TOUCH_LIST)) {
            ArcadeMenu_HandleListDrag(menu, touch_x, touch_y);
        }
        menu->last_touch_x = touch_x;
        menu->last_touch_raw_y = touch_y;
    }

    menu->touch_was_pressed = 1U;
}

static uint8_t ArcadeCollection_ReadTouch(uint16_t *x,
                                          uint16_t *y) {
    if (DISPLAY_TouchPressed()) {
        return DISPLAY_TouchGetCoordinates(x, y) ? 1U : 0U;
    }
    return 0U;
}

static uint32_t ArcadeCollection_Mix32(uint32_t value) {
    value ^= value >> 16;
    value *= 0x7FEB352DU;
    value ^= value >> 15;
    value *= 0x846CA68BU;
    value ^= value >> 16;
    return value;
}

static void ArcadeCollection_MixSensorEntropy(
    ArcadeCollection *arcade,
    const MPU6000_Data_t *mpu_data,
    HAL_StatusTypeDef mpu_status) {
    uint32_t value = arcade->sensor_entropy;

    value ^= HAL_GetTick() + 0x9E3779B9U +
             (value << 6) + (value >> 2);
    if (mpu_status == HAL_OK) {
        value ^= ((uint32_t)(uint16_t)mpu_data->accel_x << 16) |
                 (uint32_t)(uint16_t)mpu_data->gyro_z;
        value ^= ((uint32_t)(uint16_t)mpu_data->accel_y << 16) |
                 (uint32_t)(uint16_t)mpu_data->gyro_x;
        value ^= ((uint32_t)(uint16_t)mpu_data->accel_z << 16) |
                 (uint32_t)(uint16_t)mpu_data->gyro_y;
    }

    arcade->sensor_entropy = ArcadeCollection_Mix32(value);
}

static uint32_t ArcadeCollection_NextSeed(ArcadeCollection *arcade) {
    uint32_t value =
        arcade->seed ^ arcade->sensor_entropy ^ HAL_GetTick();

    if (value == 0U) {
        value = 0x9E3779B9U;
    }
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    arcade->seed = value;
    return value;
}

static void ArcadeCollection_RenderActive(ArcadeCollection *arcade) {
    switch (arcade->active_page) {
        case ARCADE_PAGE_MENU:
            ArcadeMenu_Render(&arcade->menu);
            break;
        case ARCADE_PAGE_SYSTEM_INFO:
            SystemInfoPage_Render(&arcade->system_info);
            break;
        case ARCADE_PAGE_MPU6000:
            arcade->mpu6000.renderer_initialized = 0U;
            MPU6000Page_Render(&arcade->mpu6000);
            break;
        case ARCADE_PAGE_TOUCH_TEST:
            arcade->touch_test.renderer_initialized = 0U;
            TouchTestPage_Render(&arcade->touch_test);
            break;
        case ARCADE_PAGE_COMET_CATCH:
            CometCatchRender_Init(&arcade->comet_catch);
            break;
        case ARCADE_PAGE_ORB_HUNT:
            OrbHuntRender_Init(&arcade->orb_hunt);
            break;
        case ARCADE_PAGE_GRAVITY_PONG:
            GravityPongRender_Init(&arcade->gravity_pong);
            break;
        case ARCADE_PAGE_TILT_BREAKER:
            TiltBreakerRender_Init(&arcade->tilt_breaker);
            break;
        case ARCADE_PAGE_MARBLE_MAZE:
            MarbleMazeRender_Init(&arcade->marble_maze);
            break;
        case ARCADE_PAGE_SPACE_DODGER:
            SpaceDodgerRender_Init(&arcade->space_dodger);
            break;
#if ARCADE_ENABLE_BALANCE_TOWER
        case ARCADE_PAGE_BALANCE_TOWER:
            BalanceTowerRender_Init(&arcade->balance_tower);
            break;
#endif
        case ARCADE_PAGE_SHAKE_FLIGHT:
            ShakeFlightRender_Init(&arcade->shake_flight);
            break;
#if ARCADE_ENABLE_BIPLANE_DUEL
        case ARCADE_PAGE_BIPLANE_DUEL:
            BiplaneDuelRender_Init(&arcade->biplane_duel);
            break;
#endif
        case ARCADE_PAGE_LIFE_GAME:
            LifeGameRender_Init(&arcade->life_game);
            break;
        case ARCADE_PAGE_MICRO_COLONY:
            MicroColonyRender_Init(&arcade->micro_colony);
            break;
        case ARCADE_PAGE_TETRIS:
            TetrisRender_Init(&arcade->tetris);
            break;
        default:
            break;
    }
}

void ArcadeCollection_Init(ArcadeCollection *arcade, uint32_t seed) {
    arcade->seed = (seed != 0U) ? seed : 0xA341316CU;
    arcade->sensor_entropy =
        ArcadeCollection_Mix32(arcade->seed ^ HAL_GetTick());
    arcade->active_page = ARCADE_PAGE_MENU;

    ArcadeMenu_Init(&arcade->menu);
    SystemInfoPage_Init(&arcade->system_info);
    MPU6000Page_Init(&arcade->mpu6000);
    TouchTestPage_Init(&arcade->touch_test);
    CometCatch_Init(&arcade->comet_catch,
                    ArcadeCollection_NextSeed(arcade));
    OrbHunt_Init(&arcade->orb_hunt,
                 ArcadeCollection_NextSeed(arcade));
    GravityPong_Init(&arcade->gravity_pong,
                     ArcadeCollection_NextSeed(arcade));
    TiltBreaker_Init(&arcade->tilt_breaker,
                     ArcadeCollection_NextSeed(arcade));
    MarbleMaze_Init(&arcade->marble_maze,
                    ArcadeCollection_NextSeed(arcade));
    SpaceDodger_Init(&arcade->space_dodger,
                     ArcadeCollection_NextSeed(arcade));
#if ARCADE_ENABLE_BALANCE_TOWER
    BalanceTower_Init(&arcade->balance_tower,
                      ArcadeCollection_NextSeed(arcade));
#endif
    ShakeFlight_Init(&arcade->shake_flight,
                     ArcadeCollection_NextSeed(arcade));
#if ARCADE_ENABLE_BIPLANE_DUEL
    BiplaneDuel_Init(&arcade->biplane_duel,
                     ArcadeCollection_NextSeed(arcade));
#endif
    LifeGame_Init(&arcade->life_game,
                  ArcadeCollection_NextSeed(arcade));
    MicroColony_Init(&arcade->micro_colony,
                     ArcadeCollection_NextSeed(arcade));
    Tetris_Init(&arcade->tetris,
                ArcadeCollection_NextSeed(arcade));
    ArcadeCollection_RenderActive(arcade);
}

void ArcadeCollection_Update(ArcadeCollection *arcade,
                             const MPU6000_Data_t *mpu_data,
                             HAL_StatusTypeDef mpu_status) {
    ArcadeCollection_MixSensorEntropy(arcade,
                                      mpu_data,
                                      mpu_status);

    switch (arcade->active_page) {
        case ARCADE_PAGE_MENU: {
            uint16_t touch_x = 0U;
            uint16_t touch_y = 0U;
            uint8_t touch_pressed =
                ArcadeCollection_ReadTouch(&touch_x, &touch_y);
            ArcadePageId requested_page = ARCADE_PAGE_MENU;

            ArcadeMenu_Update(&arcade->menu,
                              touch_pressed,
                              touch_x,
                              touch_y,
                              &requested_page);
            if (requested_page != ARCADE_PAGE_MENU) {
                arcade->active_page = requested_page;
                ArcadeCollection_RenderActive(arcade);
            } else if (arcade->menu.needs_redraw != 0U) {
                ArcadeMenu_Render(&arcade->menu);
            }
            break;
        }
        case ARCADE_PAGE_SYSTEM_INFO:
            break;
        case ARCADE_PAGE_MPU6000:
            MPU6000Page_Update(&arcade->mpu6000,
                               mpu_data,
                               mpu_status);
            break;
        case ARCADE_PAGE_TOUCH_TEST:
            TouchTestPage_Update(&arcade->touch_test);
            break;
        case ARCADE_PAGE_COMET_CATCH: {
            uint16_t touch_x = 0U;
            uint16_t touch_y = 0U;
            uint8_t touch_pressed =
                ArcadeCollection_ReadTouch(&touch_x, &touch_y);
            CometCatchEvent event =
                CometCatch_Update(&arcade->comet_catch,
                                  mpu_data->accel_x,
                                  touch_pressed);
            CometCatchRender_Frame(&arcade->comet_catch, event);
            break;
        }
        case ARCADE_PAGE_ORB_HUNT: {
            OrbHuntEvent event =
                OrbHunt_Update(&arcade->orb_hunt,
                               mpu_data->accel_x,
                               mpu_data->accel_y);
            OrbHuntRender_Frame(&arcade->orb_hunt, event);
            break;
        }
        case ARCADE_PAGE_GRAVITY_PONG: {
            GravityPongEvent event =
                GravityPong_Update(&arcade->gravity_pong,
                                   mpu_data->accel_x);
            GravityPongRender_Frame(&arcade->gravity_pong, event);
            break;
        }
        case ARCADE_PAGE_TILT_BREAKER: {
            TiltBreakerEvent event =
                TiltBreaker_Update(&arcade->tilt_breaker,
                                   mpu_data->accel_x);
            TiltBreakerRender_Frame(&arcade->tilt_breaker, event);
            break;
        }
        case ARCADE_PAGE_MARBLE_MAZE: {
            MarbleMazeEvent event =
                MarbleMaze_Update(&arcade->marble_maze,
                                  mpu_data->accel_x,
                                  mpu_data->accel_y);
            MarbleMazeRender_Frame(&arcade->marble_maze, event);
            break;
        }
        case ARCADE_PAGE_SPACE_DODGER: {
            SpaceDodgerEvent event =
                SpaceDodger_Update(&arcade->space_dodger,
                                   mpu_data->accel_x,
                                   mpu_data->accel_y);
            SpaceDodgerRender_Frame(&arcade->space_dodger, event);
            break;
        }
#if ARCADE_ENABLE_BALANCE_TOWER
        case ARCADE_PAGE_BALANCE_TOWER: {
            BalanceTowerEvent event =
                BalanceTower_Update(&arcade->balance_tower,
                                    mpu_data->gyro_z);
            BalanceTowerRender_Frame(&arcade->balance_tower, event);
            break;
        }
#endif
        case ARCADE_PAGE_SHAKE_FLIGHT: {
            ShakeFlightEvent event =
                ShakeFlight_Update(&arcade->shake_flight,
                                   mpu_data->accel_z);
            ShakeFlightRender_Frame(&arcade->shake_flight, event);
            break;
        }
#if ARCADE_ENABLE_BIPLANE_DUEL
        case ARCADE_PAGE_BIPLANE_DUEL: {
            BiplaneDuelEvent event =
                BiplaneDuel_Update(&arcade->biplane_duel,
                                   mpu_data->accel_x,
                                   mpu_data->accel_y);
            BiplaneDuelRender_Frame(&arcade->biplane_duel, event);
            break;
        }
#endif
        case ARCADE_PAGE_LIFE_GAME: {
            uint16_t touch_x = 0U;
            uint16_t touch_y = 0U;
            uint8_t touch_pressed =
                ArcadeCollection_ReadTouch(&touch_x, &touch_y);
            LifeGameEvent event =
                LifeGame_Update(&arcade->life_game,
                                mpu_data,
                                mpu_status);
            event |= LifeGame_HandleTouch(&arcade->life_game,
                                          touch_pressed,
                                          touch_x,
                                          touch_y);
            LifeGameRender_Frame(&arcade->life_game, event);
            break;
        }
        case ARCADE_PAGE_MICRO_COLONY: {
            MicroColonyEvent event =
                MicroColony_Update(&arcade->micro_colony,
                                   mpu_data,
                                   mpu_status);
            MicroColonyRender_Frame(&arcade->micro_colony,
                                    event);
            break;
        }
        case ARCADE_PAGE_TETRIS: {
            uint16_t touch_x = 0U;
            uint16_t touch_y = 0U;
            uint8_t touch_pressed =
                ArcadeCollection_ReadTouch(&touch_x, &touch_y);
            TetrisEvent event =
                Tetris_HandleTouch(&arcade->tetris,
                                   touch_pressed,
                                   touch_x,
                                   touch_y);

            if ((event & TETRIS_EVENT_RESTART_REQUEST) != 0U) {
                Tetris_Init(&arcade->tetris,
                            ArcadeCollection_NextSeed(arcade));
                TetrisRender_Init(&arcade->tetris);
                break;
            }

            event |= Tetris_Update(&arcade->tetris);
            TetrisRender_Frame(&arcade->tetris, event);
            break;
        }
        default:
            break;
    }
}

void ArcadeCollection_NextPage(ArcadeCollection *arcade) {
    if (arcade->active_page == ARCADE_PAGE_MENU) {
        ArcadeMenu_ToggleCategory(&arcade->menu);
    } else {
        arcade->active_page = ARCADE_PAGE_MENU;
        arcade->menu.mode = ARCADE_MENU_MODE_LIST;
        arcade->menu.touch_was_pressed = 0U;
        arcade->menu.touch_region = ARCADE_MENU_TOUCH_NONE;
        arcade->menu.drag_started = 0U;
        arcade->menu.ignore_until_release = 0U;
        arcade->menu.needs_redraw = 1U;
    }
    ArcadeCollection_RenderActive(arcade);
}

void ArcadeCollection_RestartActive(ArcadeCollection *arcade) {
    uint32_t seed = ArcadeCollection_NextSeed(arcade);

    switch (arcade->active_page) {
        case ARCADE_PAGE_MENU:
            ArcadeMenu_Init(&arcade->menu);
            break;
        case ARCADE_PAGE_SYSTEM_INFO:
            SystemInfoPage_Refresh(&arcade->system_info);
            break;
        case ARCADE_PAGE_MPU6000:
            MPU6000_Init();
            MPU6000Page_Init(&arcade->mpu6000);
            break;
        case ARCADE_PAGE_TOUCH_TEST:
            TouchTestPage_Init(&arcade->touch_test);
            break;
        case ARCADE_PAGE_COMET_CATCH:
            CometCatch_Init(&arcade->comet_catch, seed);
            break;
        case ARCADE_PAGE_ORB_HUNT:
            OrbHunt_Init(&arcade->orb_hunt, seed);
            break;
        case ARCADE_PAGE_GRAVITY_PONG:
            GravityPong_Init(&arcade->gravity_pong, seed);
            break;
        case ARCADE_PAGE_TILT_BREAKER:
            TiltBreaker_Init(&arcade->tilt_breaker, seed);
            break;
        case ARCADE_PAGE_MARBLE_MAZE:
            MarbleMaze_Init(&arcade->marble_maze, seed);
            break;
        case ARCADE_PAGE_SPACE_DODGER:
            SpaceDodger_Init(&arcade->space_dodger, seed);
            break;
#if ARCADE_ENABLE_BALANCE_TOWER
        case ARCADE_PAGE_BALANCE_TOWER:
            BalanceTower_Init(&arcade->balance_tower, seed);
            break;
#endif
        case ARCADE_PAGE_SHAKE_FLIGHT:
            ShakeFlight_Init(&arcade->shake_flight, seed);
            break;
#if ARCADE_ENABLE_BIPLANE_DUEL
        case ARCADE_PAGE_BIPLANE_DUEL:
            BiplaneDuel_Init(&arcade->biplane_duel, seed);
            break;
#endif
        case ARCADE_PAGE_LIFE_GAME:
            LifeGame_Init(&arcade->life_game, seed);
            break;
        case ARCADE_PAGE_MICRO_COLONY:
            MicroColony_Init(&arcade->micro_colony, seed);
            break;
        case ARCADE_PAGE_TETRIS:
            Tetris_Init(&arcade->tetris, seed);
            break;
        default:
            break;
    }
    ArcadeCollection_RenderActive(arcade);
}
