#pragma once

/* Longest request line accepted on the command port, without the
 * terminating NUL. */
#define CMD_REQUEST_MAX (2048)
/* Capacity of the response buffer handed to cmd_handle, NUL included. */
#define CMD_RES_MAX (8192)

int cmd_thread(unsigned int args, void* argp);
int cmd_start();
void cmd_end();

/* Parse, validate and execute a request line. `cmd` is modified in place
 * and must be NUL-terminated at `cmd[cmd_size]`; `res_msg` must hold
 * CMD_RES_MAX bytes and always comes back NUL-terminated. */
void cmd_handle(char* cmd, unsigned int cmd_size, char* res_msg);
