#include <assert.h>
#include <raylib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef uint8_t u8;
typedef uint32_t u32;
typedef struct {
    u32 a, b, c, d;
} bitset128;
typedef int8_t i8;
typedef size_t usize;
typedef ssize_t ussize;

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define ABS(n)    ((n) < 0 ? -(n) : (n))

#define MAX_VALUE           9
#define JOINED_VALUE        10
#define ROW_LENGTH          9
#define MAX_ROW             14
#define BOARD_SIZE          (ROW_LENGTH * MAX_ROW)
#define NO_INDEX            BOARD_SIZE
#define WINDOW_RATIO_WIDTH  9
#define WINDOW_RATIO_HEIGHT 16
#define WINDOW_WIDTH        800
#define WINDOW_HEIGHT       (WINDOW_WIDTH * WINDOW_RATIO_WIDTH / WINDOW_RATIO_HEIGHT)
#define SLOT_WIDTH          50
#define SLOT_HEIGHT         SLOT_WIDTH
#define SLOT_GAP            10
#define FONT_SIZE           16
#define BOARD_WIDTH         ((SLOT_WIDTH * ROW_LENGTH) + (SLOT_GAP * (ROW_LENGTH + 1)))
#define INITIAL_X           ((WINDOW_WIDTH / 2) - (BOARD_WIDTH / 2))
#define INITIAL_Y           SLOT_GAP

#define ROW_FROM_INDEX(index)    ((index) / ROW_LENGTH)
#define COL_FROM_INDEX(index)    ((index) % ROW_LENGTH)
#define INDEX_FROM_POS(row, col) (((row) * ROW_LENGTH) + (col))

const int MID_X = SLOT_WIDTH / 2;
const int MID_Y = SLOT_HEIGHT / 2;

static i8 board[BOARD_SIZE] = {0};
static usize lastIndex = 0;
static usize selectedIndex = NO_INDEX;
static_assert(BOARD_SIZE < 128, "No room for board indexes in 128 bit bitset");
static bitset128 blocking = {0};

#define BITSET128_SET(bset, n)                                                 \
    do {                                                                       \
        if ((n) < 32) {                                                        \
            (bset).d |= (1 << (n));                                            \
        } else if ((n) < 64) {                                                 \
            (bset).c |= (1 << ((n) - 32));                                     \
        } else if ((n) < 96) {                                                 \
            (bset).b |= (1 << ((n) - 64));                                     \
        } else {                                                               \
            (bset).a |= (1 << ((n) - 96));                                     \
        }                                                                      \
    } while (0)
#define BITSET128_GET(bset, n)                                                 \
    ((n) < 32     ? (bset).d & (1 << (n)) :                                    \
         (n) < 64 ? (bset).c & (1 << ((n) - 32)) :                             \
         (n) < 96 ? (bset).b & (1 << ((n) - 64)) :                             \
                    (bset).a & (1 << ((n) - 96)))
#define BITSET128_ANY(bset) ((bset).d || (bset).c || (bset).b || (bset).a)
#define BITSET128_CLEAR(bset)                                                  \
    do {                                                                       \
        (bset).d = 0;                                                          \
        (bset).c = 0;                                                          \
        (bset).b = 0;                                                          \
        (bset).a = 0;                                                          \
    } while (0)

static usize blockingAnimationFrame = 0;
const usize MAX_BLOCKING_ANIMATION_FRAMES = 120;

typedef enum {
    MATCH,
    NO_MATCH,
    MATCH_BLOCKED,
} MatchKind;

static char *GetNumberText(i8 n);
static void GuiSlot(usize row, usize col, Vector2 measure, Vector2 mouse);
static bool CanMatch(usize index, usize col, usize row);

int main(int argc, const char **argv) {
    int seed = time(NULL);
    if (argc > 1 && strcmp(argv[1], "--seed") == 0) {
        if (argc < 3) {
            TraceLog(LOG_ERROR, "missing argument for flag --seed");
            return 1;
        }

        char *endptr;
        seed = strtol(argv[2], &endptr, 0);
        if (*endptr != '\0') {
            TraceLog(LOG_ERROR, "invalid number for flag --seed");
            return 1;
        }
    }
    TraceLog(LOG_INFO, "RANDOM: seed = %#x", seed);
    srand(seed);

    lastIndex = 30;
    for (usize i = 0; i <= lastIndex; i++) {
        board[i] = (((u8)rand()) % MAX_VALUE) + 1;
    }

    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Hello, Raylib!");

    Font font = GetFontDefault();
    Vector2 measure = MeasureTextEx(font, "0", FONT_SIZE, 0);

    while (!WindowShouldClose()) {
        BeginDrawing();
        {
            ClearBackground(BLACK);

            if (BITSET128_ANY(blocking)) {
                if (blockingAnimationFrame < MAX_BLOCKING_ANIMATION_FRAMES) {
                    blockingAnimationFrame++;
                } else {
                    blockingAnimationFrame = 0;
                    BITSET128_CLEAR(blocking);
                }
            }

            Vector2 mouse = GetMousePosition();

            usize lastRow = ROW_FROM_INDEX(lastIndex);
            for (usize row = 0; row <= lastRow; row++) {
                for (usize col = 0; col < ROW_LENGTH; col++) {
                    GuiSlot(row, col, measure, mouse);
                }
            }
        }
        EndDrawing();
    }

    CloseWindow();

    return 0;
}

