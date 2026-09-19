#include "cmd.h"

#include "cmd_definitions.h"
#include "parser.h"

#include <psp2/kernel/modulemgr.h>
#include <stdbool.h>
#include <stdio.h>
#include <vitasdk.h>

#ifndef CMD_PORT
#define CMD_PORT 1338
#endif

#if CMD_PORT < 1 || CMD_PORT > 65535
#error CMD_PORT must be between 1 and 65535
#endif
#define ARG_MAX (20)
#define CMD_MAX (32)
#define CMD_IO_TIMEOUT_US (15 * 1000 * 1000)
#define CMD_BUSY_RECV_TIMEOUT_US (2 * 1000 * 1000)
#define CMD_START_TIMEOUT_MS 5000
#define CMD_WORKER_STOP_TIMEOUT_US (2 * 1000 * 1000)

/* Every accepted connection is served by its own worker thread so that an
 * executor which never returns (a launch that hangs the target application,
 * for instance) cannot stall the accept loop and take the whole command
 * channel down with it. CMD_WORKER_MAX bounds how many requests may be in
 * flight; when the pool is exhausted the accept thread still serves a bare
 * `reboot` inline so remote recovery is always possible. */
#define CMD_WORKER_MAX (4)

extern volatile int run;
extern volatile int all_is_up;
extern volatile int net_connected;

typedef struct {
    int busy;
    SceUID thid;
    int sockfd;
    char request[CMD_REQUEST_MAX + 1];
    char response[CMD_RES_MAX];
} cmd_worker;

static SceUID loader_thid = -1;
static SceUID loader_worker_mtx = -1;
static int loader_sockfd = -1;
static volatile int loader_stopping;
static volatile int loader_start_state;
static cmd_worker loader_workers[CMD_WORKER_MAX];

typedef struct {
    const cmd_definition* definition;
    char* args[ARG_MAX];
    size_t arg_count;
} parsed_command;

static int cmd_send_all(int socket, const char* message)
{
    unsigned int sent = 0;
    unsigned int length = (unsigned int)strlen(message);

    while (sent < length)
    {
        int result = sceNetSend(socket, message + sent, length - sent, 0);
        if (result <= 0)
            return result < 0 ? result : -1;
        sent += (unsigned int)result;
    }

    return (int)sent;
}

static int cmd_receive_request(int socket, char* request,
    unsigned int capacity)
{
    unsigned int used = 0;

    while (used < capacity)
    {
        int received = sceNetRecv(
            socket, request + used, capacity - used, 0);
        unsigned int i;

        if (received <= 0)
            return used > 0 ? (int)used : received;

        for (i = 0; i < (unsigned int)received; ++i)
        {
            if (request[used + i] == '\n' ||
                request[used + i] == '\r')
                return (int)(used + i + 1);
        }

        used += (unsigned int)received;
    }

    return (int)used;
}

static void response_append(char* response, const char* addition)
{
    size_t used = strlen(response);
    size_t available;

    if (used >= CMD_RES_MAX - 1)
        return;

    available = CMD_RES_MAX - used - 1;
    strncat(response, addition, available);
}

