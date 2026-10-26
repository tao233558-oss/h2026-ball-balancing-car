#include "headfile.h"
#include "menu.h"

typedef enum
{
    MENU_ROOT = 0,
    MENU_PARAM_SELECT,
    MENU_PARAM_EDIT,
    MENU_TASK_SELECT,
    MENU_RUNNING,
} menu_state_t;

typedef struct
{
    const char *name;
    int *value;
    int step;
    int min_value;
    int max_value;
} menu_param_t;

static menu_state_t menu_state;
static uint8_t menu_root_item;
static uint8_t menu_task = 1;
static uint8_t menu_param_index;
static uint8_t menu_dirty = 1;

static menu_param_t menu_params[] =
{
    {"E LAPS",   &g_e2025_laps_setting,          1, 1,       5},
    {"M1 BASE",  &g_track_square_base_pwm,   100, 0,    5000},
    {"M1 SRCH",  &g_track_square_search_pwm, 100, 0,    5000},
    {"M1 KP",    &g_track_square_kp,           5, 0,     300},
    {"M1 KD",    &g_track_square_kd,           5, 0,     300},
    {"M1 TURN",  &g_track_square_turn_pwm,   100, 0,    5000},
    {"H BASE",   &g_h_req1_straight_base,    100, 700,  1800},
    {"H TICKS",  &g_h_req1_ab_ticks,          50, 0,    3000},
};

static const char *menu_task_names[] =
{
    "",
    "E1 TRACK N",
    "E2 AIM 2S",
    "E3 AIM 4S",
    "E4 MOVE 1",
    "E5 MOVE 2",
    "E6 CIRCLE",
};

static uint8_t menu_task_count(void)
{
    return (uint8_t)((sizeof(menu_task_names) / sizeof(menu_task_names[0])) - 1U);
}

static uint8_t menu_param_count(void)
{
    return (uint8_t)(sizeof(menu_params) / sizeof(menu_params[0]));
}

static int menu_clamp_int(int value, int min_value, int max_value)
{
    if(value < min_value)
    {
        return min_value;
    }
    if(value > max_value)
    {
        return max_value;
    }
    return value;
}

static void menu_clear_lines(void)
{
    OLED_ShowString(1, 1, "                ");
    OLED_ShowString(2, 1, "                ");
    OLED_ShowString(3, 1, "                ");
    OLED_ShowString(4, 1, "                ");
}

void menu_init(void)
{
    menu_state = MENU_ROOT;
    menu_root_item = 0;
    menu_task = 1;
    menu_param_index = 0;
    menu_dirty = 1;
    mode = 1;
    set = 0;
}

void menu_handle_key(uint8_t key)
{
    menu_param_t *param;

    if(key == KEY_NONE)
    {
        return;
    }

    if(menu_state == MENU_RUNNING)
    {
        if(key == KEY_SW2)
        {
            e2025_request_abort();
            set = 0;
            Set_left_pwm(0);
            Set_right_pwm(0);
            menu_state = MENU_ROOT;
            menu_dirty = 1;
        }
        return;
    }

    if(menu_state == MENU_ROOT)
    {
        if(key == KEY_SW1)
        {
            menu_root_item ^= 1;
            menu_dirty = 1;
        }
        else if(key == KEY_SW2)
        {
            menu_state = (menu_root_item == 0) ? MENU_PARAM_SELECT : MENU_TASK_SELECT;
            menu_dirty = 1;
        }
    }
    else if(menu_state == MENU_PARAM_SELECT)
    {
        if(key == KEY_SW1)
        {
            menu_param_index++;
            if(menu_param_index >= menu_param_count())
            {
                menu_param_index = 0;
            }
            menu_dirty = 1;
        }
        else if(key == KEY_SW2)
        {
            menu_state = MENU_PARAM_EDIT;
            menu_dirty = 1;
        }
    }
    else if(menu_state == MENU_PARAM_EDIT)
    {
        param = &menu_params[menu_param_index];
        if(key == KEY_SW3)
        {
            *param->value = menu_clamp_int(*param->value + param->step, param->min_value, param->max_value);
            menu_dirty = 1;
        }
        else if(key == KEY_SW4)
        {
            *param->value = menu_clamp_int(*param->value - param->step, param->min_value, param->max_value);
            menu_dirty = 1;
        }
        else if(key == KEY_SW2)
        {
            menu_state = MENU_ROOT;
            menu_dirty = 1;
        }
    }
    else if(menu_state == MENU_TASK_SELECT)
    {
        if(key == KEY_SW1)
        {
            menu_task++;
            if(menu_task > menu_task_count())
            {
                menu_task = 1;
            }
            menu_dirty = 1;
        }
        else if(key == KEY_SW2)
        {
            mode = menu_task;
            set = 1;
            e2025_request_start((e2025_task_t) menu_task,
                                (uint8_t) g_e2025_laps_setting);
            menu_state = MENU_RUNNING;
            menu_dirty = 1;
            OLED_Clear();
        }
    }
}

void menu_update(void)
{
    e2025_status_t status;

    if(menu_state != MENU_RUNNING)
    {
        return;
    }

    e2025_get_status(&status);
    if(e2025_state_is_terminal(status.state))
    {
        set = 0;
        Set_left_pwm(0);
        Set_right_pwm(0);
    }
}

void menu_show(void)
{
    menu_param_t *param = &menu_params[menu_param_index];

    if(!menu_dirty)
    {
        return;
    }

    menu_clear_lines();

    if(menu_state == MENU_ROOT)
    {
        OLED_ShowString(1, 1, "Select Mode");
        OLED_ShowString(2, 1, (menu_root_item == 0) ? "Tune Params" : "Select Task");
        OLED_ShowString(3, 1, "SW1 Next");
        OLED_ShowString(4, 1, "SW2 OK");
    }
    else if(menu_state == MENU_PARAM_SELECT)
    {
        OLED_ShowString(1, 1, "Select Param");
        OLED_ShowString(2, 1, (char *)param->name);
        OLED_ShowString(3, 1, "Val:");
        OLED_ShowSignedNum(3, 5, *param->value, 5);
        OLED_ShowString(4, 1, "SW1 Next SW2 OK");
    }
    else if(menu_state == MENU_PARAM_EDIT)
    {
        OLED_ShowString(1, 1, "Edit Param");
        OLED_ShowString(2, 1, (char *)param->name);
        OLED_ShowString(3, 1, "Val:");
        OLED_ShowSignedNum(3, 5, *param->value, 5);
        OLED_ShowString(4, 1, "SW3+ SW4- SW2OK");
    }
    else if(menu_state == MENU_TASK_SELECT)
    {
        OLED_ShowString(1, 1, "Select Task");
        OLED_ShowString(2, 1, (char *)menu_task_names[menu_task]);
        OLED_ShowString(3, 1, "SW1 Next");
        OLED_ShowString(4, 1, "SW2 Start");
    }

    menu_dirty = 0;
}

uint8_t menu_is_running(void)
{
    return (menu_state == MENU_RUNNING) ? 1 : 0;
}
