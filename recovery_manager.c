#include "ecu_shared.h"
#include "ecu_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/neutrino.h>
#include <sys/dispatch.h>
#include <sys/iofunc.h>
#include <sys/iomsg.h>

#include <signal.h>
#include <unistd.h>
#include <spawn.h>


#define ENGINE_PROGRAM_PATH      "/tmp/engine_ecu"
#define BRAKE_PROGRAM_PATH       "/tmp/brake_ecu"
#define STEERING_PROGRAM_PATH    "/tmp/steering_ecu"


/* ---------------------------------------------------------
 * Get executable path
 * --------------------------------------------------------- */

static const char *get_program_path(int ecu_id)
{
    if (ecu_id == ECU_ENGINE)
        return ENGINE_PROGRAM_PATH;

    if (ecu_id == ECU_BRAKE)
        return BRAKE_PROGRAM_PATH;

    if (ecu_id == ECU_STEERING)
        return STEERING_PROGRAM_PATH;

    return NULL;
}


/* ---------------------------------------------------------
 * Get service name
 * --------------------------------------------------------- */

static const char *get_service_name(int ecu_id)
{
    if (ecu_id == ECU_ENGINE)
        return ENGINE_ECU_NAME;

    if (ecu_id == ECU_BRAKE)
        return BRAKE_ECU_NAME;

    if (ecu_id == ECU_STEERING)
        return STEERING_ECU_NAME;

    return NULL;
}


/* ---------------------------------------------------------
 * Stop failed ECU
 * --------------------------------------------------------- */

static int stop_ecu(pid_t pid)
{
    if (pid <= 0)
    {
        return -1;
    }

    printf("[RECOVERY] Stopping failed PID %d\n",
           pid);

    fflush(stdout);

    if (kill(pid, SIGTERM) == -1)
    {
        printf("[RECOVERY] SIGTERM failed: %s\n",
               strerror(errno));
    }

    ecu_sleep_ms(RECOVERY_STOP_WAIT_MS);

    if (kill(pid, 0) == 0)
    {
        printf("[RECOVERY] ECU still running - sending SIGKILL\n");

        fflush(stdout);

        if (kill(pid, SIGKILL) == -1)
        {
            printf("[RECOVERY] SIGKILL failed: %s\n",
                   strerror(errno));

            return -1;
        }

        ecu_sleep_ms(100);
    }

    return 0;
}


/* ---------------------------------------------------------
 * Start ECU
 * --------------------------------------------------------- */

static pid_t start_ecu(int ecu_id)
{
    const char *program;
    pid_t pid;

    program =
        get_program_path(ecu_id);

    if (program == NULL)
    {
        return -1;
    }

    printf("[RECOVERY] Starting %s\n",
           program);

    fflush(stdout);

    pid =
        spawnl(P_NOWAIT,
               program,
               program,
               NULL);

    if (pid == -1)
    {
        printf("[RECOVERY] Start failed: %s\n",
               strerror(errno));

        fflush(stdout);

        return -1;
    }

    printf("[RECOVERY] New PID = %d\n",
           pid);

    fflush(stdout);

    return pid;
}


/* ---------------------------------------------------------
 * Verify ECU service
 * --------------------------------------------------------- */

static int wait_for_ecu(int ecu_id)
{
    const char *service;
    int connection;
    int attempt;

    service =
        get_service_name(ecu_id);

    if (service == NULL)
    {
        return -1;
    }

    for (attempt = 0;
         attempt < RECOVERY_VERIFY_ATTEMPTS;
         attempt++)
    {
        connection =
            name_open(service, 0);

        if (connection != -1)
        {
            ecu_message_t request;
            ecu_message_t reply;

            memset(&request, 0, sizeof(request));
            memset(&reply, 0, sizeof(reply));

            request.type =
                MSG_STATUS_QUERY;

            request.ecu_id =
                (uint16_t)ecu_id;

            if (MsgSend(connection,
                        &request,
                        sizeof(request),
                        &reply,
                        sizeof(reply)) != -1)
            {
                name_close(connection);

                return 0;
            }

            name_close(connection);
        }

        ecu_sleep_ms(RECOVERY_VERIFY_WAIT_MS);
    }

    return -1;
}


