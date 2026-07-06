# Sysboard for OnePlus 6T

This is not the generic upstream Sysboard repository.

This repository is the OnePlus 6T / `oneplus-fajita` phone-image fork of
Sysboard. It exists specifically for the Arch Linux OnePlus 6T Hyprland phone
image, so a new reader should treat it as the phone-targeted build rather than a
neutral mirror of upstream Sysboard.

For the original general-purpose on-screen keyboard, use upstream Sysboard:
<https://github.com/System64fumo/sysboard>

## What This Fork Is For

Sysboard is a simple Wayland on-screen keyboard written with `gtkmm4` and
`gtk4-layer-shell`. This fork keeps the upstream codebase small, but ships
defaults and styling intended for the OnePlus 6T phone image.

In the OnePlus 6T Arch image, Sysboard is used as the touch keyboard for
Hyprland. It is expected to run on the phone display, appear above app windows,
and be toggled by the phone UI helpers.

## Phone-Specific Defaults

The current fork defaults are aimed at the OnePlus 6T display and touch layout:

- mobile layout enabled by default
- `360` pixel keyboard height
- zero outer margin
- semi-transparent dark keyboard background
- grey keycaps with white labels
- compact icon-style special keys for shift, backspace, hide, space, and enter
- the mobile-layout top-left key hides/minimizes the keyboard (labelled `Minimize` in the fork)
- short keypress haptics enabled by default on OnePlus 6T force-feedback input

The default config lives in:

```text
config.conf
```

The default phone styling lives in:

```text
style.css
```

Installed system copies are placed under:

```text
/usr/local/share/sys64/board/config.conf
/usr/local/share/sys64/board/style.css
```

User overrides can be placed under:

```text
~/.config/sys64/board/config.conf
~/.config/sys64/board/style.css
```

## Build

Required build/runtime pieces include:

```text
gtkmm-4.0
gtk4-layer-shell
wayland
wayland-protocols / wayland-scanner
pkgconf
make
gcc or clang
```

Build locally with:

```sh
make
```

Install with:

```sh
sudo make install
```

The Makefile installs:

```text
/usr/local/bin/sysboard
/usr/local/lib/libsysboard.so
/usr/local/share/sys64/board/config.conf
/usr/local/share/sys64/board/style.css
```

## Runtime Usage

The OnePlus 6T image normally starts and controls Sysboard through phone UI
helpers, but it can also be launched directly:

```sh
sysboard
```

Useful arguments:

```text
-m    Set margin
-H    Set height
-l    Set layout: full, mobile, mobile_numbers
-v    Print version information
```

Example phone-sized launch:

```sh
sysboard -m 0 -H 360 -l mobile
```

## OnePlus 6T Haptics

This fork can play a short force-feedback pulse on key press. It is controlled
from `config.conf`:

```ini
[main]
haptics=true
haptics-duration-ms=18
haptics-strength=0x2200
```

Set `haptics=false` in `~/.config/sys64/board/config.conf` to disable typing
haptics without rebuilding Sysboard.

If the haptics input device is not readable from the Sysboard process, the
keyboard still works and silently skips haptics.

## Signals

Sysboard can be controlled with signals:

```sh
pkill -USR1 sysboard   # show
pkill -USR2 sysboard   # hide
pkill -RTMIN sysboard  # toggle
```

When triggered manually, Sysboard may stay visible until another signal hides or
toggles it.

## Relationship To Upstream

This fork exists so the OnePlus 6T phone image can carry known-good mobile
defaults without forcing those choices onto upstream Sysboard users. Upstream is
still the right place for general-purpose Sysboard development.

When syncing from upstream, keep the phone-specific config and styling changes
visible in review so the public OnePlus 6T image does not silently regress.

## License

This fork keeps the upstream license. See `LICENSE`.

## Also Check Out

- <https://github.com/jjsullivan5196/wvkbd>
- <https://github.com/WayfireWM/wf-osk>