static char *GetNumberText(i8 n) {
    switch (ABS(n)) {
    case 1:  return "1";
    case 2:  return "2";
    case 3:  return "3";
    case 4:  return "4";
    case 5:  return "5";
    case 6:  return "6";
    case 7:  return "7";
    case 8:  return "8";
    case 9:  return "9";
    default: return "";
    }
}

static bool CanMatch(usize index, usize row, usize col) {
    BITSET128_CLEAR(blocking);

    usize selectedRow = ROW_FROM_INDEX(selectedIndex);
    usize selectedCol = COL_FROM_INDEX(selectedIndex);
    if (col == selectedCol) {
        usize startRow = MIN(row, selectedRow);
        usize endRow = MAX(row, selectedRow);
        for (usize i = startRow + 1; i < endRow; i++) {
            usize currentIndex = INDEX_FROM_POS(i, col);
            if (board[currentIndex] > 0) BITSET128_SET(blocking, currentIndex);
        }
        return !BITSET128_ANY(blocking);
    }

    usize start = MIN(index, selectedIndex);
    usize end = MAX(index, selectedIndex);
    for (usize i = start + 1; i < end; i++) {
        if (board[i] > 0) BITSET128_SET(blocking, i);
    }
    if (!BITSET128_ANY(blocking)) return true;

    usize startRow = ROW_FROM_INDEX(start);
    usize startCol = COL_FROM_INDEX(start);
    usize endRow = ROW_FROM_INDEX(end);
    usize endCol = COL_FROM_INDEX(end);

    bool isDiagonal = (startCol + startRow) == (endCol + endRow) ||
        (startCol - startRow) == (endCol - endRow);
    if (!isDiagonal) return false;

    BITSET128_CLEAR(blocking);

    i8 direction = startCol > endCol ? -1 : 1;
    i8 offset = direction;
    for (usize i = startRow + 1; i < endRow; i++, offset += direction) {
        usize currentIndex = INDEX_FROM_POS(i, startCol + offset);
        if (board[currentIndex] > 0) BITSET128_SET(blocking, currentIndex);
    }
    return !BITSET128_ANY(blocking);
}

static void GuiSlot(usize row, usize col, Vector2 measure, Vector2 mouse) {
    usize index = INDEX_FROM_POS(row, col);
    i8 n = board[index];

    usize x = INITIAL_X + col * (SLOT_WIDTH + (col == 0 ? 0 : SLOT_GAP));

    if (BITSET128_GET(blocking, index)) {
        usize middle = MAX_BLOCKING_ANIMATION_FRAMES / 2;
        x += (blockingAnimationFrame < middle ? 1 : -1) * 10;
    }

    usize y = INITIAL_Y + row * (SLOT_HEIGHT + (row == 0 ? 0 : SLOT_GAP));

    Rectangle bounds = {x, y, SLOT_WIDTH, SLOT_HEIGHT};

    DrawRectangleRec(bounds, index == selectedIndex ? SKYBLUE : WHITE);

    if (n) {
        DrawText(GetNumberText(n), x + MID_X - (measure.x / 2),
                 y + MID_Y - (measure.y / 2), FONT_SIZE,
                 n < 0 ? LIGHTGRAY : BLACK);

        if (BITSET128_ANY(blocking)) return;

        if (n > 0 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
            CheckCollisionPointRec(mouse, bounds)) {
            if (selectedIndex == NO_INDEX) {
                selectedIndex = index;
            } else if (selectedIndex == index) {
                selectedIndex = NO_INDEX;
            } else {
                i8 selected = board[selectedIndex];
                if (selected != n && selected + n != JOINED_VALUE) {
                    selectedIndex = index;
                } else if (CanMatch(index, row, col)) {
                    board[index] = -n;
                    board[selectedIndex] = -selected;
                    selectedIndex = NO_INDEX;
                }
            }
        }
    }
}
