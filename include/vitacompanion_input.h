#pragma once

#include <stddef.h>
#include <stdint.h>

#define VITACOMPANION_ANALOG_CENTER 128
#define VITACOMPANION_TOUCH_MAX_X 1919
#define VITACOMPANION_TOUCH_MAX_Y 1087
#define VITACOMPANION_TOUCH_SLOTS 4

typedef enum {
    VITACOMPANION_INPUT_BUTTON,
    VITACOMPANION_INPUT_ANALOG,
    VITACOMPANION_INPUT_TOUCH,
    VITACOMPANION_INPUT_RESET
} vitacompanion_input_type;

typedef enum {
    VITACOMPANION_STICK_LEFT,
    VITACOMPANION_STICK_RIGHT
} vitacompanion_stick;

typedef enum {
    VITACOMPANION_TOUCH_FRONT,
    VITACOMPANION_TOUCH_REAR
} vitacompanion_touch_port;

typedef struct {
    vitacompanion_input_type type;
    int active;
    union {
        struct {
            uint32_t mask;
        } button;
        struct {
            vitacompanion_stick stick;
            uint8_t x;
            uint8_t y;
        } analog;
        struct {
            vitacompanion_touch_port port;
            uint8_t slot;
            uint16_t x;
            uint16_t y;
        } touch;
    } data;
} vitacompanion_input_action;

int vitacompanion_parse_press(char **arg_list, size_t arg_count,
    vitacompanion_input_action *action);
int vitacompanion_parse_release(char **arg_list, size_t arg_count,
    vitacompanion_input_action *action);
const char *vitacompanion_input_parse_error(int result);
