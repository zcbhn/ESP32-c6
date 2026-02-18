/**
 * @file pid.h
 * @brief Discrete PID 온도 컨트롤러
 */
#ifndef RBMS_PID_H
#define RBMS_PID_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float kp, ki, kd;
    float setpoint;
    float integral;
    float prev_measurement;  /* derivative-on-measurement 용 */
    float out_min, out_max;
} pid_ctrl_t;

esp_err_t pid_init(pid_ctrl_t *pid, float kp, float ki, float kd);
esp_err_t pid_set_limits(pid_ctrl_t *pid, float min_out, float max_out);
esp_err_t pid_set_setpoint(pid_ctrl_t *pid, float setpoint);
float     pid_compute(pid_ctrl_t *pid, float measurement, float dt);
void      pid_reset(pid_ctrl_t *pid);

#ifdef __cplusplus
}
#endif

#endif /* RBMS_PID_H */