void cmd_handle(char* cmd, unsigned int cmd_size, char* res_msg)
{
    char* command_strings[CMD_MAX] = {0};
    parsed_command commands[CMD_MAX] = {0};
    size_t command_count = 0;
    size_t command_index;

    res_msg[0] = '\0';
    if (!parse_cmd_chain(cmd, cmd_size, command_strings, CMD_MAX,
        &command_count))
    {
        strcpy(res_msg, "Error: Too many chained commands.\n");
        return;
    }

    if (command_count == 0)
    {
        strcpy(res_msg, "Error: Empty command.\n");
        return;
    }

    for (command_index = 0; command_index < command_count; ++command_index)
    {
        parsed_command* parsed = &commands[command_index];

        parsed->arg_count = parse_cmd(command_strings[command_index],
            strlen(command_strings[command_index]), parsed->args, ARG_MAX);
        if (parsed->arg_count == 0)
        {
            strcpy(res_msg, "Error: Empty command.\n");
            return;
        }

        parsed->definition = cmd_get_definition(parsed->args[0]);
        if (parsed->definition == NULL)
        {
            strcpy(res_msg, "Error: Unknown command.\n");
            return;
        }

        if (parsed->arg_count - 1 <
                parsed->definition->min_arg_count ||
            parsed->arg_count - 1 >
                parsed->definition->max_arg_count)
        {
            strcpy(res_msg, "Error: Incorrect number of arguments.\n");
            return;
        }

        if (parsed->definition->validator &&
            !parsed->definition->validator(parsed->args,
                parsed->arg_count, res_msg))
            return;
    }

    for (command_index = 0; command_index < command_count; ++command_index)
    {
        char command_response[2048] = {0};
        parsed_command* parsed = &commands[command_index];

        parsed->definition->executor(
            parsed->args, parsed->arg_count, command_response);
        response_append(res_msg, command_response);
    }
}

/* Reads one request from `sockfd`, runs it and writes the reply. */
static void cmd_serve_request(int sockfd, char* request, char* response)
{
    int size = cmd_receive_request(sockfd, request, CMD_REQUEST_MAX);

    response[0] = '\0';
    if (size > 0)
    {
        request[size] = '\0';
        if (size == CMD_REQUEST_MAX &&
            request[size - 1] != '\n' &&
            request[size - 1] != '\r')
            strcpy(response, "Error: Command request is too long.\n");
        else
            cmd_handle(request, (unsigned int)size, response);
    }

    if (response[0] != '\0')
        cmd_send_all(sockfd, response);
}

static int cmd_worker_thread(unsigned int args, void* argp)
{
    cmd_worker* worker = *(cmd_worker**)argp;

    (void)args;

    cmd_serve_request(worker->sockfd, worker->request, worker->response);

    sceKernelLockMutex(loader_worker_mtx, 1, NULL);
    sceNetSocketClose(worker->sockfd);
    worker->sockfd = -1;
    worker->thid = -1;
    worker->busy = 0;
    sceKernelUnlockMutex(loader_worker_mtx, 1);

    sceKernelExitDeleteThread(0);
    return 0;
}

/* Returns true when the request is exactly one `reboot` command. */
static bool cmd_request_is_bare_reboot(char* request, unsigned int size)
{
    char* command_strings[2] = {0};
    char* args[2] = {0};
    size_t command_count = 0;

    if (!parse_cmd_chain(request, size, command_strings, 2, &command_count) ||
        command_count != 1)
        return false;

    return parse_cmd(command_strings[0], strlen(command_strings[0]), args, 2) == 1 &&
        strcmp(args[0], "reboot") == 0;
}

/* Called on the accept thread when every worker slot is taken. Only a bare
 * `reboot` is honoured here: it never blocks, and it is the one command a
 * wedged console must still accept. Everything else is refused quickly so
 * the accept loop stays responsive. */
static void cmd_serve_busy(int sockfd)
{
    static char request[CMD_REQUEST_MAX + 1];
    static char response[CMD_RES_MAX];
    int timeout_us = CMD_BUSY_RECV_TIMEOUT_US;
    int size;

    sceNetSetsockopt(sockfd, SCE_NET_SOL_SOCKET,
        SCE_NET_SO_RCVTIMEO, &timeout_us, sizeof(timeout_us));

    size = cmd_receive_request(sockfd, request, CMD_REQUEST_MAX);
    if (size <= 0)
        return;
    request[size] = '\0';

    if (cmd_request_is_bare_reboot(request, (unsigned int)size))
    {
        char* args[1] = { "reboot" };
        cmd_reboot(args, 1, response);
    }
    else
    {
        strcpy(response,
            "Error: Too many commands in progress; only 'reboot' is accepted "
            "until one finishes.\n");
    }

    cmd_send_all(sockfd, response);
}

