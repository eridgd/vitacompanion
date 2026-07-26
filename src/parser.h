#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

size_t parse_cmd(char *cmd, size_t cmd_size, char **arg_list,
    size_t arg_max);
bool parse_cmd_chain(char *input, size_t input_size, char **commands,
    size_t command_max, size_t *command_count);
bool parse_wait_duration_ms(const char *value, uint32_t *duration_ms);
