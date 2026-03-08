#include <assert.h>
#include <raylib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "./inter.c"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef u32 bitset128[4];
typedef int8_t i8;
typedef size_t usize;

// Add `printf` checking for `TraceLog`
void TraceLog(int logLevel, const char *text, ...)
    __attribute__((format(printf, 2, 3)));

#define UNREACHABLE(message)                                                   \
    do {                                                                       \
        fprintf(stderr, "%s:%d: UNREACHABLE: %s\n", __FILE__, __LINE__,        \
                message);                                                      \
        abort();                                                               \
    } while (0)

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define ABS(n)    ((n) < 0 ? -(n) : (n))

#if defined(__GNUC__) || defined(__clang__)
#define BITCOUNT __builtin_popcount
#else
static usize CustomBitcount(usize n) {
    usize result = 0;
    for (; n; n <<= 1) {
        if (n & 1) result++;
    }
    return result;
}
#define BITCOUNT CustomBitcount
#endif // defined(__GNUC__) || defined(__clang__)

#define BITSET128_SET(bset, n)                                                 \
    do {                                                                       \
        usize index = (n) / 32;                                                \
        (bset)[index] |= (1 << ((n) - (32 * index)));                          \
    } while (0)
#define BITSET128_GET(bset, n)                                                 \
    ((bset)[(n) / 32] & (1 << ((n) - (32 * ((n) / 32)))))
#define BITSET128_ANY(bset) ((bset)[0] || (bset)[1] || (bset)[2] || (bset)[3])
#define BITSET128_CLEAR(bset)                                                  \
    do {                                                                       \
        (bset)[0] = 0;                                                         \
        (bset)[1] = 0;                                                         \
        (bset)[2] = 0;                                                         \
        (bset)[3] = 0;                                                         \
    } while (0)

typedef enum {
    ANIMATION_NONE = 0,
    ANIMATION_MATCHING,
    ANIMATION_BLOCKING
} Animation_Kind;

#define MAX_VALUE           9
#define JOINED_VALUE        10
#define COL_COUNT           9
#define ROW_COUNT           14
#define INITIAL_SLOTS       42
#define BOARD_SIZE          (COL_COUNT * ROW_COUNT)
#define NO_INDEX            BOARD_SIZE
#define WINDOW_RATIO_WIDTH  9
#define WINDOW_RATIO_HEIGHT 16
#define WINDOW_WIDTH        800
#define WINDOW_HEIGHT       (WINDOW_WIDTH * WINDOW_RATIO_WIDTH / WINDOW_RATIO_HEIGHT)
#define SLOT_WIDTH          50
#define SLOT_HEIGHT         SLOT_WIDTH
#define SLOT_GAP            10
#define FONT_SIZE           20
#define BOARD_WIDTH         ((SLOT_WIDTH * COL_COUNT) + (SLOT_GAP * (COL_COUNT + 1)))
#define INITIAL_Y           SLOT_GAP
#define BUTTON_RADIUS       30.0
#define BADGE_RADIUS        15
#define PLUS_FONT_SIZE      32
#define PLUS                "+"
#define COMPLETED           "-"

#define ROW_FROM_INDEX(index)    ((index) / COL_COUNT)
#define COL_FROM_INDEX(index)    ((index) % COL_COUNT)
#define INDEX_FROM_POS(row, col) (((row) * COL_COUNT) + (col))

const int INITIAL_X = ((WINDOW_WIDTH / 2) - (BOARD_WIDTH / 2));
const int SLOT_MID_X = SLOT_WIDTH / 2;
const int SLOT_MID_Y = SLOT_HEIGHT / 2;
const int WINDOW_MID_Y = WINDOW_HEIGHT / 2;
const int BUTTON_CENTER_X = INITIAL_X / 2;
const int BADGE_CENTER_X = (BADGE_RADIUS / 2);

const Vector2 ADD_SLOTS_BUTTON_CENTER = {BUTTON_CENTER_X, WINDOW_MID_Y};
const Vector2 BADGE_CENTER = {
    ADD_SLOTS_BUTTON_CENTER.x + BUTTON_RADIUS - BADGE_CENTER_X,
    ADD_SLOTS_BUTTON_CENTER.y - BUTTON_RADIUS + BADGE_CENTER_X};

