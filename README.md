<p align="center">
  <img src="cclock.svg" width="144" alt="cclock logo">
</p>

# cclock

A small, draggable countdown timer for Wayland. It attaches to any screen edge,
moves between outputs, and stays out of the way until hovered.

| Running | Paused |
| --- | --- |
| ![Running timer](screenshots/running.png) | ![Paused timer](screenshots/paused.png) |

![Time picker](screenshots/picker.png)

## Features

- Drag the timer between screen edges and outputs.
- Click to pause or resume; hover to reveal text and the close button.
- Choose separate running and paused colors with automatic text contrast.
- Control or query the running timer from the command line.
- Pick a duration interactively when no time is supplied.
- Continue into visible overtime after the countdown reaches zero.

## Install

Install the latest release from Codeberg with Nix:

```sh
nix profile install git+https://codeberg.org/tenyoru/cclock
```

For a NixOS flake, add the input and package:

```nix
{
  inputs.cclock.url = "git+https://codeberg.org/tenyoru/cclock";

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

Start a 25-minute timer:

```sh
cclock -m 25 -t "Deep work"
```

Set running and paused colors:

```sh
cclock -m 5 -c steelblue -C '#ffd60a'
```

Control the active timer:

```sh
cclock --pause
cclock --resume
cclock --toggle
cclock --time-get
cclock --stop
```

Run `cclock --help` for concise option help or `man cclock` for the full
reference.

## Development

```sh
nix develop
just build
just test
```

cclock requires Linux, Wayland, and a compositor with layer-shell support. It
is developed and tested on niri.
