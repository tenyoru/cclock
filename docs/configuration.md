# Configuration

cclock creates `~/.config/cclock/config.toml` on first use. If
`XDG_CONFIG_HOME` is set, it uses `$XDG_CONFIG_HOME/cclock/config.toml`
instead. Command-line values override the corresponding color and OLED
settings.

```toml
[colors]
running = "#0b0b0d"
paused = "#ffd60a"
overtime = "#d32f2f"

[oled]
# "none", "all", or one output name
output = "DP-2"
interval = 60
timeout = 10

[keyboard]
motion = true
step = 20

[commands]
before_start = "playerctl pause"
on_zero = "notify-send 'cclock' 'Time is up'"
on_stop = "playerctl play"
```

## Appearance

The `[colors]` section controls the timer state colors. `colors.running`,
`colors.paused`, and `colors.overtime` accept opaque Qt color names or hex
values. Running and paused text contrast is selected automatically. Overtime
always uses dark text. Pausing during overtime shows the paused color until
the timer resumes.

## Outputs

The timer output is persisted with the dynamic state in
`~/.local/share/cclock/cclock.conf`. `--screen NAME` overrides that output for
one run. There is no timer output key in `config.toml`. cclock uses another
available output while the selected output is disconnected and returns when
it reconnects. Output geometry is read from Qt whenever surfaces are created;
no size needs to be configured.

The `[oled]` section controls OLED protection. `oled.output` accepts:

- `"none"` disables it.
- `"all"` enables it on every output.
- An output name enables it only there.

Protection applies only while the blob is on the selected output. The whole
blob continuously and linearly traverses one collapsed blob width or height
during each `oled.interval`. It cycles among its saved position and one step to
either side; clamping against its current size keeps it fully visible. The
saved position does not change. The interval must be at least 10 seconds. On a
protected output, the blob uses 90% opacity until hovered or dragged.

Paused blobs and their digits remain visible. After `oled.timeout` seconds
without interaction, running overtime changes from a red background with dark
digits to a black background with red digits. Hovering the blob restores its
opacity and full state color and restarts the timeout. The timeout must be at
least one second.

## Keyboard Motion

The `[keyboard]` section controls keyboard movement. `keyboard.step` is the
movement distance in pixels. `keyboard.motion` allows clicking the blob to
activate the movement keys without showing the focus outline.

Run `cclock --focus` to enter focus mode. Use `h` and `l` along the top or
bottom edge, and `k` and `j` along the left or right edge. Each press moves
`keyboard.step` pixels. `Shift+H/J/K/L` moves the blob to the
left/bottom/top/right edge. `Tab` toggles pause, `Ctrl+C` stops the timer, and
`Ctrl+F` toggles fullscreen on the timer's current output. `Escape` exits
fullscreen before focus mode. Fullscreen uses the blob's current state colors
and is not persisted.

Dropping the blob after a drag exits focus mode and removes its focus outline.

`cclock --focus` works regardless of `keyboard.motion`, activates the movement
keys, and shows the focus outline until `Escape` or a drag. Bind it through the
compositor for a global shortcut. The movement keys themselves are local.

## Commands

The `[commands]` section contains optional shell commands.
`commands.before_start` runs immediately before countdown starts,
`commands.on_zero` runs when it first crosses zero, and `commands.on_stop` runs
on a normal UI or CLI stop. Each runs once through `/bin/sh -c`; empty values
do nothing.

Dynamic state such as the current edge, position, output, and last picker
value is stored separately in `~/.local/share/cclock/cclock.conf`; do not put
those values in `config.toml`.
