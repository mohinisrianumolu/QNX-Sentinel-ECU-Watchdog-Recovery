#include "ecu_shared.h"
#include "ecu_config.h"
#include "gpio_hal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <spawn.h>

#include <sys/neutrino.h>
#include <sys/dispatch.h>
#include <sys/iofunc.h>
#include <sys/iomsg.h>
#include <unistd.h>

#define TIMEOUT_THRESHOLD_MS 1000

typedef struct {
    int ecu_id;
    const char *name;
    const char *binary_name;
    pid_t pid;
    pid_t failed_pid;
    uint64_t last_heartbeat;
    int heartbeat_received;
    int recovery_pending;
} ecu_state_t;

static ecu_state_t engine_state;
static ecu_state_t brake_state;
static ecu_state_t steering_state;

static name_attach_t *watchdog_channel = NULL;
static int recovery_connection = -1;
static pthread_mutex_t output_mutex = PTHREAD_MUTEX_INITIALIZER;

static ecu_state_t *get_ecu_state(int ecu_id) {
    if (ecu_id == ECU_ENGINE)   return &engine_state;
    if (ecu_id == ECU_BRAKE)    return &brake_state;
    if (ecu_id == ECU_STEERING) return &steering_state;
    return NULL;
}

static void print_health_status(void) {
    pthread_mutex_lock(&output_mutex);
    const char *e = engine_state.recovery_pending   ? "RECOVERY IN PROGRESS " :
                   (engine_state.heartbeat_received ? "HEARTBEAT RECEIVED   " : "WAITING             ");
    const char *b = brake_state.recovery_pending    ? "RECOVERY IN PROGRESS " :
                   (brake_state.heartbeat_received  ? "HEARTBEAT RECEIVED   " : "WAITING             ");
    const char *s = steering_state.recovery_pending ? "RECOVERY IN PROGRESS " :
                   (steering_state.heartbeat_received ? "HEARTBEAT RECEIVED   " : "WAITING             ");

    printf("[HEALTH] ENGINE ECU : %s | BRAKE ECU : %s | STEERING ECU : %s\n", e, b, s);
    fflush(stdout);
    pthread_mutex_unlock(&output_mutex);
}

static int connect_to_recovery(void) {
    if (recovery_connection != -1) return recovery_connection;
    recovery_connection = name_open(RECOVERY_NAME, 0);
    return recovery_connection;
}

typedef struct {
    ecu_state_t *state;
    int fault_type;
    pid_t old_pid;
} recovery_param_t;

