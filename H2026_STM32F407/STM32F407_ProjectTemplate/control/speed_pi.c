#include "speed_pi.h"

#define SPEED_PI_KP               3
#define SPEED_PI_INTEGRAL_DIV     16
#define SPEED_PI_INTEGRAL_LIMIT   800
#define SPEED_PI_OUTPUT_MIN       120
#define SPEED_PI_OUTPUT_MAX       500

static int16_t clamp_i16(int32_t value, int16_t minimum, int16_t maximum)
{
    if(value < minimum)
    {
        return minimum;
    }
    if(value > maximum)
    {
        return maximum;
    }
    return (int16_t)value;
}

void speed_pi_reset(speed_pi_t *controller, int16_t target_counts)
{
    controller->target_counts = target_counts;
    controller->integral = 0;
    controller->output_permille = 0;
}

int16_t speed_pi_step(speed_pi_t *controller,
                      int16_t measured_counts,
                      int16_t feedforward_permille)
{
    int16_t error =
        (int16_t)(controller->target_counts - measured_counts);
    int32_t integral =
        (int32_t)controller->integral + (int32_t)error;
    int32_t output;

    controller->integral =
        clamp_i16(integral,
                  -SPEED_PI_INTEGRAL_LIMIT,
                  SPEED_PI_INTEGRAL_LIMIT);

    output = (int32_t)feedforward_permille +
             ((int32_t)SPEED_PI_KP * error) +
             ((int32_t)controller->integral / SPEED_PI_INTEGRAL_DIV);

    controller->output_permille =
        clamp_i16(output, SPEED_PI_OUTPUT_MIN, SPEED_PI_OUTPUT_MAX);
    return controller->output_permille;
}
