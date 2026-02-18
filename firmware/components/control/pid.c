/**
 * @file pid.c
 * @brief Discrete PID 온도 컨트롤러
 */
#include "pid.h"
#include "esp_log.h"

static const char *TAG = "pid";

esp_err_t pid_init(pid_ctrl_t *pid, float kp, float ki, float kd)
{
    if (pid == NULL) return ESP_ERR_INVALID_ARG;

    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->setpoint = 0.0f;
    pid->integral = 0.0f;
    pid->prev_measurement = 0.0f;
    pid->out_min = 0.0f;
    pid->out_max = 100.0f;

    ESP_LOGI(TAG, "PID init: Kp=%.2f Ki=%.2f Kd=%.2f", kp, ki, kd);
    return ESP_OK;
}

esp_err_t pid_set_limits(pid_ctrl_t *pid, float min_out, float max_out)
{
    if (pid == NULL) return ESP_ERR_INVALID_ARG;
    pid->out_min = min_out;
    pid->out_max = max_out;
    return ESP_OK;
}

esp_err_t pid_set_setpoint(pid_ctrl_t *pid, float setpoint)
{
    if (pid == NULL) return ESP_ERR_INVALID_ARG;
    pid->setpoint = setpoint;
    return ESP_OK;
}

float pid_compute(pid_ctrl_t *pid, float measurement, float dt)
{
    if (pid == NULL || dt <= 0.0f) return 0.0f;

    float error = pid->setpoint - measurement;

    /* Proportional */
    float p_term = pid->kp * error;

    /* Integral (적분 누적은 아래 back-calculation 후) */
    pid->integral += error * dt;
    float i_term = pid->ki * pid->integral;

    /* Derivative-on-measurement (setpoint 변경 시 미분 킥 방지) */
    float d_term = -pid->kd * (measurement - pid->prev_measurement) / dt;
    pid->prev_measurement = measurement;

    /* 미클램핑 출력 계산 */
    float output_raw = p_term + i_term + d_term;

    /* Output clamping */
    float output = output_raw;
    if (output > pid->out_max) output = pid->out_max;
    if (output < pid->out_min) output = pid->out_min;

    /* Back-calculation anti-windup: 클램핑된 만큼 적분 보정 */
    if (pid->ki > 0.001f) {
        float saturation_error = output - output_raw;
        pid->integral += saturation_error / pid->ki;
    }

    return output;
}

void pid_reset(pid_ctrl_t *pid)
{
    if (pid == NULL) return;
    pid->integral = 0.0f;
    pid->prev_measurement = 0.0f;
}
