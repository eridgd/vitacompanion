#include <vitacompanion_input.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum {
    INPUT_PARSE_INVALID_TARGET = -1,
    INPUT_PARSE_ARGUMENT_COUNT = -2,
    INPUT_PARSE_INVALID_NUMBER = -3,
    INPUT_PARSE_OUT_OF_RANGE = -4
};

typedef struct {
    const char *name;
    uint32_t mask;
} button_name;

static const button_name button_names[] = {
    {"select", 0x00000001},
    {"l3", 0x00000002},
    {"r3", 0x00000004},
    {"start", 0x00000008},
    {"up", 0x00000010},
    {"right", 0x00000020},
    {"down", 0x00000040},
    {"left", 0x00000080},
    {"l", 0x00000100},
    {"l2", 0x00000100},
    {"r", 0x00000200},
    {"r2", 0x00000200},
    {"l1", 0x00000400},
    {"r1", 0x00000800},
    {"triangle", 0x00001000},
    {"circle", 0x00002000},
    {"cross", 0x00004000},
    {"x", 0x00004000},
    {"square", 0x00008000},
    {"ps", 0x00010000}
};

static int parse_uint(const char *value, unsigned int maximum,
    unsigned int *result)
{
    char *end;
    unsigned long parsed;

    if (!value || value[0] == '\0')
        return INPUT_PARSE_INVALID_NUMBER;

    parsed = strtoul(value, &end, 10);
    if (*end != '\0')
        return INPUT_PARSE_INVALID_NUMBER;
    if (parsed > maximum)
        return INPUT_PARSE_OUT_OF_RANGE;

    *result = (unsigned int)parsed;
    return 0;
}

static int parse_button(const char *name, uint32_t *mask)
{
    size_t i;

    for (i = 0; i < sizeof(button_names) / sizeof(button_names[0]); ++i)
    {
        if (strcmp(name, button_names[i].name) == 0)
        {
            *mask = button_names[i].mask;
            return 0;
        }
    }

    return INPUT_PARSE_INVALID_TARGET;
}

static bool parse_stick(const char *name, vitacompanion_stick *stick)
{
    if (strcmp(name, "left-stick") == 0)
        *stick = VITACOMPANION_STICK_LEFT;
    else if (strcmp(name, "right-stick") == 0)
        *stick = VITACOMPANION_STICK_RIGHT;
    else
        return false;

    return true;
}

static bool parse_touch_port(const char *name,
    vitacompanion_touch_port *port)
{
    if (strcmp(name, "front-touch") == 0)
        *port = VITACOMPANION_TOUCH_FRONT;
    else if (strcmp(name, "rear-touch") == 0)
        *port = VITACOMPANION_TOUCH_REAR;
    else
        return false;

    return true;
}

int vitacompanion_parse_press(char **arg_list, size_t arg_count,
    vitacompanion_input_action *action)
{
    unsigned int value;
    int result;

    if (!arg_list || !action || arg_count < 2)
        return INPUT_PARSE_ARGUMENT_COUNT;

    memset(action, 0, sizeof(*action));
    action->active = 1;

    if (parse_stick(arg_list[1], &action->data.analog.stick))
    {
        if (arg_count != 4)
            return INPUT_PARSE_ARGUMENT_COUNT;

        result = parse_uint(arg_list[2], 255, &value);
        if (result < 0)
            return result;
        action->data.analog.x = (uint8_t)value;

        result = parse_uint(arg_list[3], 255, &value);
        if (result < 0)
            return result;
        action->data.analog.y = (uint8_t)value;

        action->type = VITACOMPANION_INPUT_ANALOG;
        return 0;
    }

    if (parse_touch_port(arg_list[1], &action->data.touch.port))
    {
        if (arg_count != 5)
            return INPUT_PARSE_ARGUMENT_COUNT;

        result = parse_uint(arg_list[2], VITACOMPANION_TOUCH_SLOTS - 1,
            &value);
        if (result < 0)
            return result;
        action->data.touch.slot = (uint8_t)value;

        result = parse_uint(arg_list[3], VITACOMPANION_TOUCH_MAX_X,
            &value);
        if (result < 0)
            return result;
        action->data.touch.x = (uint16_t)value;

        result = parse_uint(arg_list[4], VITACOMPANION_TOUCH_MAX_Y,
            &value);
        if (result < 0)
            return result;
        action->data.touch.y = (uint16_t)value;

        action->type = VITACOMPANION_INPUT_TOUCH;
        return 0;
    }

    if (arg_count != 2)
        return INPUT_PARSE_ARGUMENT_COUNT;

    result = parse_button(arg_list[1], &action->data.button.mask);
    if (result < 0)
        return result;

    action->type = VITACOMPANION_INPUT_BUTTON;
    return 0;
}

int vitacompanion_parse_release(char **arg_list, size_t arg_count,
    vitacompanion_input_action *action)
{
    unsigned int value;
    int result;

    if (!arg_list || !action || arg_count < 2)
        return INPUT_PARSE_ARGUMENT_COUNT;

    memset(action, 0, sizeof(*action));

    if (strcmp(arg_list[1], "all") == 0)
    {
        if (arg_count != 2)
            return INPUT_PARSE_ARGUMENT_COUNT;
        action->type = VITACOMPANION_INPUT_RESET;
        return 0;
    }

    if (parse_stick(arg_list[1], &action->data.analog.stick))
    {
        if (arg_count != 2)
            return INPUT_PARSE_ARGUMENT_COUNT;
        action->type = VITACOMPANION_INPUT_ANALOG;
        return 0;
    }

    if (parse_touch_port(arg_list[1], &action->data.touch.port))
    {
        if (arg_count != 3)
            return INPUT_PARSE_ARGUMENT_COUNT;

        result = parse_uint(arg_list[2], VITACOMPANION_TOUCH_SLOTS - 1,
            &value);
        if (result < 0)
            return result;
        action->data.touch.slot = (uint8_t)value;
        action->type = VITACOMPANION_INPUT_TOUCH;
        return 0;
    }

    if (arg_count != 2)
        return INPUT_PARSE_ARGUMENT_COUNT;

    result = parse_button(arg_list[1], &action->data.button.mask);
    if (result < 0)
        return result;

    action->type = VITACOMPANION_INPUT_BUTTON;
    return 0;
}

const char *vitacompanion_input_parse_error(int result)
{
    switch (result)
    {
    case INPUT_PARSE_INVALID_TARGET:
        return "Error: Unknown input target.\n";
    case INPUT_PARSE_ARGUMENT_COUNT:
        return "Error: Incorrect number of arguments.\n";
    case INPUT_PARSE_INVALID_NUMBER:
        return "Error: Input values must be decimal integers.\n";
    case INPUT_PARSE_OUT_OF_RANGE:
        return "Error: Input value is out of range.\n";
    default:
        return "Error: Invalid input.\n";
    }
}
