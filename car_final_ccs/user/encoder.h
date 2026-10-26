#ifndef __ENCODER_H__
#define __ENCODER_H__

#include <stdint.h>

/* Change only these two signs if a hand-forward wheel test reports negative. */
#define ENCODER_LEFT_APP_SIGN  (-1)
#define ENCODER_RIGHT_APP_SIGN (-1)

extern int left_encoder;
extern int right_encoder;

void encoder_init(void);
int read_encoder1(void);
int read_encoder2(void);
void encoder_read_and_reset(int32_t *encoder1, int32_t *encoder2);
int32_t encoder_left_apply_sign(int32_t raw_count);
int32_t encoder_right_apply_sign(int32_t raw_count);


#endif

