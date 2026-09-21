<p align="center">
  <img src="cclock.svg" width="144" alt="cclock logo">
</p>

# cclock

A small countdown timer for Wayland. It attaches to any screen edge, moves
between outputs, and stays out of the way until hovered.

| Running | Paused |
| --- | --- |
| ![Running timer](screenshots/running.png) | ![Paused timer](screenshots/paused.png) |

![Time picker](screenshots/picker.png)

## Features

- Drag the timer between screen edges and outputs.
- Survive output disconnection, use a fallback, and return when it reconnects.
- Click to pause or resume; hover to reveal text and the close button.
- Configure running, paused, and overtime colors.
- Optionally protect one or every OLED output from static bright states.
- Control or query the timer from the command line.
- Run commands before start, at zero, and on stop.

## Install

```sh
nix profile install github:tenyoru/cclock
```

For a NixOS flake:

```nix
{
  inputs.cclock.url = "github:tenyoru/cclock";

  outputs = { nixpkgs, cclock, ... }: {
    nixosConfigurations.your-host = nixpkgs.lib.nixosSystem {
      modules = [{
        environment.systemPackages = [
          cclock.packages.x86_64-linux.default
        ];
      }];
    };
  };
}
```

## Usage

```sh
cclock -m 25 -t "Deep work"
cclock -m 5 --running-color steelblue --paused-color '#ffd60a'
cclock --screen DP-2 --oled DP-2 -m 25

cclock --pause
cclock --resume
cclock --toggle
cclock --time-get
cclock --text-get
cclock --stop
```

Running cclock without a duration opens the time picker. Different duration
options are added, so `cclock -m 1 -s 30` starts at 90 seconds.

At zero, the blob changes to the overtime color and continues counting. The
overlay omits the leading `+`; `cclock --time-get` retains it for scripts.
While overtime is paused, the paused color takes precedence.

On protected OLED outputs, digits move slightly at intervals. A paused blob
hides after ten idle seconds, while overtime switches from a red background to
red digits on black. Hovering restores the full state color.

Run `cclock --help` for every command-line option, `man cclock` for the full
reference, and see [configuration](docs/configuration.md) for all TOML keys.

## Development

```sh
nix develop
just build
just test
nix flake check
MANPAGER=cat man -l ./cclock.1
git diff --check
```

cclock requires Linux, Wayland, and a compositor with layer-shell support. It
is developed and tested on niri.