static cmd_worker* cmd_worker_take(void)
{
    int i;

    for (i = 0; i < CMD_WORKER_MAX; ++i)
    {
        if (!loader_workers[i].busy)
        {
            loader_workers[i].busy = 1;
            loader_workers[i].thid = -1;
            loader_workers[i].sockfd = -1;
            return &loader_workers[i];
        }
    }

    return NULL;
}

static int cmd_worker_start(cmd_worker* worker, int sockfd)
{
    char name[48];
    SceUID thid;
    int result;

    snprintf(name, sizeof(name), "vitacompanion_cmd_worker_%d",
        (int)(worker - loader_workers));
    thid = sceKernelCreateThread(name, cmd_worker_thread, 0x40, 0x10000,
        0, 0, NULL);
    if (thid < 0)
        return thid;

    worker->sockfd = sockfd;
    worker->thid = thid;
    result = sceKernelStartThread(thid, sizeof(worker), &worker);
    if (result < 0)
    {
        sceKernelDeleteThread(thid);
        worker->sockfd = -1;
        worker->thid = -1;
        return result;
    }

    return 0;
}

int cmd_thread(unsigned int args, void* argp)
{
    struct SceNetSockaddrIn loaderaddr = {0};
    int result;

    (void)args;
    (void)argp;

    loader_sockfd = sceNetSocket("vitacompanion_cmd_sock", SCE_NET_AF_INET, SCE_NET_SOCK_STREAM, 0);
    if (loader_sockfd < 0)
        goto exit;

    loaderaddr.sin_family = SCE_NET_AF_INET;
    loaderaddr.sin_addr.s_addr = sceNetHtonl(SCE_NET_INADDR_ANY);
    loaderaddr.sin_port = sceNetHtons(CMD_PORT);

    result = sceNetBind(loader_sockfd, (struct SceNetSockaddr*)&loaderaddr, sizeof(loaderaddr));
    if (result < 0)
        goto close_and_exit;

    result = sceNetListen(loader_sockfd, 8);
    if (result < 0)
        goto close_and_exit;
    loader_start_state = 1;

    while (run && net_connected && !loader_stopping)
    {
        struct SceNetSockaddrIn clientaddr = {0};
        int client_sockfd;
        unsigned int addrlen = sizeof(clientaddr);

        client_sockfd = sceNetAccept(loader_sockfd, (struct SceNetSockaddr*)&clientaddr, &addrlen);
        if (client_sockfd >= 0)
        {
            int timeout_us = CMD_IO_TIMEOUT_US;
            cmd_worker* worker;
            int started = -1;

            sceNetSetsockopt(client_sockfd, SCE_NET_SOL_SOCKET,
                SCE_NET_SO_SNDTIMEO, &timeout_us, sizeof(timeout_us));
            sceNetSetsockopt(client_sockfd, SCE_NET_SOL_SOCKET,
                SCE_NET_SO_RCVTIMEO, &timeout_us, sizeof(timeout_us));

            sceKernelLockMutex(loader_worker_mtx, 1, NULL);
            if (loader_stopping)
            {
                sceKernelUnlockMutex(loader_worker_mtx, 1);
                sceNetSocketClose(client_sockfd);
                break;
            }
            worker = cmd_worker_take();
            if (worker)
            {
                started = cmd_worker_start(worker, client_sockfd);
                if (started < 0)
                    worker->busy = 0;
            }
            sceKernelUnlockMutex(loader_worker_mtx, 1);

            if (started >= 0)
                continue;

            if (worker == NULL)
                cmd_serve_busy(client_sockfd);
            else
                cmd_send_all(client_sockfd,
                    "Error: Could not start a command worker.\n");
            sceNetSocketClose(client_sockfd);
        }
        else if (loader_stopping)
        {
            break;
        }
        else
        {
            sceKernelDelayThread(100 * 1000);
        }
    }

    goto exit;

close_and_exit:
    sceNetSocketClose(loader_sockfd);
    loader_sockfd = -1;

exit:
    if (loader_start_state == 0)
        loader_start_state = -1;
    sceKernelExitDeleteThread(0);
    return 0;
}

