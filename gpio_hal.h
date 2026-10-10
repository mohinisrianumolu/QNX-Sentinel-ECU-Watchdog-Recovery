#ifndef GPIO_HAL_H
#define GPIO_HAL_H

#define ENGINE_GREEN_GPIO       17
#define ENGINE_YELLOW_GPIO      27
#define ENGINE_RED_GPIO         22

#define BRAKE_GREEN_GPIO        23
#define BRAKE_YELLOW_GPIO       24
#define BRAKE_RED_GPIO          25

#define STEERING_GREEN_GPIO     5
#define STEERING_YELLOW_GPIO    6
#define STEERING_RED_GPIO       12

int  gpio_init(void);
int  gpio_set_output(int gpio, int value);

// Heartbeat Blinking (Green Toggles)
void gpio_toggle_engine_heartbeat(void);
void gpio_toggle_brake_heartbeat(void);
void gpio_toggle_steering_heartbeat(void);

// Engine States
void gpio_set_engine_normal(void);
void gpio_set_engine_recovery(void);
void gpio_set_engine_fault(void);

// Brake States
void gpio_set_brake_normal(void);
void gpio_set_brake_recovery(void);
void gpio_set_brake_fault(void);

// Steering States
void gpio_set_steering_normal(void);
void gpio_set_steering_recovery(void);
void gpio_set_steering_fault(void);

#endif
