#include "gpio_hal.h"
#include <stdio.h>
#include <stdlib.h>

static int pin_states[32] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
static int green_toggle[3] = {0, 0, 0};

int gpio_set_output(int gpio, int value) {
    if (gpio < 0 || gpio >= 32) return -1;

    // Never re-run shell command if the pin is already in the requested state
    if (pin_states[gpio] == value) return 0;

    char command[64];
    // ONLY change output level (dh / dl). NEVER re-configure "op" during runtime!
    snprintf(command, sizeof(command), "gpio-bcm2711 set %d %s", gpio, value ? "dh" : "dl");
    system(command);
    pin_states[gpio] = value;
    return 0;
}

int gpio_init(void) {
    int pins[] = {
        ENGINE_GREEN_GPIO, ENGINE_YELLOW_GPIO, ENGINE_RED_GPIO,
        BRAKE_GREEN_GPIO, BRAKE_YELLOW_GPIO, BRAKE_RED_GPIO,
        STEERING_GREEN_GPIO, STEERING_YELLOW_GPIO, STEERING_RED_GPIO
    };

    char command[64];
    for (int i = 0; i < 9; i++) {
        // 1. Configure direction as output ONCE at boot
        snprintf(command, sizeof(command), "gpio-bcm2711 set %d op", pins[i]);
        system(command);
        // 2. Drive pin LOW initially
        snprintf(command, sizeof(command), "gpio-bcm2711 set %d dl", pins[i]);
        system(command);
        pin_states[pins[i]] = 0;
    }
    return 0;
}

/* --- Heartbeat Toggles (Only Green Pin Toggles) --- */

void gpio_toggle_engine_heartbeat(void) {
    green_toggle[0] = !green_toggle[0];
    gpio_set_output(ENGINE_GREEN_GPIO, green_toggle[0]);
}

void gpio_toggle_brake_heartbeat(void) {
    green_toggle[1] = !green_toggle[1];
    gpio_set_output(BRAKE_GREEN_GPIO, green_toggle[1]);
}

void gpio_toggle_steering_heartbeat(void) {
    green_toggle[2] = !green_toggle[2];
    gpio_set_output(STEERING_GREEN_GPIO, green_toggle[2]);
}

/* --- Engine ECU States --- */

void gpio_set_engine_normal(void) {
    gpio_set_output(ENGINE_YELLOW_GPIO, 0);
    gpio_set_output(ENGINE_RED_GPIO, 0);
    gpio_set_output(ENGINE_GREEN_GPIO, 1);
    green_toggle[0] = 1;
}

void gpio_set_engine_recovery(void) {
    gpio_set_output(ENGINE_GREEN_GPIO, 0);
    gpio_set_output(ENGINE_RED_GPIO, 0);
    gpio_set_output(ENGINE_YELLOW_GPIO, 1);
}

void gpio_set_engine_fault(void) {
    gpio_set_output(ENGINE_GREEN_GPIO, 0);
    gpio_set_output(ENGINE_YELLOW_GPIO, 0);
    gpio_set_output(ENGINE_RED_GPIO, 1);
}

/* --- Brake ECU States --- */

void gpio_set_brake_normal(void) {
    gpio_set_output(BRAKE_YELLOW_GPIO, 0);
    gpio_set_output(BRAKE_RED_GPIO, 0);
    gpio_set_output(BRAKE_GREEN_GPIO, 1);
    green_toggle[1] = 1;
}

void gpio_set_brake_recovery(void) {
    gpio_set_output(BRAKE_GREEN_GPIO, 0);
    gpio_set_output(BRAKE_RED_GPIO, 0);
    gpio_set_output(BRAKE_YELLOW_GPIO, 1);
}

void gpio_set_brake_fault(void) {
    gpio_set_output(BRAKE_GREEN_GPIO, 0);
    gpio_set_output(BRAKE_YELLOW_GPIO, 0);
    gpio_set_output(BRAKE_RED_GPIO, 1);
}

/* --- Steering ECU States --- */

void gpio_set_steering_normal(void) {
    gpio_set_output(STEERING_YELLOW_GPIO, 0);
    gpio_set_output(STEERING_RED_GPIO, 0);
    gpio_set_output(STEERING_GREEN_GPIO, 1);
    green_toggle[2] = 1;
}

void gpio_set_steering_recovery(void) {
    gpio_set_output(STEERING_GREEN_GPIO, 0);
    gpio_set_output(STEERING_RED_GPIO, 0);
    gpio_set_output(STEERING_YELLOW_GPIO, 1);
}

void gpio_set_steering_fault(void) {
    gpio_set_output(STEERING_GREEN_GPIO, 0);
    gpio_set_output(STEERING_YELLOW_GPIO, 0);
    gpio_set_output(STEERING_RED_GPIO, 1);
}
