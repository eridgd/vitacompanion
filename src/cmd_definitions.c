#include "cmd_definitions.h"
#include "nosleep.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <vitasdk.h>

#define COUNT_OF(arr) (sizeof(arr) / sizeof(arr[0]))
#define CMD_RESPONSE_MAX 2048

const cmd_definition cmd_definitions[] = {
    {.name = "help", .description = "Display this help screen", .arg_count = 0, .executor = &cmd_help},
    {.name = "destroy", .description = "Kill all running applications", .arg_count = 0, .executor = &cmd_destroy},
    {.name = "launch", .description = "Launch an app by Title ID", .arg_count = 1, .executor = &cmd_launch},
    {.name = "kill", .description = "Kill an app by Title ID", .arg_count = 1, .executor = &cmd_kill},
    {.name = "nosleep", .description = "Control automatic suspend prevention", .arg_count = 1, .executor = &cmd_nosleep},
    {.name = "reboot", .description = "Reboot the console", .arg_count = 0, .executor = &cmd_reboot},
    {.name = "screen", .description = "Turn the screen on or off", .arg_count = 1, .executor = &cmd_screen}
};

const cmd_definition *cmd_get_definition(char *cmd_name) {
  for (unsigned int i = 0; i < COUNT_OF(cmd_definitions); i++) {
    if (!strcmp(cmd_name, cmd_definitions[i].name)) {
      return &(cmd_definitions[i]);
    }
  }

  return NULL;
}

void cmd_help(char **arg_list, size_t arg_count, char *res_msg) {
  int longest_cmd = 0;
  size_t used = 0;

  for (size_t i = 0; i < COUNT_OF(cmd_definitions); ++i) {
    int cmd_length = (int)strlen(cmd_definitions[i].name);

    if (cmd_length > longest_cmd) {
      longest_cmd = cmd_length;
    }
  }

  used = (size_t)snprintf(res_msg, CMD_RESPONSE_MAX, "%-*s\t\t%s\n",
    longest_cmd, "Command", "Description");
  if (used >= CMD_RESPONSE_MAX)
    return;

  for (size_t i = 0; i < COUNT_OF(cmd_definitions); ++i) {
    int written = snprintf(res_msg + used, CMD_RESPONSE_MAX - used,
      "%-*s\t\t%s\n", longest_cmd, cmd_definitions[i].name,
      cmd_definitions[i].description);
    if (written < 0 || (size_t)written >= CMD_RESPONSE_MAX - used)
      return;
    used += (size_t)written;
  }
}


void cmd_kill(char **arg_list, size_t arg_count, char* res_msg) {
  if (sceAppMgrDestroyAppByName(arg_list[1]) < 0) {
    strcpy(res_msg, "Error: cannot kill the app. Is the TITLEID correct?\n");
  } else {
    strcpy(res_msg, "Killed.\n");
  }
}

void cmd_destroy(char **arg_list, size_t arg_count, char *res_msg) {
  sceAppMgrDestroyOtherApp();
  strcpy(res_msg, "Apps destroyed.\n");
}

void cmd_nosleep(char **arg_list, size_t arg_count, char *res_msg) {
  char *state = arg_list[1];

  if (!strcmp(state, "on")) {
    nosleep_set_enabled(true);
    strcpy(res_msg, "No-sleep enabled.\n");
  } else if (!strcmp(state, "off")) {
    nosleep_set_enabled(false);
    strcpy(res_msg, "No-sleep disabled.\n");
  } else if (!strcmp(state, "status")) {
    strcpy(res_msg, nosleep_is_enabled() ? "No-sleep enabled.\n" : "No-sleep disabled.\n");
  } else {
    strcpy(res_msg, "Error: param should be 'on', 'off' or 'status'\n");
  }
}

void cmd_launch(char **arg_list, size_t arg_count, char *res_msg) {
  char uri[32];

  snprintf(uri, 32, "psgm:play?titleid=%s", arg_list[1]);

  if (sceAppMgrLaunchAppByUri(0x20000, uri) < 0) {
    strcpy(res_msg, "Error: cannot launch the app. Is the TITLEID correct?\n");
  } else {
    strcpy(res_msg, "Launched.\n");
  }
}

void cmd_reboot(char **arg_list, size_t arg_count, char *res_msg) {
  scePowerRequestColdReset();
  strcpy(res_msg, "Rebooting...\n");
}

void cmd_screen(char **arg_list, size_t arg_count, char *res_msg) {
  char *state = arg_list[1];

  if (!strcmp(state, "on")) {
    scePowerRequestDisplayOn();
    strcpy(res_msg, "Turning display on...\n");
  } else if (!strcmp(state, "off")) {
    scePowerRequestDisplayOff();
    strcpy(res_msg, "Turning display off...\n");
  } else {
    strcpy(res_msg, "Error: param should be 'on' or 'off'\n");
  }
}