/* ---------------------------------------------------------
 * Recover ECU
 * --------------------------------------------------------- */

static void recover_ecu(recovery_message_t *message)
{
    pid_t new_pid;

    printf("\n");
    printf("[RECOVERY] Starting recovery for ");

    if (message->ecu_id == ECU_ENGINE)
        printf("ENGINE ECU\n");
    else if (message->ecu_id == ECU_BRAKE)
        printf("BRAKE ECU\n");
    else if (message->ecu_id == ECU_STEERING)
        printf("STEERING ECU\n");

    printf("[RECOVERY] Failed PID = %d\n",
           message->failed_pid);

    printf("\n");
    fflush(stdout);


    /* -----------------------------------------------------
     * Step 1
     * ----------------------------------------------------- */

    printf("[RECOVERY] Step 1/4 : Stopping failed ECU...\n");
    fflush(stdout);

    if (stop_ecu(message->failed_pid) != 0)
    {
        printf("[RECOVERY] Failed to stop ECU\n");
        fflush(stdout);
        return;
    }

    /*
     * Give the stopped ECU time to completely disappear.
     */
    ecu_sleep_ms(1000);


    /* -----------------------------------------------------
     * Step 2
     * ----------------------------------------------------- */

    printf("[RECOVERY] Step 2/4 : ECU stopped\n");
    fflush(stdout);

    /*
     * Keep the ECU in stopped state for a visible period.
     */
    ecu_sleep_ms(1000);


    /* -----------------------------------------------------
     * Step 3
     * ----------------------------------------------------- */

    printf("[RECOVERY] Step 3/4 : Starting new ECU...\n");
    fflush(stdout);

    new_pid = start_ecu(message->ecu_id);

    if (new_pid == -1)
    {
        printf("[RECOVERY] ECU restart failed\n");
        fflush(stdout);
        return;
    }

    /*
     * Give the new ECU time to initialise and
     * register its QNX service.
     */
    ecu_sleep_ms(1000);


    /* -----------------------------------------------------
     * Step 4
     * ----------------------------------------------------- */

    printf("[RECOVERY] Step 4/4 : Verifying heartbeat...\n");
    fflush(stdout);

    /*
     * wait_for_ecu() checks whether the replacement
     * ECU service is alive.
     */
    if (wait_for_ecu(message->ecu_id) == 0)
    {
        printf("\n");

        if (message->ecu_id == ECU_ENGINE)
            printf("[RECOVERY] ENGINE ECU RECOVERED SUCCESSFULLY\n");
        else if (message->ecu_id == ECU_BRAKE)
            printf("[RECOVERY] BRAKE ECU RECOVERED SUCCESSFULLY\n");
        else if (message->ecu_id == ECU_STEERING)
            printf("[RECOVERY] STEERING ECU RECOVERED SUCCESSFULLY\n");

        printf("\n");
    }
    else
    {
        printf("[RECOVERY] ECU verification failed\n");
    }

    fflush(stdout);
}


/* ---------------------------------------------------------
 * Main
 * --------------------------------------------------------- */

int main(int argc, char *argv[])
{
    name_attach_t *channel;

    char receive_buffer[256];

    uint16_t message_type;

    int rcvid;

    (void)argc;
    (void)argv;


    channel =
        name_attach(NULL,
                    RECOVERY_NAME,
                    0);

    if (channel == NULL)
    {
        printf("[RECOVERY] name_attach failed: %s\n",
               strerror(errno));

        return EXIT_FAILURE;
    }


    printf("[RECOVERY MANAGER] STARTED | PID=%d\n",
           getpid());

    fflush(stdout);


    while (1)
    {
        recovery_message_t message;

        memset(receive_buffer, 0, sizeof(receive_buffer));

        rcvid =
            MsgReceive(channel->chid,
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


        if (message_type == MSG_RECOVERY)
        {
            memcpy(&message,
                   receive_buffer,
                   sizeof(message));

            /*
             * Acknowledge the watchdog first.
             * Recovery itself is performed afterward.
             */

            MsgReply(rcvid, EOK, NULL, 0);

            recover_ecu(&message);

            continue;
        }


        MsgError(rcvid, ENOSYS);
    }

    return EXIT_SUCCESS;
}
