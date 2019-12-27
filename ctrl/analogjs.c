
#include <stdio.h>
#include <string.h>

#include <dos.h>
#include "lib/inttypes.h"

#include "ctrl/control.h"
#include "lib/flag.h"
#include "lib/log.h"

#define JOYSTICK_PORT  0x201

struct axis
{
    int min, max, center;
    int deadzone_pct;
};

const int forwardmove[2] = {0x19, 0x32};
const int sidemove[2] = {0x18, 0x28};

// From joystick.asm:
extern int joystickx, joysticky;
extern void ReadJoystick(void);

static char *config_file = "analogjs.cfg";
static int calibrate = 0;

static struct axis x_axis, y_axis;

static int always_run = 0;
static int turn_speed = 512;
static int novert = 0;

static int joyb_fire = 0;
static int joyb_strafe = 1;
static int joyb_use = 3;
static int joyb_speed = 2;
static int joyb_forward = -1;

static struct
{
    const char *name;
    int *var;
} config_vars[] = {
    {"always_run",          &always_run},
    {"turn_speed",          &turn_speed},
    {"novert",              &novert},
    {"cal_x_min",           &x_axis.min},
    {"cal_x_max",           &x_axis.max},
    {"cal_x_center",        &x_axis.center},
    {"cal_x_deadzone_pct",  &x_axis.deadzone_pct},
    {"cal_y_min",           &y_axis.min},
    {"cal_y_max",           &y_axis.max},
    {"cal_y_center",        &y_axis.center},
    {"cal_y_deadzone_pct",  &y_axis.deadzone_pct},
    {"joyb_fire",           &joyb_fire},
    {"joyb_strafe",         &joyb_strafe},
    {"joyb_use",            &joyb_use},
    {"joyb_speed",          &joyb_speed},
    {"joyb_forward",        &joyb_forward},
};

static int ParseConfigFile(char *filename)
{
    char config_name[20];
    int value;
    int i;
    FILE *fs;

    fs = fopen(filename, "r");
    if (fs == NULL)
    {
        return 0;
    }

    while (!feof(fs))
    {
        if (fscanf(fs, "%20s %d\n", config_name, &value) != 2)
        {
            continue;
        }
        for (i = 0; i < sizeof(config_vars) / sizeof(*config_vars); ++i)
        {
            if (!strcmp(config_vars[i].name, config_name))
            {
                *config_vars[i].var = value;
                break;
            }
        }
    }

    fclose(fs);
    return 1;
}

static void WriteConfigFile(char *filename)
{
    FILE *fs;
    int i;

    fs = fopen(filename, "w");
    if (fs == NULL)
    {
        Error("Failed to open '%s' for writing.", filename);
    }

    for (i = 0; i < sizeof(config_vars) / sizeof(*config_vars); ++i)
    {
        fprintf(fs, "%-20s%d\n", config_vars[i].name, *config_vars[i].var);
    }

    fclose(fs);
}

static int ReadButtons(void)
{
    return (inportb(JOYSTICK_PORT) >> 4) & 0xf;
}

static void WaitButtonPress(void)
{
    while ((ReadButtons() & 0x01) != 0)
    {
        CheckAbort("Joystick calibration");
    }
    while ((ReadButtons() & 0x01) == 0)
    {
        CheckAbort("Joystick calibration");
    }
}

static void CalibrateJoystick(void)
{
    LogMessage("CENTER the joystick and press button 1:");
    WaitButtonPress();
    ReadJoystick();
    x_axis.center = joystickx; y_axis.center = joysticky;

    LogMessage("Push the joystick to the UPPER LEFT and press button 1:");
    WaitButtonPress();
    ReadJoystick();
    x_axis.min = joystickx; y_axis.min = joysticky;

    LogMessage("Push the joystick to the LOWER RIGHT and press button 1:");
    WaitButtonPress();
    ReadJoystick();
    x_axis.max = joystickx; y_axis.max = joysticky;
}

static int16_t AdjustAxisValue(struct axis *a, int v, int speed)
{
    long scratch;
    int range;

    if (v < a->min)
    {
        v = a->min;
    }
    else if (v > a->max)
    {
        v = a->max;
    }

    range = (a->max - a->min) * a->deadzone_pct / 200;

    if (v > a->center - range && v < a->center + range)
    {
        // In dead zone
        return 0;
    }
    else if (v < a->center)
    {
        v = a->center - v;
        range = a->center - a->min;
        scratch = ((long) speed * v) / range;
        return (int16_t) scratch;
    }
    else
    {
        v = v - a->center;
        range = a->max - a->center;
        scratch = ((long) -speed * v) / range;
        return (int16_t) scratch;
    }
}

#define IS_PRESSED(buttons, b) \
    (b < 0 ? 0 : \
     b > 20 ? 1 : (buttons & (1 << b)) == 0)

void ControlCallback(ticcmd_t *ticcmd)
{
    int buttons;
    int run;

    ReadJoystick();
    buttons = ReadButtons();

    run = always_run || IS_PRESSED(buttons, joyb_speed);

    if (IS_PRESSED(buttons, joyb_strafe))
    {
        ticcmd->sidemove += AdjustAxisValue(
            &x_axis, joystickx, -sidemove[run]);
    }
    else
    {
        ticcmd->angleturn += AdjustAxisValue(&x_axis, joystickx, turn_speed);
    }

    if (IS_PRESSED(buttons, joyb_forward))
    {
        ticcmd->forwardmove += forwardmove[run];
    }
    else if (!novert)
    {
        ticcmd->forwardmove += AdjustAxisValue(
            &y_axis, joysticky, forwardmove[run]);
    }

    if (IS_PRESSED(buttons, joyb_fire))
    {
        ticcmd->buttons |= BT_ATTACK;
    }
    if (IS_PRESSED(buttons, joyb_use))
    {
        ticcmd->buttons |= BT_USE;
    }
}

int main(int argc, char *argv[])
{
    char **args;

    StringFlag("-jscfg", &config_file, "filename",
               "Path to config file to use");
    BoolFlag("-calibrate", &calibrate, "Calibrate joystick");
    ControlRegisterFlags();

    args = ParseCommandLine(argc, argv);

    if (calibrate)
    {
        ParseConfigFile(config_file);
        CalibrateJoystick();
        WriteConfigFile(config_file);
        LogMessage("Wrote config file %s", config_file);
        return 0;
    }

    if (args == NULL)
    {
        ErrorPrintUsage("No command given to run.");
    }

    if (!ParseConfigFile(config_file))
    {
        Error("Failed to read config file %s", config_file);
    }

    // We take over joystick control.
    args = AppendArgs(args, "-nojoy", NULL);

    ControlLaunchDoom(args, ControlCallback);

    return 0;
}