static void *recovery_worker_thread(void *arg) {
    recovery_param_t *param = (recovery_param_t *)arg;
    ecu_state_t *state = param->state;
    int ecu_id = state->ecu_id;
    pid_t old_pid = param->old_pid;

    // 1. Hold RED LED for 1.2s for visual fault verification
    ecu_sleep_ms(1200);

    // 2. Switch to YELLOW LED for the entire recovery duration
    if (ecu_id == ECU_ENGINE)        gpio_set_engine_recovery();
    else if (ecu_id == ECU_BRAKE)   gpio_set_brake_recovery();
    else if (ecu_id == ECU_STEERING) gpio_set_steering_recovery();

    pthread_mutex_lock(&output_mutex);
    printf("\n[RECOVERY] Starting recovery for %s\n", state->name);
    printf("[RECOVERY] Failed PID = %d\n\n", old_pid);
    printf("[RECOVERY] Step 1/4 : Stopping failed ECU...\n\n");
    fflush(stdout);
    pthread_mutex_unlock(&output_mutex);

    // Terminate failed process
    if (old_pid > 0) {
        kill(old_pid, SIGKILL);
    }
    ecu_sleep_ms(400);

    pthread_mutex_lock(&output_mutex);
    printf("[RECOVERY] Step 2/4 : ECU stopped\n\n");
    fflush(stdout);
    pthread_mutex_unlock(&output_mutex);

    print_health_status();
    printf("\n");

    pthread_mutex_lock(&output_mutex);
    printf("[RECOVERY] Step 3/4 : Starting new ECU...\n\n");
    fflush(stdout);
    pthread_mutex_unlock(&output_mutex);

    // Dispatch process restart to recovery_manager or direct spawn fallback
    int coid = connect_to_recovery();
    if (coid != -1) {
        recovery_message_t msg;
        memset(&msg, 0, sizeof(msg));
        msg.type = MSG_RECOVERY;
        msg.ecu_id = ecu_id;
        msg.fault_type = param->fault_type;
        msg.failed_pid = old_pid;
        MsgSend(coid, &msg, sizeof(msg), NULL, 0);
    } else {
        pid_t new_pid;
        char path[128];
        snprintf(path, sizeof(path), "./%s", state->binary_name);
        char *args[] = { (char *)state->binary_name, NULL };
        posix_spawnp(&new_pid, path, NULL, NULL, args, NULL);
    }

    print_health_status();
    printf("\n");

    pthread_mutex_lock(&output_mutex);
    printf("[RECOVERY] Step 4/4 : Verifying heartbeat...\n\n");
    fflush(stdout);
    pthread_mutex_unlock(&output_mutex);

    print_health_status();
    printf("\n");

    // The Yellow LED remains active here.
    // Transition to Green occurs only when message_thread receives a verified heartbeat.

    free(param);
    return NULL;
}

static void trigger_recovery(ecu_state_t *state, int fault_type) {
    if (state == NULL || state->recovery_pending) return;

    state->recovery_pending = 1;
    pid_t dying_pid = state->pid;
    state->failed_pid = dying_pid;

    // Immediately kill dying process to avoid trailing messages
    if (dying_pid > 0) {
        kill(dying_pid, SIGKILL);
    }

    // Immediately latch RED LED
    if (state->ecu_id == ECU_ENGINE)        gpio_set_engine_fault();
    else if (state->ecu_id == ECU_BRAKE)   gpio_set_brake_fault();
    else if (state->ecu_id == ECU_STEERING) gpio_set_steering_fault();

    recovery_param_t *param = malloc(sizeof(recovery_param_t));
    if (!param) return;
    param->state = state;
    param->fault_type = fault_type;
    param->old_pid = dying_pid;

    pthread_t th;
    if (pthread_create(&th, NULL, recovery_worker_thread, param) == 0) {
        pthread_detach(th);
    } else {
        free(param);
        state->recovery_pending = 0;
    }
}

