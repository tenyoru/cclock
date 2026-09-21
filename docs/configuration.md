# Configuration

cclock creates `~/.config/cclock/config.toml` on first use. If
`XDG_CONFIG_HOME` is set, it uses `$XDG_CONFIG_HOME/cclock/config.toml`
instead. Command-line values override the corresponding color, screen, and
OLED settings.

```toml
running_color = "#0b0b0d"
paused_color = "#ffd60a"
overtime_color = "#d32f2f"
screen = "DP-2"

# "none", "all", or one output name
oled = "DP-2"
oled_interval = 60
oled_shift = 5
oled_timeout = 10

keyboard_motion = true
keyboard_step = 20

before_start = "playerctl pause"
on_zero = "notify-send 'cclock' 'Time is up'"
on_stop = "playerctl play"
```

## Appearance

`running_color`, `paused_color`, and `overtime_color` accept opaque Qt color
names or hex values. Running and paused text contrast is selected
automatically. Overtime always uses dark text. Pausing during overtime shows
the paused color until the timer resumes.

## Outputs

`screen` names the preferred output, such as `DP-2`. cclock uses another
available output while it is disconnected and returns when it reconnects.
Output geometry is read from Qt whenever surfaces are created; no size needs
to be configured.

`oled` controls pixel shifting:

- `"none"` disables it.
- `"all"` enables it on every output.
- An output name enables it only there.

Every `oled_interval` seconds, the digits cycle through five positions up to
`oled_shift` pixels from their normal position. The blob and its saved position
do not move. The interval must be at least 10 seconds, and the shift is limited
to 0-20 pixels.

After `oled_timeout` seconds without interaction, a paused blob fades out
completely. Overtime changes from a red background with dark digits to a black
background with red digits. Hovering the blob restores its full state color
and restarts the timeout. The timeout must be at least one second.

## Keyboard Motion

With `keyboard_motion = true`, click the timer to give it keyboard focus.
Use `h` and `l` along the top or bottom edge, and `k` and `j` along the left
or right edge. Each press moves `keyboard_step` pixels. These are local keys,
not global shortcuts.

## Commands

`before_start`, `on_zero`, and `on_stop` are optional shell commands. Each
runs once through `/bin/sh -c`: immediately before countdown starts, when it
first crosses zero, and on a normal UI or CLI stop. Empty values do nothing.

Dynamic state such as the current edge, position, output, and last picker
value is stored separately in `~/.config/cclock/cclock.conf`; do not put those
values in `config.toml`.
