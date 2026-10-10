#ifndef ECU_SHARED_H
#define ECU_SHARED_H

#include <stdint.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>

/* ---------------------------------------------------------
 * Service names
 * --------------------------------------------------------- */

#define ENGINE_ECU_NAME       "engine_ecu"
#define BRAKE_ECU_NAME        "brake_ecu"
#define STEERING_ECU_NAME     "steering_ecu"

#define WATCHDOG_NAME         "ecu_watchdog"
#define RECOVERY_NAME         "ecu_recovery"


/* ---------------------------------------------------------
 * ECU identifiers
 * --------------------------------------------------------- */

#define ECU_ENGINE            1
#define ECU_BRAKE             2
#define ECU_STEERING          3


/* ---------------------------------------------------------
 * Fault types
 * --------------------------------------------------------- */

#define FAULT_NONE             0
#define FAULT_HEARTBEAT        1
#define FAULT_DEADLOCK         2
#define FAULT_OVERRUN          3


/* ---------------------------------------------------------
 * IPC message types
 * --------------------------------------------------------- */

#define MSG_HEARTBEAT          1000
#define MSG_FAULT_INJECT       1001
#define MSG_TASK_COMPLETE      1002
#define MSG_STATUS_QUERY       1003
#define MSG_STATUS_REPLY       1004
#define MSG_RECOVERY           1005


/* ---------------------------------------------------------
 * Generic ECU message
 * --------------------------------------------------------- */

typedef struct
{
    uint16_t type;
    uint16_t ecu_id;

    uint32_t sequence;
    uint32_t value_ms;

    pid_t sender_pid;

} ecu_message_t;


/* ---------------------------------------------------------
 * Fault injection message
 * --------------------------------------------------------- */

typedef struct
{
    uint16_t type;
    uint16_t ecu_id;

    uint16_t fault_type;
    uint16_t reserved;

    uint32_t press_count;

} fault_message_t;


/* ---------------------------------------------------------
 * Recovery request
 * --------------------------------------------------------- */

typedef struct
{
    uint16_t type;
    uint16_t ecu_id;

    uint16_t fault_type;
    uint16_t reserved;

    pid_t failed_pid;

} recovery_message_t;


/* ---------------------------------------------------------
 * Current monotonic time in milliseconds
 * --------------------------------------------------------- */

static inline uint64_t ecu_now_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return ((uint64_t)ts.tv_sec * 1000ULL) +
           ((uint64_t)ts.tv_nsec / 1000000ULL);
}


/* ---------------------------------------------------------
 * Sleep helper
 * --------------------------------------------------------- */

static inline void ecu_sleep_ms(uint32_t milliseconds)
{
    struct timespec request;

    request.tv_sec =
        milliseconds / 1000U;

    request.tv_nsec =
        (long)(milliseconds % 1000U) * 1000000L;

    while (nanosleep(&request, &request) == -1)
    {
        if (errno != EINTR)
        {
            break;
        }
    }
}

#endif
