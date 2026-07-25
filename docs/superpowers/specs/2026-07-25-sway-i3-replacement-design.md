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

## Amendments from on-hardware validation (Task 9) and final review

On-hardware testing (Task 9) and a final whole-branch review surfaced several
places where this spec no longer matched what was actually built. Rather than
rewriting the sections above, this section records the deltas:

- **Terminal is `kitty`, not `wezterm`.** `wezterm-gui` reliably SIGABRTs on
  first Wayland connection under Sway (`wl_display_dispatch_queue_pending`
  fatal abortᵇ). Task 9 first moved `$mod+Return`/`$mod+w` to `alacritty`
  as a quick fix, but `alacritty` was never added to `env-install.sh`'s
  install/stow lists on either distro. The final review resolved this by
  switching to `kitty` instead — already fully installed and stowed on both
  the apt and pacman branches. `wezterm` is dropped from the pacman
  package/stow lists entirely (Debian keeps it, unused by Sway).
- **Single auto-detecting bar, not two hardcoded-output bars.** The bar/output
  names in the original translation table (`DP-1.1`, `HDMI-0`, `DP-3`,
  `DP-1.2`) didn't match this machine's real wlroots output name
  (`HDMI-A-1`, single monitor) — the bar existed but had nothing to attach
  to. Fixed by collapsing to one `bar {}` block with no hardcoded `output`
  line (shows on every connected output automatically) and dropping the
  workspace-output pins and `exec_always swaymsg output ... disable` lines.
- **Rofi is native-Wayland out of the box, no XWayland needed.** The spec
  above assumed rofi ran via Sway's built-in XWayland support. On-hardware
  testing found Wayland support was merged into rofi mainline as of 2.0.0
  (the AUR `rofi-wayland` fork was removed upstream for this reason), and
  that's the version already installed here (`rofi -help` confirms
  `wayland: selected`). No config/package change was needed, and — combined
  with the other changes below — the final setup has no XWayland dependency
  anywhere.
- **`google-chrome` launched with `--ozone-platform-hint=auto`ᵃ.** Chrome
  defaults to X11/XWayland rendering on Linux; this flag forced native
  Wayland rendering instead, avoiding a dependency on `xorg-xwayland`.
  ᵃSuperseded 2026-07-26 — see the wl_shm crash amendment at the end of this
  section; Chrome and wezterm are now deliberately routed through XWayland.
- **`mako` + `polkit-gnome` added.** `wg_toggle.sh` uses `notify-send` and
  `nm-applet` needs a polkit authentication agent to modify network
  connections; neither had anything to talk to in a standalone Sway session
  (both silently worked during testing only because Plasma was also
  installed on the test machine). Added `mako` (Sway's standard notification
  daemon) and `polkit-gnome` (lightweight; deliberately not
  `polkit-kde-agent` or `lxqt-policykit`, which pull in more) to the pacman
  package list, plus `exec --no-startup-id` lines for both in the config.
- **`network-manager-applet`/`network-manager-gnome` + `sysstat` added to
  *both* distros.** `nm-applet` and `mpstat` are referenced by the shared
  i3blocks config (tray icon, `[cpu]` block) but were never installed by
  either package-manager branch — the tray was empty and the cpu block was
  broken on both. This is an intentional, confirmed exception to this spec's
  "Debian/apt install path must not change at all" scope constraint: the
  fix is a pre-existing bug unrelated to the Sway migration, and the user
  approved applying it to both distros rather than leaving Debian broken.
- **Focused-window border color added.** `default_border pixel 0` was
  carried over unchanged from the old i3 config (where it was also never
  configured), so there was no visible focus indicator. Changed to
  `default_border pixel 2` and added a `client.*` border-color block using
  a new `$color_focus_border #a6d189` (Catppuccin Frappe green, matching
  the palette already in use: `$color_highlight` is Frappe blue,
  `$color_non_focused_foreground` is Frappe lavender,
  `$color_urgent_foreground` is Frappe red).
- **Rounded corners considered, explicitly declined.** `swayfx` (a Sway fork
  with corner-radius/blur support) was discussed with the user and declined
  in favor of staying on vanilla Sway, which has no native support for
  corner radius. Not a gap — a deliberate choice.
- **`for_window` needs both a `class` and an `app_id` rule.** `class`
  criteria only match XWayland windows; native Sway/Wayland apps (e.g.
  GNOME Calculator) use `app_id` instead. Added
  `for_window [app_id="org.gnome.Calculator"] floating enable` alongside the
  existing `class="Gnome-calculator"` rule (harmless if the app ever runs
  under XWayland).

## Amendment (2026-07-26): wl_shm crash root cause refined, XWayland reintroduced for Chrome and wezterm

ᵇFollow-up debugging (2026-07-26) refined the wezterm crash's root cause, and
found the same bug hits Chrome — reopening the "no XWayland dependency
anywhere" state claimed in the amendments above:

- **Root cause:** both `wezterm-gui` and `google-chrome-stable
  --ozone-platform-hint=auto` abort with the identical low-level error,
  `[wayland-client error] Attempted to dispatch unknown opcode 0 for wl_shm,
  aborting.`, inside `libwayland-client.so`. `wayland` (1.25.0-1.1, installed
  2026-07-24) and `wlroots0.20`/`sway` (0.20.2-1.1/1:1.12-4.1, installed
  2026-07-25) are both bleeding-edge installs from within 48h of each other.
  Since the crash is identical across two unrelated codebases, reproduces
  under neither Plasma/kwin nor XWayland, and both packages are this recent,
  this is a `wayland`↔`wlroots` `wl_shm`-dispatch regression, not an
  application bug or a config gap — "known multi-year upstream wezterm/
  wlroots issue" (as previously recorded above) undersold how precisely
  this was pinned down and wrongly implied it was wezterm-specific.
- **Fix:** force XWayland for both apps rather than downgrading and
  `IgnorePkg`-locking `wayland`/`wlroots0.20` — the user's explicit
  preference, since a version lock is easy to forget to release, whereas an
  app-level flag is self-contained. `~/.config/chrome-flags.conf` (new,
  machine-local, not stowed) sets `--ozone-platform=x11`; the `$mod+c`
  binding dropped `--ozone-platform-hint=auto` (it was overriding back to
  native Wayland). `wezterm/.config/wezterm/wezterm.lua` got
  `config.enable_wayland = false` too, even though `wezterm` isn't in the
  pacman stow list — the file is still shared with the Debian/i3 path and
  still gets run manually on this machine.
- **Revert trigger:** re-run the repro (launch `google-chrome-stable` or
  `wezterm-gui start -- true` with no ozone/wayland override) whenever
  `wayland` or `wlroots0.20` next appear in a `pacman -Syu` upgrade list —
  those are the only two packages implicated. If it no longer crashes,
  remove the three changes above and Chrome/wezterm can go back to native
  Wayland.