static void *message_thread(void *argument) {
    char receive_buffer[256];
    uint16_t message_type;
    int rcvid;
    (void)argument;

    while (1) {
        memset(receive_buffer, 0, sizeof(receive_buffer));
        rcvid = MsgReceive(watchdog_channel->chid, receive_buffer, sizeof(receive_buffer), NULL);

        if (rcvid <= 0) continue;

        memcpy(&message_type, receive_buffer, sizeof(message_type));

        if (message_type == _IO_CONNECT) {
            MsgReply(rcvid, EOK, NULL, 0);
            continue;
        }

        // Heartbeat handling
        if (message_type == MSG_HEARTBEAT) {
            ecu_message_t message;
            memcpy(&message, receive_buffer, sizeof(message));
            ecu_state_t *state = get_ecu_state(message.ecu_id);

            if (state != NULL) {
                // If in recovery, only accept heartbeats from the replacement PID
                if (state->recovery_pending) {
                    if (message.sender_pid != state->failed_pid) {
                        state->pid = message.sender_pid;
                        state->last_heartbeat = ecu_now_ms();
                        state->heartbeat_received = 1;
                        state->recovery_pending = 0; // RECOVERY CONFIRMED

                        // Restore Green LED
                        if (state->ecu_id == ECU_ENGINE)        gpio_set_engine_normal();
                        else if (state->ecu_id == ECU_BRAKE)   gpio_set_brake_normal();
                        else if (state->ecu_id == ECU_STEERING) gpio_set_steering_normal();

                        pthread_mutex_lock(&output_mutex);
                        printf("[RECOVERY] %s RECOVERED SUCCESSFULLY\n\n", state->name);
                        fflush(stdout);
                        pthread_mutex_unlock(&output_mutex);

                        print_health_status();
                    }
                } else {
                    // Nominal operational heartbeat
                    state->pid = message.sender_pid;
                    state->last_heartbeat = ecu_now_ms();
                    state->heartbeat_received = 1;

                    if (state->ecu_id == ECU_ENGINE)        gpio_toggle_engine_heartbeat();
                    else if (state->ecu_id == ECU_BRAKE)   gpio_toggle_brake_heartbeat();
                    else if (state->ecu_id == ECU_STEERING) gpio_toggle_steering_heartbeat();
                }
            }

            MsgReply(rcvid, EOK, NULL, 0);
            continue;
        }

        // Steering Task Overrun
        if (message_type == MSG_TASK_COMPLETE) {
            ecu_message_t message;
            memcpy(&message, receive_buffer, sizeof(message));

            MsgReply(rcvid, EOK, NULL, 0);

            if (message.ecu_id == ECU_STEERING) {
                if (message.value_ms > STEERING_TASK_BUDGET_MS && !steering_state.recovery_pending) {
                    pthread_mutex_lock(&output_mutex);
                    printf("\n============================================================\n");
                    printf("STEERING ECU EXECUTION OVERRUN DETECTED\n");
                    printf("============================================================\n");
                    printf("Execution time : %u ms\n", message.value_ms);
                    printf("Allowed time   : %d ms\n\n", STEERING_TASK_BUDGET_MS);
                    printf("WATCHDOG : STEERING ECU FAILURE DETECTED\n");
                    printf("WATCHDOG : FAULT = TASK OVERRUN\n");
                    fflush(stdout);
                    pthread_mutex_unlock(&output_mutex);

                    steering_state.pid = message.sender_pid;
                    trigger_recovery(&steering_state, FAULT_OVERRUN);
                }
            }
            continue;
        }

        MsgError(rcvid, ENOSYS);
    }
    return NULL;
}

static void *monitor_thread(void *argument) {
    uint64_t current_time;
    uint64_t elapsed_time;
    (void)argument;

    while (1) {
        current_time = ecu_now_ms();

        // 1. Engine ECU Check
        if (engine_state.heartbeat_received && !engine_state.recovery_pending) {
            elapsed_time = current_time - engine_state.last_heartbeat;
            if (elapsed_time > TIMEOUT_THRESHOLD_MS) {
                pthread_mutex_lock(&output_mutex);
                printf("\n============================================================\n");
                printf("ENGINE ECU HEARTBEAT MISSED\n");
                printf("============================================================\n");
                printf("No heartbeat for %llu ms\n\n", (unsigned long long)elapsed_time);
                printf("WATCHDOG : ENGINE ECU FAILURE DETECTED\n");
                printf("WATCHDOG : FAULT = HEARTBEAT FAILURE\n");
                fflush(stdout);
                pthread_mutex_unlock(&output_mutex);

                trigger_recovery(&engine_state, FAULT_HEARTBEAT);
            }
        }

        // 2. Brake ECU Check
        if (brake_state.heartbeat_received && !brake_state.recovery_pending) {
            elapsed_time = current_time - brake_state.last_heartbeat;
            if (elapsed_time > TIMEOUT_THRESHOLD_MS) {
                pthread_mutex_lock(&output_mutex);
                printf("\n============================================================\n");
                printf("BRAKE ECU HEARTBEAT MISSED\n");
                printf("============================================================\n");
                printf("No heartbeat for %llu ms\n\n", (unsigned long long)elapsed_time);
                printf("WATCHDOG : BRAKE ECU FAILURE DETECTED\n");
                printf("WATCHDOG : FAULT = DEADLOCK\n");
                fflush(stdout);
                pthread_mutex_unlock(&output_mutex);

                trigger_recovery(&brake_state, FAULT_DEADLOCK);
            }
        }

        // 3. Steering ECU Timeout Fallback
        if (steering_state.heartbeat_received && !steering_state.recovery_pending) {
            elapsed_time = current_time - steering_state.last_heartbeat;
            if (elapsed_time > TIMEOUT_THRESHOLD_MS) {
                pthread_mutex_lock(&output_mutex);
                printf("\n============================================================\n");
                printf("STEERING ECU HEARTBEAT MISSED\n");
                printf("============================================================\n");
                printf("No heartbeat for %llu ms\n\n", (unsigned long long)elapsed_time);
                printf("WATCHDOG : STEERING ECU FAILURE DETECTED\n");
                printf("WATCHDOG : FAULT = TIMEOUT\n");
                fflush(stdout);
                pthread_mutex_unlock(&output_mutex);

                trigger_recovery(&steering_state, FAULT_OVERRUN);
            }
        }

        ecu_sleep_ms(WATCHDOG_CHECK_PERIOD_MS);
    }
    return NULL;
}