Font font;
Vector2 NUMBER_MEASURES[MAX_VALUE + 1] = {0};
Vector2 PLUS_MEASURE = {0};
Vector2 COMPLETED_MEASURE = {0};

static u8 adds = 5;
static i8 board[BOARD_SIZE] = {0};
static usize slotsCount = 0;
static usize selectedIndex = NO_INDEX;
static_assert(BOARD_SIZE <= 128, "board too large for 128 bitset of indexes");
static bitset128 blocking = {0};
static bool matchMultiRow = false;
static usize matches[2] = {NO_INDEX, NO_INDEX};
static u16 completed = 0;

#define ANIMATION_FRAMES 240
static usize animationFrame = 0;

static Vector2 CenterFromIndex(usize index);
static Vector2 EndOfRow(usize index);
static Vector2 StartOfRow(usize index);
static char *GetNumberText(i8 n);
static bool CanMatch(usize index, usize col, usize row);
static void ClearRowIfNeeded(usize row);
static void GuiSlot(usize row, usize col, Vector2 mouse);
static void GuiAddSlotsButton(Vector2 mouse);

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

    usize nonAssignable = completed;
    for (usize i = 0; i < INITIAL_SLOTS; i++) {
        usize availableCount = (MAX_VALUE - BITCOUNT(nonAssignable));
        u8 n = ((u8)rand()) % availableCount;
        i8 available = -1;
        for (u8 i = 0; i < MAX_VALUE; i++) {
            if (!(nonAssignable & (1 << i))) available++;
            if (available == n) {
                u8 value = i + 1;
                board[slotsCount++] = value;
                if (rand() % 2 == 0) {
                    nonAssignable ^= 1 << i;
                    if (nonAssignable == ((1 << MAX_VALUE) - 1))
                        nonAssignable = completed;
                }
                break;
            }
        }
    }

    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Number Match");

    font = LoadFont_Inter();

    for (usize i = 0; i < MAX_VALUE + 1; i++) {
        NUMBER_MEASURES[i] =
            MeasureTextEx(font, GetNumberText(i), FONT_SIZE, 0);
    }
    PLUS_MEASURE = MeasureTextEx(font, PLUS, PLUS_FONT_SIZE, 0);
    COMPLETED_MEASURE = MeasureTextEx(font, COMPLETED, FONT_SIZE, 0);

    while (!WindowShouldClose()) {
        BeginDrawing();
        {
            ClearBackground(BLACK);

            Animation_Kind animation = matches[0] != NO_INDEX ?
                ANIMATION_MATCHING :
                BITSET128_ANY(blocking) ? ANIMATION_BLOCKING :
                                          ANIMATION_NONE;

            if (animation) {
                if (animationFrame < ANIMATION_FRAMES) {
                    animationFrame++;
                } else {
                    animationFrame = 0;
                    BITSET128_CLEAR(blocking);
                    matches[0] = NO_INDEX;
                    matches[1] = NO_INDEX;
                }
            }

            Vector2 mouse = GetMousePosition();

            // TODO: implement scrolling
            for (usize row = 0; row < ROW_COUNT; row++) {
                for (usize col = 0; col < COL_COUNT; col++) {
                    GuiSlot(row, col, mouse);
                }
            }

            if (animation == ANIMATION_MATCHING &&
                animationFrame < ANIMATION_FRAMES) {
                if (matchMultiRow) {
                    DrawLineEx(CenterFromIndex(matches[0]),
                               EndOfRow(matches[0]), 3, SKYBLUE);
                    DrawLineEx(CenterFromIndex(matches[1]),
                               StartOfRow(matches[1]), 3, SKYBLUE);
                } else {
                    DrawLineEx(CenterFromIndex(matches[0]),
                               CenterFromIndex(matches[1]), 3, SKYBLUE);
                }
            }

            GuiAddSlotsButton(mouse);

            // TODO: show hint button

            for (usize i = 0; i < MAX_VALUE; i++) {
                i8 n = i + 1;
                char *text;
                Vector2 measure;
                if (completed & (1 << i)) {
                    text = COMPLETED;
                    measure = COMPLETED_MEASURE;
                } else {
                    text = GetNumberText(n);
                    measure = NUMBER_MEASURES[n];
                }
                int textMidX = (measure.x / 2);
                int x = INITIAL_X + BOARD_WIDTH + 10 - textMidX;
                int y = WINDOW_MID_Y / 2 + (i * 20);
                Vector2 pos = {x, y};
                DrawTextEx(font, text, pos, FONT_SIZE, 0, WHITE);
            }
        }
        EndDrawing();
    }

    CloseWindow();

    return 0;
}

