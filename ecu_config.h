#ifndef ECU_CONFIG_H
#define ECU_CONFIG_H


/* ---------------------------------------------------------
 * Heartbeat configuration
 * --------------------------------------------------------- */

#define HEARTBEAT_PERIOD_MS       100
#define HEARTBEAT_TIMEOUT_MS      500

#define WATCHDOG_CHECK_PERIOD_MS  100


/* ---------------------------------------------------------
 * Steering timing supervision
 * --------------------------------------------------------- */

#define STEERING_TASK_BUDGET_MS   50
#define STEERING_FAULT_TIME_MS    120


/* ---------------------------------------------------------
 * Recovery configuration
 * --------------------------------------------------------- */

#define RECOVERY_STOP_WAIT_MS     200
#define RECOVERY_VERIFY_WAIT_MS   100
#define RECOVERY_VERIFY_ATTEMPTS  30


/* ---------------------------------------------------------
 * Console configuration
 * --------------------------------------------------------- */

#define HEALTH_DISPLAY_PERIOD_MS  1000

#endif
