#include <raylib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef uint8_t u8;
typedef int8_t i8;
typedef size_t usize;
typedef ssize_t ussize;

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define ABS(n)    ((n) < 0 ? -(n) : (n))

#define MAX_VALUE           9
#define JOINED_VALUE        10
#define ROW_LENGTH          9
#define MAX_ROW             15
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

static i8 BOARD[BOARD_SIZE] = {0};
static usize lastIndex = 0;
static usize selectedIndex = NO_INDEX;

static char *GetNumberText(i8 n);
static void GuiSlot(usize row, usize col, Vector2 measure, Vector2 mouse);

int main(void) {
    srand(time(NULL));

    lastIndex = 20;
    for (usize i = 0; i <= lastIndex; i++) {
        BOARD[i] = (((u8)rand()) % MAX_VALUE) + 1;
    }

    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Hello, Raylib!");

    Font font = GetFontDefault();
    Vector2 measure = MeasureTextEx(font, "0", FONT_SIZE, 0);

    while (!WindowShouldClose()) {
        BeginDrawing();
        {
            ClearBackground(BLACK);

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

static void GuiSlot(usize row, usize col, Vector2 measure, Vector2 mouse) {
    usize index = INDEX_FROM_POS(row, col);
    i8 n = BOARD[index];

    usize x = INITIAL_X + col * (SLOT_WIDTH + (col == 0 ? 0 : SLOT_GAP));
    usize y = INITIAL_Y + row * (SLOT_HEIGHT + (row == 0 ? 0 : SLOT_GAP));
    Rectangle bounds = {x, y, SLOT_WIDTH, SLOT_HEIGHT};

    DrawRectangleRec(bounds, index == selectedIndex ? SKYBLUE : WHITE);

    if (n) {
        DrawText(GetNumberText(n), x + MID_X - (measure.x / 2),
                 y + MID_Y - (measure.y / 2), FONT_SIZE,
                 n < 0 ? LIGHTGRAY : BLACK);

        if (n > 0 && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
            CheckCollisionPointRec(mouse, bounds)) {
            if (selectedIndex == NO_INDEX) {
                selectedIndex = index;
            } else if (selectedIndex == index) {
                selectedIndex = NO_INDEX;
            } else {
                i8 selected = BOARD[selectedIndex];
                if (selected != n && selected + n != JOINED_VALUE) {
                    TraceLog(LOG_ERROR, "Cannot match these two");
                    TraceLog(LOG_ERROR, "selected = %d", selected);
                    TraceLog(LOG_ERROR, "n = %d", n);
                    // TODO: report error
                    return;
                }

                usize selectedRow = ROW_FROM_INDEX(selectedIndex);
                usize selectedCol = COL_FROM_INDEX(selectedIndex);
                bool canMatch = true;
                if (col == selectedCol) {
                    usize startRow = MIN(row, selectedRow);
                    usize endRow = MAX(row, selectedRow);
                    for (usize i = startRow + 1; i < endRow; i++) {
                        if (BOARD[INDEX_FROM_POS(i, col)]) {
                            canMatch = false;
                            break;
                        }
                    }
                } else {
                    usize start = MIN(index, selectedIndex);
                    usize end = MAX(index, selectedIndex);
                    for (usize i = start + 1; i < end; i++) {
                        if (BOARD[i]) {
                            canMatch = false;
                            break;
                        }
                    }
                }
                // TODO: support diagonal

                if (!canMatch) {
                    TraceLog(LOG_ERROR, "Cannot match these two");
                    TraceLog(LOG_ERROR, "selectedIndex = %d", selectedIndex);
                    TraceLog(LOG_ERROR, "index = %d", index);
                    // TODO: report error
                    return;
                }

                BOARD[index] = -n;
                BOARD[selectedIndex] = -selected;
                selectedIndex = NO_INDEX;
            }
        }
    }
}