static int cmd_workers_idle(void)
{
    int i;

    for (i = 0; i < CMD_WORKER_MAX; ++i)
        if (loader_workers[i].busy)
            return 0;
    return 1;
}

int cmd_start()
{
    int result;
    int waited_ms;

    if (loader_thid >= 0)
        return -1;

    /* The worker mutex outlives cmd_end() while a worker is still stuck in
     * an executor, so only create it when no previous instance survives. */
    if (loader_worker_mtx < 0)
    {
        loader_worker_mtx = sceKernelCreateMutex(
            "vitacompanion_cmd_worker_mutex", 0, 0, NULL);
        if (loader_worker_mtx < 0)
            return loader_worker_mtx;
    }

    loader_thid = sceKernelCreateThread("vitacompanion_cmd_thread", cmd_thread, 0x40, 0x10000, 0, 0, NULL);
    if (loader_thid < 0)
    {
        result = loader_thid;
        if (cmd_workers_idle())
        {
            sceKernelDeleteMutex(loader_worker_mtx);
            loader_worker_mtx = -1;
        }
        return result;
    }

    loader_sockfd = -1;
    loader_stopping = 0;
    loader_start_state = 0;
    result = sceKernelStartThread(loader_thid, 0, NULL);
    if (result < 0)
    {
        sceKernelDeleteThread(loader_thid);
        loader_thid = -1;
        if (cmd_workers_idle())
        {
            sceKernelDeleteMutex(loader_worker_mtx);
            loader_worker_mtx = -1;
        }
        return result;
    }

    for (waited_ms = 0;
        waited_ms < CMD_START_TIMEOUT_MS && loader_start_state == 0;
        waited_ms += 10)
        sceKernelDelayThread(10 * 1000);

    if (loader_start_state != 1)
    {
        loader_stopping = 1;
        if (loader_sockfd >= 0)
            sceNetSocketClose(loader_sockfd);
        sceKernelWaitThreadEnd(loader_thid, NULL, NULL);
        loader_thid = -1;
        loader_sockfd = -1;
        if (cmd_workers_idle())
        {
            sceKernelDeleteMutex(loader_worker_mtx);
            loader_worker_mtx = -1;
        }
        return -1;
    }

    return 0;
}

void cmd_end()
{
    const int abort_flags = SCE_NET_SOCKET_ABORT_FLAG_RCV_PRESERVATION |
        SCE_NET_SOCKET_ABORT_FLAG_SND_PRESERVATION;
    SceUID worker_thids[CMD_WORKER_MAX];
    int i;

    if (loader_thid < 0)
        return;

    loader_stopping = 1;
    if (loader_sockfd >= 0)
        sceNetSocketClose(loader_sockfd);

    /* Abort every in-flight client socket so workers blocked on network I/O
     * return, then let the accept loop drain. */
    sceKernelLockMutex(loader_worker_mtx, 1, NULL);
    for (i = 0; i < CMD_WORKER_MAX; ++i)
    {
        worker_thids[i] = loader_workers[i].busy ? loader_workers[i].thid : -1;
        if (loader_workers[i].busy && loader_workers[i].sockfd >= 0)
            sceNetSocketAbort(loader_workers[i].sockfd, abort_flags);
    }
    sceKernelUnlockMutex(loader_worker_mtx, 1);

    sceKernelWaitThreadEnd(loader_thid, NULL, NULL);

    /* A worker stuck inside an executor cannot be interrupted; give each one
     * a bounded grace period and otherwise leave it to finish on its own.
     * Its slot stays marked busy until it does. */
    for (i = 0; i < CMD_WORKER_MAX; ++i)
    {
        SceUInt timeout_us = CMD_WORKER_STOP_TIMEOUT_US;

        if (worker_thids[i] >= 0)
            sceKernelWaitThreadEnd(worker_thids[i], NULL, &timeout_us);
    }

    loader_thid = -1;
    loader_sockfd = -1;
    loader_start_state = 0;

    if (cmd_workers_idle())
    {
        sceKernelDeleteMutex(loader_worker_mtx);
        loader_worker_mtx = -1;
    }
}
