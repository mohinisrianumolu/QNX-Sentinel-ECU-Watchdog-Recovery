#include "ecu_shared.h"
#include "ecu_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>

#include <sys/neutrino.h>
#include <sys/dispatch.h>
#include <sys/iofunc.h>
#include <sys/iomsg.h>

#include <unistd.h>


static atomic_int overrun_requested = 0;

static int watchdog_connection = -1;

static name_attach_t *steering_channel = NULL;


/* ---------------------------------------------------------
 * Send heartbeat
 * --------------------------------------------------------- */

static void send_heartbeat(unsigned long sequence)
{
    ecu_message_t message;

    memset(&message, 0, sizeof(message));

    message.type = MSG_HEARTBEAT;
    message.ecu_id = ECU_STEERING;
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
 * Report task execution time
 * --------------------------------------------------------- */

static void send_task_completion(unsigned long sequence,
                                  uint32_t execution_time)
{
    ecu_message_t message;

    memset(&message, 0, sizeof(message));

    message.type = MSG_TASK_COMPLETE;
    message.ecu_id = ECU_STEERING;
    message.sequence = sequence;
    message.value_ms = execution_time;
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
 * Simulated steering control calculation
 * --------------------------------------------------------- */

static void steering_control_step(void)
{
    int target_angle;
    int current_angle;
    int error;
    int control_output;

    target_angle = 30;
    current_angle = 25;

    error = target_angle - current_angle;

    control_output = error * 10;

    (void)control_output;
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

        rcvid = MsgReceive(steering_channel->chid,
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

            if (fault.fault_type == FAULT_OVERRUN)
            {
                printf("[STEERING ECU] EXECUTION OVERRUN INJECTED\n");
                fflush(stdout);

                atomic_store(&overrun_requested, 1);
            }

            MsgReply(rcvid, EOK, NULL, 0);
            continue;
        }

        if (message_type == MSG_STATUS_QUERY)
        {
            ecu_message_t reply;

            memset(&reply, 0, sizeof(reply));

            reply.type = MSG_STATUS_REPLY;
            reply.ecu_id = ECU_STEERING;
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
 * Steering task
 * --------------------------------------------------------- */

static void *steering_task(void *argument)
{
    unsigned long sequence;
    uint64_t start_time;
    uint64_t end_time;
    uint32_t execution_time;

    (void)argument;

    sequence = 0;

    while (1)
    {
        sequence++;

        start_time = ecu_now_ms();

        steering_control_step();

        if (atomic_exchange(&overrun_requested, 0))
        {
            ecu_sleep_ms(STEERING_FAULT_TIME_MS);
        }

        end_time = ecu_now_ms();

        execution_time =
            (uint32_t)(end_time - start_time);

        send_task_completion(sequence,
                             execution_time);

        ecu_sleep_ms(HEARTBEAT_PERIOD_MS);
    }

    return NULL;
}


/* ---------------------------------------------------------
 * Main
 * --------------------------------------------------------- */

int main(int argc, char *argv[])
{
    pthread_t ipc_thread_id;
    pthread_t steering_thread_id;
    unsigned long heartbeat_sequence;

    (void)argc;
    (void)argv;

    steering_channel =
        name_attach(NULL,
                    STEERING_ECU_NAME,
                    0);

    if (steering_channel == NULL)
    {
        printf("[STEERING ECU] name_attach failed: %s\n",
               strerror(errno));

        return EXIT_FAILURE;
    }

    printf("[STEERING ECU] STARTED | PID=%d\n",
           getpid());

    fflush(stdout);

    if (pthread_create(&ipc_thread_id,
                       NULL,
                       ipc_thread,
                       NULL) != 0)
    {
        printf("[STEERING ECU] IPC thread creation failed\n");

        name_detach(steering_channel, 0);

        return EXIT_FAILURE;
    }

    if (pthread_create(&steering_thread_id,
                       NULL,
                       steering_task,
                       NULL) != 0)
    {
        printf("[STEERING ECU] steering task creation failed\n");

        name_detach(steering_channel, 0);

        return EXIT_FAILURE;
    }

    heartbeat_sequence = 0;

    while (1)
    {
        heartbeat_sequence++;

        send_heartbeat(heartbeat_sequence);

        ecu_sleep_ms(HEARTBEAT_PERIOD_MS);
    }

    return EXIT_SUCCESS;
}
