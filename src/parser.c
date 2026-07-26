#include "parser.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// Note: maybe use https://github.com/ryanflannery/str2argv
//       doesn't seem to support positional argument decision trees though


size_t parse_cmd(char *cmd, size_t cmd_size, char **arg_list, size_t arg_max) {
    size_t arg_count = 0;
    bool in_arg = false;

    for (unsigned int i=0; i < cmd_size && arg_count < arg_max; i ++) {
        bool is_endline = cmd[i] == '\n' || cmd[i] == '\r';
        bool is_space = cmd[i] == ' ' || cmd[i] == '\t' || is_endline;

        if (is_space) {
            cmd[i] = '\0';
            in_arg = false;

            if (is_endline) {
                break;
            }
        } else if (!in_arg) {
            arg_list[arg_count] = &(cmd[i]);
            arg_count += 1;
            in_arg = true;
        }
    }

    return arg_count;
}

bool parse_cmd_chain(char *input, size_t input_size, char **commands,
    size_t command_max, size_t *command_count)
{
    char *command_start;
    bool has_content = false;
    size_t count = 0;
    size_t i;

    if (!input || !commands || !command_count)
        return false;

    command_start = input;
    for (i = 0; i < input_size; ++i)
    {
        bool is_endline = input[i] == '\n' || input[i] == '\r';
        bool is_separator = input[i] == ';' || is_endline;

        if (is_separator)
        {
            input[i] = '\0';
            if (has_content)
            {
                if (count >= command_max)
                    return false;
                commands[count++] = command_start;
            }

            has_content = false;
            command_start = &input[i + 1];
            if (is_endline)
                break;
        }
        else if (input[i] != ' ' && input[i] != '\t')
        {
            has_content = true;
        }
    }

    if (i == input_size && has_content)
    {
        if (count >= command_max)
            return false;
        commands[count++] = command_start;
    }

    *command_count = count;
    return true;
}

bool parse_wait_duration_ms(const char *value, uint32_t *duration_ms)
{
    uint64_t number = 0;
    size_t digit_count = 0;
    uint64_t multiplier;

    if (!value || !duration_ms)
        return false;

    while (value[digit_count] >= '0' && value[digit_count] <= '9')
    {
        number = number * 10 + (uint64_t)(value[digit_count] - '0');
        if (number > UINT32_MAX)
            return false;
        ++digit_count;
    }

    if (digit_count == 0)
        return false;

    if (strcmp(&value[digit_count], "ms") == 0)
        multiplier = 1;
    else if (strcmp(&value[digit_count], "s") == 0)
        multiplier = 1000;
    else
        return false;

    if (number > UINT32_MAX / multiplier)
        return false;

    *duration_ms = (uint32_t)(number * multiplier);
    return true;
}
