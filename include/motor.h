#ifndef MOTOR_H
#define MOTOR_H

#ifdef __cplusplus
extern "C" {
#endif

void GPIO_INIT();

void SERVO_INIT();

void motor_control(int id, int fx);

void set_servo_angle(int channel, int angle);

#ifdef __cplusplus
}
#endif

#endif