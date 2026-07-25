# Sway as the i3 replacement on Arch-based distros

## Background

`env-install.sh` currently installs i3 (X11) on both Debian and Arch. On a
fresh CachyOS box, choosing the i3 login-greeter session failed with a black
screen and no keyboard/mouse input. Root cause: Arch's `i3-wm` package has no
dependency on `xorg-server` (unlike Debian's `i3` package, which pulls in
`xserver-xorg` via Recommends), so i3 was installed and offered as a session
with no X server present to actually run it — `plasmalogin` tried to exec
`/usr/bin/X`, which didn't exist, and the session crashed to a bare VT.

Rather than papering over this by adding `xorg-server` to the Arch package
list, we're replacing i3 with **Sway** on Arch-based distros: a Wayland
compositor deliberately built to be i3-config-compatible (same keybinding
syntax, same manual/tree-based tiling model, same workspace semantics,
i3bar-protocol-compatible bar). This avoids the X11 stack entirely on Arch,
matching CachyOS's Wayland-first default (Plasma/Wayland), while preserving
the user's existing keybindings and window behavior.

The Debian/apt path is unaffected by this change and keeps installing i3 + X11
+ picom exactly as it does today.

## Decisions

These were confirmed with the user before writing this spec:

- **Full replacement, not a fallback.** Arch installs Sway only — no
  `xorg-server`, no `i3-wm`, no `picom`. If something in the translated config
  doesn't work, it gets fixed in Sway rather than falling back to an X11
  session.
- **Hardware workaround scripts are translated now, best-effort.** The
  existing `~/.config/i3/scripts/` directory has X11-only hacks for a flaky
  HDMI/dock setup (`kick_hdmi.sh`, `thunderbolt.sh`,
  `toga_screens_office.sh`, `dpms-hdmi-fix.sh`). These get ported to
  `wlr-randr`/`swaymsg` syntax now, but are explicitly flagged as unverified:
  output names may not match between X11 RandR and wlroots, and wlroots may
  handle hotplug/DPMS well enough that some of these hacks are no longer
  needed at all. This is only verifiable once Sway is actually running on the
  user's hardware.
- **Rofi stays, unchanged.** Rofi keeps running via Sway's built-in XWayland
  support. No config or theme changes — the existing catppuccin rofi theme is
  untouched. (Considered switching to `wofi` for a pure-Wayland launcher;
  rejected because it would require rewriting the theme from scratch for
  marginal benefit, and rofi-wayland forks aren't available in CachyOS's
  configured repos without adding AUR support.)
- **`~/dotfiles` (separate repo) gets edited too.** The Sway config lives in
  the user's dotfiles repo (`tgasslander/dotfiles`), not in this repo. A new
  `sway` stow package is added there, mirroring the existing `i3` package.
  Changes are committed there but **not pushed** without separate
  confirmation.

## Scope

In scope:
- `env-install.sh` Arch (pacman) package list and stow line
- New `~/dotfiles/sway` stow package (config + scripts), replacing the
  functional role of `~/dotfiles/i3` + `~/dotfiles/picom` on Arch
- Translating every i3-config feature currently in use (see inventory below)
  to its Sway/Wayland equivalent

Out of scope:
- Any change to the Debian/apt install path (stays i3 + X11 + picom, as-is)
- Rewriting the i3blocks status scripts (`battery.sh`, `mx_master.sh`,
  `coderkeeb.sh`, `spotify.sh`, `wireguard.sh`, `lang.sh` output logic) — none
  of these touch X11 except `lang.sh`'s layout query, which is covered below
- Switching to a different bar (waybar, etc.) — Sway's built-in bar covers
  everything i3bar did, including tray, so there's no need
- Verifying/fixing real monitor output names, since that requires the actual
  hardware and a running Sway session

## Package changes (`env-install.sh`)

Arch (`pacman`) branch:

**Remove:** `i3-wm`, `i3lock`, `picom`, `xorg-server` (the last of these was
never actually installed — this spec supersedes the earlier plan to add it)

**Add:** `sway`, `swaylock`, `swayidle`, `swaybg`, `brightnessctl`,
`wlr-randr`, `jq`

(`jq` is needed by the translated keyboard-layout scripts, which parse
`swaymsg -t get_inputs`/`get_outputs` JSON output.)

`feh` is dropped from the Arch list (no longer needed — `swaybg` replaces its
one use, setting the wallpaper). `i3blocks`, `rofi`, `wezterm`, `wget`,
`unzip`, `base-devel`, `python` are unchanged.

The Debian (`apt`) branch is not touched.

`i3.inc` stays as-is — it currently only contains the Debian-specific
third-party i3 repo setup and is already skipped on Arch (`command -v
pacman` check), so no new `.inc` file is needed for this change.

