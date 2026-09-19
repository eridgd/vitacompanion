#include "net.h"

#include "cmd.h"
#include "log.h"

#include <ftpvita.h>
#include <stdio.h>
#include <string.h>
#include <vitasdk.h>

#define NET_CTL_ERROR_NOT_TERMINATED ((int)0x80412102)

extern volatile int run;

volatile int all_is_up;
volatile int net_connected;

SceUID net_thid = -1;
static int netctl_cb_id = -1;
static int netctl_initialized;

int net_start()
{
    int result;

    net_thid = sceKernelCreateThread("vitacompanion_net_thread", net_thread, 0x40, 0x10000, 0, 0, NULL);
    if (net_thid < 0)
        return net_thid;

    result = sceKernelStartThread(net_thid, 0, NULL);
    if (result < 0)
    {
        sceKernelDeleteThread(net_thid);
        net_thid = -1;
        return result;
    }

    return 0;
}

void net_end()
{
    if (netctl_cb_id >= 0)
    {
        sceNetCtlInetUnregisterCallback(netctl_cb_id);
        netctl_cb_id = -1;
    }

    if (all_is_up)
    {
        cmd_end();
        ftpvita_fini();
        all_is_up = 0;
    }

    if (netctl_initialized)
    {
        sceNetCtlTerm();
        netctl_initialized = 0;
    }
}

/* FTP `SITE <command chain>`: runs a command-port request over the FTP
 * control connection. FTP serves every client on its own thread, so this
 * remains usable (for `SITE reboot` in particular) even if the command port
 * is wedged by an executor that never returns. */
static void ftp_site_command(ftpvita_client_info_t* client)
{
    static const char* const reply_ok = "200";
    static const char* const reply_failed = "500";
    char request[CMD_REQUEST_MAX + 2];
    char response[CMD_RES_MAX];
    char line[256];
    char text[256 - 8];
    const char* code;
    const char* cursor;
    size_t length;

    cursor = client->recv_cmd_args;
    while (*cursor == ' ' || *cursor == '\t')
        cursor++;
    length = strlen(cursor);
    if (length == 0)
    {
        ftpvita_ext_client_send_ctrl_msg(client,
            "501 SITE requires a vitacompanion command." FTPVITA_EOL);
        return;
    }
    if (length > CMD_REQUEST_MAX)
    {
        ftpvita_ext_client_send_ctrl_msg(client,
            "501 SITE command is too long." FTPVITA_EOL);
        return;
    }

    memcpy(request, cursor, length);
    request[length] = '\n';
    request[length + 1] = '\0';
    cmd_handle(request, (unsigned int)(length + 1), response);

    code = strncmp(response, "Error:", 6) == 0 ? reply_failed : reply_ok;
    cursor = response;
    while (*cursor)
    {
        const char* end = strchr(cursor, '\n');
        size_t line_length = end ? (size_t)(end - cursor) : strlen(cursor);

        /* libk's snprintf does not implement "%.*s", so copy the line
         * out and print it with a plain "%s". */
        if (line_length > sizeof(text) - 1)
            line_length = sizeof(text) - 1;
        memcpy(text, cursor, line_length);
        text[line_length] = '\0';
        snprintf(line, sizeof(line), "%s-%s" FTPVITA_EOL, code, text);
        ftpvita_ext_client_send_ctrl_msg(client, line);
        cursor = end ? end + 1 : cursor + line_length;
    }
    snprintf(line, sizeof(line), "%s SITE command %s." FTPVITA_EOL, code,
        code == reply_ok ? "completed" : "failed");
    ftpvita_ext_client_send_ctrl_msg(client, line);
}

static void do_net_connected()
{
    char vita_ip[16];
    unsigned short int vita_port;

    LOG("do_net_connected\n");

#if ENABLE_LOGGING == 1
    ftpvita_set_info_log_cb(LOG);
    ftpvita_set_debug_log_cb(LOG);
#endif

    ftpvita_set_file_buf_size(512 * 1024);

    if (ftpvita_init(vita_ip, &vita_port) >= 0)
    {
        ftpvita_add_device("ux0:");
        ftpvita_add_device("ur0:");
        ftpvita_add_device("uma0:");
        ftpvita_add_device("imc0:");
        ftpvita_add_device("xmc0:");
        ftpvita_add_device("grw0:");
        ftpvita_ext_add_custom_command("SITE", ftp_site_command);

        if (cmd_start() >= 0)
        {
            all_is_up = 1;
        }
        else
        {
            ftpvita_fini();
        }
    }
}

static void netctl_cb(int event_type, void* arg)
{
    int state;
    int result;

    LOG("netctl cb: %d\n", event_type);
    (void)arg;

    result = sceNetCtlInetGetState(&state);
    if (result < 0)
    {
        LOG("sceNetCtlInetGetState: 0x%08X\n", result);
        return;
    }

    if (state != 3 && all_is_up)
    {
        net_connected = 0;
        cmd_end();
        ftpvita_fini();
        all_is_up = 0;
    }
    else if (state == 3 && !all_is_up)
    { /* IP obtained */
        net_connected = 1;
        do_net_connected();
    }
}

int net_thread(unsigned int args, void* argp)
{
    int ret;

    (void)args;
    (void)argp;

    sceKernelDelayThread(3 * 1000 * 1000);
    if (!run)
        return 0;

    ret = sceNetCtlInit();
    LOG("sceNetCtlInit: 0x%08X\n", ret);
    if (ret < 0 && ret != NET_CTL_ERROR_NOT_TERMINATED)
        return ret;
    netctl_initialized = ret == 0;

    ret = sceNetCtlInetRegisterCallback(netctl_cb, NULL, &netctl_cb_id);
    LOG("sceNetCtlInetRegisterCallback: 0x%08X\n", ret);
    if (ret < 0)
        return ret;

    netctl_cb(0, NULL);
    while (run)
    {
        sceNetCtlCheckCallback();
        sceKernelDelayThread(1000 * 1000);
    }

    return 0;
}
