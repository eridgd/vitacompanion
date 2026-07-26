#pragma once

#include <vitacompanion_input.h>

#include <stdbool.h>

int input_start(void);
void input_end(void);
bool input_is_ready(void);
int input_apply(const vitacompanion_input_action *action);
