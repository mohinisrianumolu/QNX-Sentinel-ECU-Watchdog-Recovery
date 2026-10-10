#include "ecu_shared.h"
#include "ecu_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdatomic.h>

#include <sys/neutrino.h>
#include <sys/dispatch.h>
#include <sys/iofunc.h>
#include <sys/iomsg.h>

#include <unistd.h>


static atomic_int heartbeat_enabled = 1;

static int watchdog_connection = -1;

static name_attach_t *engine_channel = NULL;


/* ---------------------------------------------------------
 * Send heartbeat to watchdog
 * --------------------------------------------------------- */

static void send_heartbeat(unsigned long sequence)
{
    ecu_message_t message;

    memset(&message, 0, sizeof(message));

    message.type = MSG_HEARTBEAT;
    message.ecu_id = ECU_ENGINE;
    message.sequence = sequence;
    message.sender_pid = getpid();

    if (watchdog_connection == -1)
    {
        watchdog_connection =
            name_open(WATCHDOG_NAME, 0);
    }

    if (watchdog_connection != -1)
    {
        if (MsgSend(watchdog_connection,
                    &message,
                    sizeof(message),
                    NULL,
                    0) == -1)
        {
            name_close(watchdog_connection);
            watchdog_connection = -1;
        }
    }
}


/* ---------------------------------------------------------
 * IPC thread
 * --------------------------------------------------------- */

static void *ipc_thread(void *argument)
{
    char receive_buffer[256];
    uint16_t message_type;
    int rcvid;

    (void)argument;

    while (1)
    {
        memset(receive_buffer, 0, sizeof(receive_buffer));

        rcvid = MsgReceive(engine_channel->chid,
                           receive_buffer,
                           sizeof(receive_buffer),
                           NULL);

        if (rcvid == -1)
        {
            continue;
        }

        if (rcvid == 0)
        {
            continue;
        }

        memcpy(&message_type,
               receive_buffer,
               sizeof(message_type));

        if (message_type == _IO_CONNECT)
        {
            MsgReply(rcvid, EOK, NULL, 0);
            continue;
        }

        if (message_type == MSG_FAULT_INJECT)
        {
            fault_message_t fault;

            memset(&fault, 0, sizeof(fault));

            memcpy(&fault,
                   receive_buffer,
                   sizeof(fault));

            if (fault.fault_type == FAULT_HEARTBEAT)
            {
                printf("[ENGINE ECU] HEARTBEAT FAILURE INJECTED\n");
                fflush(stdout);

                atomic_store(&heartbeat_enabled, 0);
            }

            MsgReply(rcvid, EOK, NULL, 0);
            continue;
        }

        if (message_type == MSG_STATUS_QUERY)
        {
            ecu_message_t reply;

            memset(&reply, 0, sizeof(reply));

            reply.type = MSG_STATUS_REPLY;
            reply.ecu_id = ECU_ENGINE;
            reply.sender_pid = getpid();

            MsgReply(rcvid,
                     EOK,
                     &reply,
                     sizeof(reply));

            continue;
        }

        MsgError(rcvid, ENOSYS);
    }

    return NULL;
}


/* ---------------------------------------------------------
 * Main
 * --------------------------------------------------------- */

int main(int argc, char *argv[])
{
    pthread_t ipc_thread_id;
    unsigned long sequence;

    (void)argc;
    (void)argv;

    engine_channel =
        name_attach(NULL,
                    ENGINE_ECU_NAME,
                    0);

    if (engine_channel == NULL)
    {
        printf("[ENGINE ECU] name_attach failed: %s\n",
               strerror(errno));

        return EXIT_FAILURE;
    }

    printf("[ENGINE ECU] STARTED | PID=%d\n",
           getpid());

    fflush(stdout);

    if (pthread_create(&ipc_thread_id,
                       NULL,
                       ipc_thread,
                       NULL) != 0)
    {
        printf("[ENGINE ECU] IPC thread creation failed\n");

        name_detach(engine_channel, 0);

        return EXIT_FAILURE;
    }

    sequence = 0;

    while (1)
    {
        sequence++;

        if (atomic_load(&heartbeat_enabled))
        {
            send_heartbeat(sequence);
        }

        ecu_sleep_ms(HEARTBEAT_PERIOD_MS);
    }

    return EXIT_SUCCESS;
}