static Vector2 CenterFromIndex(usize index) {
    usize col = COL_FROM_INDEX(index);
    usize row = ROW_FROM_INDEX(index);
    Vector2 ret = {0};
    ret.x =
        INITIAL_X + col * (SLOT_WIDTH + (col == 0 ? 0 : SLOT_GAP)) + SLOT_MID_X;
    ret.y = INITIAL_Y + row * (SLOT_HEIGHT + (row == 0 ? 0 : SLOT_GAP)) +
        SLOT_MID_Y;
    return ret;
}

static Vector2 EndOfRow(usize index) {
    usize row = ROW_FROM_INDEX(index);
    Vector2 ret = {0};
    ret.x =
        INITIAL_X + ((COL_COUNT - 1) * (SLOT_WIDTH + SLOT_GAP)) + (SLOT_WIDTH);
    ret.y = INITIAL_Y + row * (SLOT_HEIGHT + (row == 0 ? 0 : SLOT_GAP)) +
        SLOT_MID_Y;
    return ret;
}

static Vector2 StartOfRow(usize index) {
    usize row = ROW_FROM_INDEX(index);
    Vector2 ret = {0};
    ret.x = INITIAL_X;
    ret.y = INITIAL_Y + row * (SLOT_HEIGHT + (row == 0 ? 0 : SLOT_GAP)) +
        SLOT_MID_Y;
    return ret;
}

static char *GetNumberText(i8 n) {
    switch (ABS(n)) {
    case 0:  return "0";
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
    // TODO: do we need this reset?
    BITSET128_CLEAR(blocking);
    matchMultiRow = false;

    usize selectedRow = ROW_FROM_INDEX(selectedIndex);
    usize selectedCol = COL_FROM_INDEX(selectedIndex);
    if (col == selectedCol) {
        usize startRow = MIN(row, selectedRow);
        usize endRow = MAX(row, selectedRow);
        for (usize i = startRow + 1; i < endRow; i++) {
            usize currentIndex = INDEX_FROM_POS(i, col);
            if (board[currentIndex] > 0) BITSET128_SET(blocking, currentIndex);
        }
        if (!BITSET128_ANY(blocking)) {
            matches[0] = index;
            matches[1] = selectedIndex;
            return true;
        } else {
            return false;
        }
    }

    usize start = MIN(index, selectedIndex);
    usize end = MAX(index, selectedIndex);
    for (usize i = start + 1; i < end; i++) {
        if (board[i] > 0) BITSET128_SET(blocking, i);
    }
    if (!BITSET128_ANY(blocking)) {
        matchMultiRow = row != selectedRow;
        matches[0] = start;
        matches[1] = end;
        return true;
    }

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
    if (!BITSET128_ANY(blocking)) {
        matches[0] = index;
        matches[1] = selectedIndex;
        return true;
    } else {
        return false;
    }
}

static void GuiSlot(usize row, usize col, Vector2 mouse) {
    usize index = INDEX_FROM_POS(row, col);
    i8 n = board[index];

    usize x = INITIAL_X + col * (SLOT_WIDTH + (col == 0 ? 0 : SLOT_GAP));

    // TODO: add animation for making a match
    if (BITSET128_GET(blocking, index)) {
        usize middle = ANIMATION_FRAMES / 2;
        x += (animationFrame < middle ? 1 : -1) * (SLOT_GAP / 2);
    }

    usize y = INITIAL_Y + row * (SLOT_HEIGHT + (row == 0 ? 0 : SLOT_GAP));

    Rectangle bounds = {x, y, SLOT_WIDTH, SLOT_HEIGHT};

    DrawRectangleRec(bounds, index == selectedIndex ? SKYBLUE : WHITE);

    if (n) {
        usize centerX = x + SLOT_MID_X;
        usize centerY = y + SLOT_MID_Y;

        if (matches[0] == index || matches[1] == index) {
            DrawCircle(centerX, centerY, SLOT_MID_Y - 3, SKYBLUE);
        }

        Vector2 measure = NUMBER_MEASURES[ABS(n)];
        Vector2 pos = {centerX - (measure.x / 2), centerY - (measure.y / 2)};
        DrawTextEx(font, GetNumberText(n), pos, FONT_SIZE, 0,
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

                    bool stillExists = false;
                    bool selectedStillExists = false;
                    for (usize i = 0; i < slotsCount; i++) {
                        i8 value = board[i];
                        if (!stillExists && value == n) {
                            stillExists = true;
                        }
                        if (!selectedStillExists && value == selected) {
                            selectedStillExists = true;
                        }
                        if (stillExists && selectedStillExists) break;
                    }
                    if (!stillExists) {
                        completed |= (1 << (n - 1));
                    }
                    if (!selectedStillExists) {
                        completed |= (1 << (selected - 1));
                    }

                    usize selectedRow = ROW_FROM_INDEX(selectedIndex);
                    usize lastRow = MAX(selectedRow, row);
                    usize firstRow = MIN(selectedRow, row);
                    ClearRowIfNeeded(lastRow);
                    ClearRowIfNeeded(firstRow);

                    selectedIndex = NO_INDEX;
                }
            }
        }
    }
}

