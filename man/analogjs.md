# analogjs

This is the manual for the `analogjs` driver included with Vanilla Utilities.

DOS Doom has built-in support for the PC joystick interface, but the input is
"digital": even though the joystick is an analog device, turning is equivalent
to pressing the "turn left" and "turn right" buttons on the keyboard. It is
therefore less precise than using a mouse.

The `analogjs` driver makes use of Doom's
[external control API](https://doomwiki.org/wiki/External_control_driver) to
provide joystick input that is more precise. It also adds some additional
features over Doom's built-in joystick support, including the ability to
adjust turning speed, disable vertical movement and bind a joystick button to
move forward.

Some things to be aware of:

 * If you have a Gravis Gamepad, this driver cannot add any additional turning
   precision, because it is a digital device. The other features may be
   helpful, though.
 * A joystick is not a very good interface for playing Doom.
 * You can only use this driver with Doom, not other Doom engine games.

## Command line arguments

| Argument   | Parameter  | Description                          |
| ---------- | ---------- | ------------------------------------ |
| -jscfg     | `filename` | Path to config file to use (default is `analogjs.cfg`). |
| -calibrate |            | Calibrate joystick (see next section). |

## Calibration

Before you can use the driver, you must generate a configuration file, using
`-calibrate`. Follow the instructions as directed:
```
C:\DOOM2>analogjs -calibrate
analogjs: CENTER the joystick and press button 1:
analogjs: Push the joystick to the UPPER LEFT and press button 1:
analogjs: Push the joystick to the LOWER RIGHT and press button 1:
analogjs: Wrote config file analogjs.cfg

C:\DOOM2>
```
Once you've generated the configuration file, you can edit it to tweak some
of the defaults. Unfortunately there is not currently a friendlier option for
editing the file (like Doom's SETUP program).

## Examples

The `analogjs` driver has very few options. After generating a configuration
file, you can launch the game as follows:
```
C:\DOOM2>analogjs.exe doom2.exe
```
If you get this error message, it is because you have not gone through the
calibration sequence (see the previous section):
```
C:\DOOM2>analogjs.exe doom2.exe
analogjs: Failed to read config file analogjs.cfg
```

## Configuration file parameters

| Parameter name        | Description                     |
| --------------        | -----------                     |
| `always_run`          | If 1, equivalent to always holding down run key. Default 0. |
| `turn_speed`          | Multiplier to adjust turning speed. Default 512. |
| `novert`              | If 1, forward/backward movement is disabled. Default 0. |
| `cal_x_min`           | Joystick X value when pushed to extreme left. |
| `cal_x_max`           | Joystick X value when pushed to extreme right. |
| `cal_x_center`        | Joystick X value when centered. |
| `cal_x_deadzone_pct`  | Percent of X value range that is considered zero. |
| `cal_y_min`           | Joystick Y value when pushed to extreme top. |
| `cal_y_max`           | Joystick Y value when pushed to extreme bottom. |
| `cal_y_center`        | Joystick Y value when centered. |
| `cal_y_deadzone_pct`  | Percent of Y value range that is considered zero. |
| `joyb_fire`           | Joystick button that fires the player weapon. Default 0 (button 1). |
| `joyb_strafe`         | Joystick button that activates strafe mode. Default 1 (button 2). |
| `joyb_use`            | Joystick button that uses items / activates doors. Default 3 (button 4). |
| `joyb_speed`          | Joystick button that activates run mode. Default 2 (button 3). |
| `joyb_forward`        | Joystick button to make player walk forward. Default -1 (disabled). |
