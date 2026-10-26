#ifndef __xunji_h_
#define __xunji_h_

#include "headfile.h"

#define D1 digtal(1)
#define D2 digtal(2)
#define D3 digtal(3)
#define D4 digtal(4)
#define D5 digtal(5)
#define D6 digtal(6)
#define D7 digtal(7)
#define D8 digtal(8)

extern  int last_statue;
extern   int  now_statue;
extern   int  change_flag1;
extern int g_track_square_base_pwm;
extern int g_track_square_search_pwm;
extern int g_track_square_kp;
extern int g_track_square_kd;
extern int g_track_square_pwm_limit;
extern int g_track_square_turn_pwm;
extern int g_h_req1_straight_base;
extern int g_h_req1_left_trim;
extern int g_h_req1_right_trim;
extern int g_h_req1_target_counts;

typedef struct
{
    uint8_t armed;
    uint8_t active_ticks;
    uint8_t clear_ticks;
} track_corner_filter_t;

void xunji_init(void);
uint8_t track_read_pattern(void);
int track_error_from_pattern(uint8_t pattern);
int track_get_error(void);
void track_corner_filter_init(track_corner_filter_t *filter);
uint8_t track_corner_filter_step(track_corner_filter_t *filter, uint8_t pattern);
void track_square_run(void);
void h_task_reset(void);
void h_req1_run(void);
uint8_t h_task_is_done(void);
uint16_t h_task_elapsed_ticks(void);
void h_task_update_distance(int32_t left_total, int32_t right_total);
void track2(void);
void track4(void);
void track3(void);
unsigned char digtal(unsigned char channel);
void pid_track(void);
void track1(void);

#endif
