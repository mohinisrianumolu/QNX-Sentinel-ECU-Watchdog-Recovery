#include "ecu_shared.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/neutrino.h>
#include <sys/dispatch.h>

#include <unistd.h>


/* ---------------------------------------------------------
 * Send fault to ECU
 * --------------------------------------------------------- */

static int send_fault(const char *ecu_name,
                      int ecu_id,
                      int fault_type,
                      unsigned int press_count)
{
    int connection;

    fault_message_t message;


    connection =
        name_open(ecu_name, 0);

    if (connection == -1)
    {
        printf("FAULT: Cannot connect to %s: %s\n",
               ecu_name,
               strerror(errno));

        return -1;
    }


    memset(&message, 0, sizeof(message));

    message.type =
        MSG_FAULT_INJECT;

    message.ecu_id =
        (uint16_t)ecu_id;

    message.fault_type =
        (uint16_t)fault_type;

    message.press_count =
        press_count;


    if (MsgSend(connection,
                &message,
                sizeof(message),
                NULL,
                0) == -1)
    {
        printf("FAULT: MsgSend failed: %s\n",
               strerror(errno));

        name_close(connection);

        return -1;
    }


    name_close(connection);

    return 0;
}


/* ---------------------------------------------------------
 * Main
 * --------------------------------------------------------- */

int main(int argc, char *argv[])
{
    int character;

    unsigned int engine_count;
    unsigned int brake_count;
    unsigned int steering_count;

    (void)argc;
    (void)argv;


    engine_count = 0;
    brake_count = 0;
    steering_count = 0;


    printf("\n");
    printf("============================================================\n");
    printf("                    FAULT INJECTOR\n");
    printf("============================================================\n");

    printf("\n");
    printf("1 = Engine ECU heartbeat failure\n");
    printf("2 = Brake ECU deadlock\n");
    printf("3 = Steering ECU execution overrun\n");
    printf("q = Quit\n");
    printf("\n");

    fflush(stdout);


    while (1)
    {
        printf("FAULT> ");
        fflush(stdout);

        character =
            getchar();


        if (character == EOF)
        {
            break;
        }


        if (character == '\n')
        {
            continue;
        }


        /* -------------------------------------------------
         * Engine fault
         * ------------------------------------------------- */

        if (character == '1')
        {
            engine_count++;

            printf("\n");
            printf("FAULT: Engine ECU heartbeat failure\n");
            printf("FAULT: Injection count = %u\n",
                   engine_count);

            fflush(stdout);

            send_fault(ENGINE_ECU_NAME,
                       ECU_ENGINE,
                       FAULT_HEARTBEAT,
                       engine_count);

            continue;
        }


        /* -------------------------------------------------
         * Brake fault
         * ------------------------------------------------- */

        if (character == '2')
        {
            brake_count++;

            printf("\n");
            printf("FAULT: Brake ECU deadlock\n");
            printf("FAULT: Injection count = %u\n",
                   brake_count);

            fflush(stdout);

            send_fault(BRAKE_ECU_NAME,
                       ECU_BRAKE,
                       FAULT_DEADLOCK,
                       brake_count);

            continue;
        }


        /* -------------------------------------------------
         * Steering fault
         * ------------------------------------------------- */

        if (character == '3')
        {
            steering_count++;

            printf("\n");
            printf("FAULT: Steering ECU execution overrun\n");
            printf("FAULT: Injection count = %u\n",
                   steering_count);

            fflush(stdout);

            send_fault(STEERING_ECU_NAME,
                       ECU_STEERING,
                       FAULT_OVERRUN,
                       steering_count);

            continue;
        }


        /* -------------------------------------------------
         * Quit
         * ------------------------------------------------- */

        if (character == 'q' ||
            character == 'Q')
        {
            printf("\nFAULT: Injector stopped\n");

            break;
        }


        printf("FAULT: Unknown command\n");
    }


    return EXIT_SUCCESS;
}
