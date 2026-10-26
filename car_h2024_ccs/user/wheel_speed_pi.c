#include "wheel_speed_pi.h"

static int32_t wheel_speed_pi_clamp(int32_t value,
                                    int32_t minimum,
                                    int32_t maximum)
{
    if(value < minimum)
    {
        return minimum;
    }
    if(value > maximum)
    {
        return maximum;
    }
    return value;
}

void wheel_speed_pi_init(wheel_speed_pi_t *controller,
                         const wheel_speed_pi_config_t *config)
{
    controller->config = *config;
    wheel_speed_pi_reset(controller);
}

void wheel_speed_pi_reset(wheel_speed_pi_t *controller)
{
    controller->integrator = 0;
    controller->output_pwm = controller->config.feedforward_pwm;
}

int32_t wheel_speed_pi_step(wheel_speed_pi_t *controller,
                            int32_t target_counts,
                            int32_t measured_counts)
{
    int32_t error;
    int32_t candidate_integrator;
    int32_t raw_output;

    if(target_counts <= 0)
    {
        controller->integrator = 0;
        controller->output_pwm = 0;
        return 0;
    }
    if(measured_counts < 0)
    {
        measured_counts = 0;
    }

    error = target_counts - measured_counts;
    candidate_integrator = wheel_speed_pi_clamp(
        controller->integrator + error,
        -controller->config.integrator_limit,
        controller->config.integrator_limit);
    raw_output = controller->config.feedforward_pwm +
        controller->config.kp * error +
        controller->config.ki * candidate_integrator;

    /* Conditional integration prevents wind-up while an output is saturated. */
    if((raw_output > controller->config.maximum_pwm && error > 0) ||
       (raw_output < controller->config.minimum_pwm && error < 0))
    {
        raw_output = controller->config.feedforward_pwm +
            controller->config.kp * error +
            controller->config.ki * controller->integrator;
    }
    else
    {
        controller->integrator = candidate_integrator;
    }

    controller->output_pwm = wheel_speed_pi_clamp(
        raw_output,
        controller->config.minimum_pwm,
        controller->config.maximum_pwm);
    return controller->output_pwm;
}