static void shutdown_handler(int sig) {
    (void)sig;
    int pins[] = {
        ENGINE_GREEN_GPIO, ENGINE_YELLOW_GPIO, ENGINE_RED_GPIO,
        BRAKE_GREEN_GPIO, BRAKE_YELLOW_GPIO, BRAKE_RED_GPIO,
        STEERING_GREEN_GPIO, STEERING_YELLOW_GPIO, STEERING_RED_GPIO
    };

    char cmd[64];
    for (int i = 0; i < 9; i++) {
        snprintf(cmd, sizeof(cmd), "gpio-bcm2711 set %d dl", pins[i]);
        system(cmd);
    }

    if (watchdog_channel) name_detach(watchdog_channel, 0);
    if (recovery_connection != -1) name_close(recovery_connection);

    exit(0);
}

int main(int argc, char *argv[]) {
    pthread_t msg_thread_id;
    pthread_t mon_thread_id;
    uint64_t last_display_time;
    (void)argc; (void)argv;

    setvbuf(stdout, NULL, _IONBF, 0);

    signal(SIGINT, shutdown_handler);
    signal(SIGTERM, shutdown_handler);

    memset(&engine_state, 0, sizeof(engine_state));
    memset(&brake_state, 0, sizeof(brake_state));
    memset(&steering_state, 0, sizeof(steering_state));

    engine_state.ecu_id = ECU_ENGINE;     engine_state.name = "ENGINE ECU";   engine_state.binary_name = "engine_ecu";
    brake_state.ecu_id = ECU_BRAKE;       brake_state.name = "BRAKE ECU";     brake_state.binary_name = "brake_ecu";
    steering_state.ecu_id = ECU_STEERING; steering_state.name = "STEERING ECU"; steering_state.binary_name = "steering_ecu";

    if (gpio_init() != 0) {
        printf("[WATCHDOG] GPIO initialization failed\n");
        fflush(stdout);
    }

    watchdog_channel = name_attach(NULL, WATCHDOG_NAME, 0);
    if (watchdog_channel == NULL) {
        printf("[WATCHDOG] name_attach failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    printf("[WATCHDOG] STARTED | PID=%d\n\n", getpid());
    fflush(stdout);

    pthread_create(&msg_thread_id, NULL, message_thread, NULL);
    pthread_create(&mon_thread_id, NULL, monitor_thread, NULL);

    last_display_time = ecu_now_ms();

    while (1) {
        uint64_t current_time = ecu_now_ms();
        if ((current_time - last_display_time) >= HEALTH_DISPLAY_PERIOD_MS) {
            print_health_status();
            last_display_time = current_time;
        }
        ecu_sleep_ms(100);
    }

    return EXIT_SUCCESS;
}