static void GuiAddSlotsButton(Vector2 mouse) {
    DrawCircleV(ADD_SLOTS_BUTTON_CENTER, BUTTON_RADIUS,
                !adds ? LIGHTGRAY : WHITE);
    Vector2 pos = {ADD_SLOTS_BUTTON_CENTER.x - PLUS_MEASURE.x / 2,
                   ADD_SLOTS_BUTTON_CENTER.y - PLUS_MEASURE.y / 2};
    DrawTextEx(font, PLUS, pos, PLUS_FONT_SIZE, 0, BLACK);

    DrawCircleV(BADGE_CENTER, BADGE_RADIUS, !adds ? GRAY : SKYBLUE);
    Vector2 measure = NUMBER_MEASURES[adds];
    pos.x = BADGE_CENTER.x - (measure.x / 2);
    pos.y = BADGE_CENTER.y - (measure.y / 2);
    DrawTextEx(font, GetNumberText(adds), pos, FONT_SIZE, 0, BLACK);

    if (adds &&
        CheckCollisionPointCircle(mouse, ADD_SLOTS_BUTTON_CENTER,
                                  BUTTON_RADIUS) &&
        IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        adds -= 1;

        usize count = 0;
        for (usize i = 0; i < slotsCount; i++) {
            if (board[i] > 0) count++;
        }
        usize newSlots = MIN(count, BOARD_SIZE - slotsCount - 1);

        usize index = 0;
        for (usize i = 0; i < newSlots; i++) {
            board[slotsCount++] = board[index];
            do {
                index++;
            } while (board[index] <= 0);
        }
    }
}

static void ClearRowIfNeeded(usize row) {
    usize lastRow = ROW_FROM_INDEX(slotsCount - 1);

    if (row == lastRow) return;

    for (usize col = 0; col < COL_COUNT; col++) {
        if (board[INDEX_FROM_POS(row, col)] > 0) return;
    }

    for (usize i = row; i <= lastRow; i++) {
        for (usize col = 0; col < COL_COUNT; col++) {
            board[INDEX_FROM_POS(i, col)] = board[INDEX_FROM_POS(i + 1, col)];
        }
    }

    for (usize col = 0; col < COL_COUNT; col++) {
        board[INDEX_FROM_POS(lastRow, col)] = 0;
    }

    slotsCount -= COL_COUNT;
}
