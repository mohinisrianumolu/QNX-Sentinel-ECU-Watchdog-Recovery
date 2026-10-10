#include "ecu_shared.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <spawn.h>

#include <unistd.h>


#define RECOVERY_PROGRAM      "/tmp/recovery_manager"
#define WATCHDOG_PROGRAM      "/tmp/watchdog"
#define ENGINE_PROGRAM        "/tmp/engine_ecu"
#define BRAKE_PROGRAM         "/tmp/brake_ecu"
#define STEERING_PROGRAM      "/tmp/steering_ecu"


/* ---------------------------------------------------------
 * Start process
 * --------------------------------------------------------- */

static pid_t start_process(const char *program)
{
    pid_t pid;

    pid =
        spawnl(P_NOWAIT,
               program,
               program,
               NULL);

    if (pid == -1)
    {
        printf("[LAUNCHER] Failed to start %s: %s\n",
               program,
               strerror(errno));

        return -1;
    }

    printf("[LAUNCHER] Started %s | PID=%d\n",
           program,
           pid);

    fflush(stdout);

    return pid;
}


/* ---------------------------------------------------------
 * Main
 * --------------------------------------------------------- */

int main(int argc, char *argv[])
{
    pid_t recovery_pid;
    pid_t watchdog_pid;
    pid_t engine_pid;
    pid_t brake_pid;
    pid_t steering_pid;

    (void)argc;
    (void)argv;


    printf("\n");
    printf("============================================================\n");
    printf("          AUTOMOTIVE ECU SUPERVISORY FRAMEWORK\n");
    printf("============================================================\n\n");

    fflush(stdout);


    /* -----------------------------------------------------
     * Recovery manager
     * ----------------------------------------------------- */

    recovery_pid =
        start_process(RECOVERY_PROGRAM);

    if (recovery_pid == -1)
        return EXIT_FAILURE;

    ecu_sleep_ms(300);


    /* -----------------------------------------------------
     * Watchdog
     * ----------------------------------------------------- */

    watchdog_pid =
        start_process(WATCHDOG_PROGRAM);

    if (watchdog_pid == -1)
        return EXIT_FAILURE;

    ecu_sleep_ms(300);


    /* -----------------------------------------------------
     * Engine
     * ----------------------------------------------------- */

    engine_pid =
        start_process(ENGINE_PROGRAM);

    if (engine_pid == -1)
        return EXIT_FAILURE;

    ecu_sleep_ms(200);


    /* -----------------------------------------------------
     * Brake
     * ----------------------------------------------------- */

    brake_pid =
        start_process(BRAKE_PROGRAM);

    if (brake_pid == -1)
        return EXIT_FAILURE;

    ecu_sleep_ms(200);


    /* -----------------------------------------------------
     * Steering
     * ----------------------------------------------------- */

    steering_pid =
        start_process(STEERING_PROGRAM);

    if (steering_pid == -1)
        return EXIT_FAILURE;


    printf("\n");
    printf("============================================================\n");
    printf("                ALL SERVICES STARTED\n");
    printf("============================================================\n");

    printf("Engine ECU     PID : %d\n",
           engine_pid);

    printf("Brake ECU      PID : %d\n",
           brake_pid);

    printf("Steering ECU   PID : %d\n",
           steering_pid);

    printf("Watchdog       PID : %d\n",
           watchdog_pid);

    printf("Recovery       PID : %d\n",
           recovery_pid);

    printf("============================================================\n\n");

    fflush(stdout);


    while (1)
    {
        ecu_sleep_ms(1000);
    }


    return EXIT_SUCCESS;
}