The `stow` line in `env-install.sh` branches by `$PKG_MGR`:
- Arch: `stow -R sway i3blocks nvim starship Xresources tmux kitty rofi wezterm`
- Debian: unchanged (`stow -R i3 i3blocks nvim starship Xresources tmux kitty picom rofi wezterm`)

## Config translation inventory

Every feature in the current `~/dotfiles/i3/.config/i3/config` and its
supporting scripts, and its Sway equivalent:

| i3 feature | Sway equivalent |
|---|---|
| `gaps inner/outer`, `set $mod`, `floating_modifier`, all `bindsym` focus/move/split/resize/fullscreen/workspace/layout bindings, `mode "resize"`, `mode "$system"` | Unchanged — same syntax, Sway is config-compatible |
| `font pango:...` | Unchanged |
| `exec_always --no-startup-id picom` | Removed — Sway composites natively, no separate compositor process |
| `exec ... xss-lock --transfer-sleep-lock -- i3lock --nofork` | `exec swayidle -w timeout 600 'swaylock -f' before-sleep 'swaylock -f'` |
| `exec --no-startup-id nm-applet` | Unchanged exec; add `tray_output primary` to both `bar {}` blocks so Sway's native SNI tray support picks it up (i3bar has no tray; Sway's does) |
| `bindsym XF86AudioRaiseVolume/... exec pactl ...` | Unchanged (pactl is DE-agnostic) |
| `bindsym XF86MonBrightnessUp/Down exec xbacklight -inc/-dec 20` | `brightnessctl set 20%+` / `brightnessctl set 20%-` |
| `bar { ... status_command i3blocks ... }`, `output DP-1.1`/`HDMI-0`/`DP-3`, `workspace N output ...` | Unchanged syntax (Sway supports `bar`, `output`, and `workspace <n> output <name>` natively); **output names flagged for on-hardware verification** — X11 RandR names may not match wlroots connector names |
| `exec_always xrandr --output DP-1.1 --off` (and `DP-1.2`) | `exec_always swaymsg output DP-1.1 disable` (name to verify) |
| `exec_always feh --bg-scale <path1> --bg-scale <path2>` | `output * bg <path> fill` per output, using Sway's native wallpaper directive instead of a separate `swaybg` exec |
| `bindsym $mod+i exec toggle_keyboard_layout.sh` (uses `setxkbmap -query`/`setxkbmap se\|us`) | Rewritten to read current layout via `swaymsg -t get_inputs \| jq` and set it via `swaymsg input type:keyboard xkb_layout se\|us` |
| `lang.sh` (uses `setxkbmap -print`) | Rewritten to read the active layout the same way, via `swaymsg -t get_inputs \| jq` |
| `i3exit.sh`: `i3-msg exit` | `swaymsg exit` |
| `i3exit.sh`: `i3lock -t -i <path>` | `swaylock -i <path>` |
| `i3exit.sh`: `systemctl suspend/hibernate/reboot/poweroff` | Unchanged (DE-agnostic) |
| `dpms-hdmi-fix.sh` (`xset q`, `xrandr --output HDMI-0 --auto`) | Rewritten using `wlr-randr` output state instead of `xset`/`xrandr`; ported best-effort per the decision above — may prove unnecessary under wlroots |
| `kick_hdmi.sh`, `thunderbolt.sh`, `toga_screens_office.sh` (`xrandr --output ... --mode ...`) | Rewritten using `wlr-randr --output <name> --mode <WxH>`; output names and necessity both flagged for on-hardware verification |
| `wg_toggle.sh` | Unchanged (no X11 dependency — `ip`, `wg-quick`, `dhclient`, `notify-send` are all DE-agnostic) |
| `for_window [class="Gnome-calculator"] floating enable` | Unchanged — Sway supports `for_window` and X11 `class` criteria (via XWayland) identically |
| `bindsym $mod+space/$mod+d exec rofi -show drun/run` | Unchanged — runs via Sway's built-in XWayland |
| `bindsym $mod+Return exec wezterm`, `$mod+c exec google-chrome` | Unchanged |
| Session file (`/usr/share/xsessions/i3.desktop`) | No work needed — the `sway` Arch package ships its own `/usr/share/wayland-sessions/sway.desktop`, so it appears in the login greeter automatically once installed |

## Validation plan

The multi-monitor output names and the DPMS/monitor-kick scripts cannot be
verified from this environment — they depend on the actual dock/HDMI hardware
and a live Sway session. After install, the user runs `swaymsg -t get_outputs`
and the output names/assignments in the config and scripts get corrected
together against real values, rather than guessed blind. Everything else
(keybindings, workspace behavior, bar, lock/idle, brightness, tray, rofi,
i3blocks) is expected to work as soon as Sway starts, since it's either
unchanged syntax or a mechanical tool substitution with no hardware
dependency.

## Testing

No automated test suite exists for this repo (per `CLAUDE.md`); validation is
`bash -n` on any shell script and `shellcheck` where available, plus the
on-hardware manual check described above. This matches the existing
project convention.
