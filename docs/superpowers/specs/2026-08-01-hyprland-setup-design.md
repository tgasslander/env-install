# Hyprland as a second Wayland session alongside Sway

## Background

The Arch/CachyOS path of `env-install.sh` currently installs Sway as the
tiling WM (see
`docs/superpowers/specs/2026-07-25-sway-i3-replacement-design.md`). This spec
adds **Hyprland** as a second, primary session, configured to reproduce the
existing Sway window behavior and keybindings.

Hyprland's native layouts (`dwindle`, `master`) are automatic/dynamic and do
not reproduce i3/Sway's manual tree-based tiling. The
[`hy3`](https://github.com/outfoxxed/hy3) plugin restores it: explicit
splits, tabbed groups, focus-parent, and group-aware window movement. hy3 is
therefore not optional here — without it the "same window behavior" goal is
unreachable.

Sway is **kept installed and stowed**. hy3 is an out-of-tree Hyprland plugin
compiled against an exact Hyprland version and refuses to load against any
other, so every `hyprland` upgrade can break the desktop until a matching
plugin build lands. Sway is the break-glass session for those windows.

The Debian/apt path is unaffected and keeps installing i3 + X11 + picom.

## Decisions

Confirmed with the user before writing this spec:

- **Coexistence, not replacement.** Both sessions are installed, stowed, and
  offered by the greeter. Hyprland is the daily driver; Sway is the fallback
  when hy3 lags a Hyprland release. This deliberately departs from the
  i3→Sway spec's "full replacement, not a fallback" stance, because that
  migration had no equivalent version-coupling risk.
- **`hyprland-plugin-hy3` (AUR), not `hy3` (AUR).** At time of writing
  `aur/hyprland-plugin-hy3` is `0.56.1-2.1`, exactly matching
  `cachyos-extra-znver4/hyprland 0.56.1-2.1`. Upstream's own `aur/hy3` is
  still on `0.55.0-1`. Track the version-matched package.
- **Waybar with native modules where they exist.** Not a thin wrapper around
  the existing i3blocks scripts. See "Bar" below.
- **hyprlock + hypridle**, not swaylock + swayidle.
- **Full Hyprland look.** Rounding, blur, shadows and animations are enabled.
  Window *behavior* and *keybindings* match Sway; *aesthetics* deliberately
  do not. The Catppuccin Frappe palette already in use carries over as the
  base colors.
- **Chrome runs native Wayland under Hyprland, with no XWayland forcing.**
  This is a user priority, not an incidental cleanup — Chrome is the primary
  suspect in an open lag-spike investigation and must not carry an X11
  workaround that confounds it. See "Chrome" below.

## Scope

In scope:

- New `hypr` and `waybar` stow packages in `~/dotfiles`
- New `hyprland.inc` in this repo, plus Arch package-list and stow-line
  changes in `env-install.sh`
- An AUR-helper bootstrap (first AUR dependency this repo has ever had)
- A Chrome launch wrapper that picks the ozone platform per session
- `CLAUDE.md` update

Out of scope:

- Any change to the Debian/apt path
- Any change to the existing `sway` stow package, beyond what the Chrome
  wrapper requires
- Resolving the open Chrome `CrBrowserMain` degradation in `UPDATES.md`.
  Moving off XWayland removes a confounder from that investigation; it is
  **not** a fix, and that document already rules XWayland out as the cause.

## Repo layout

`~/dotfiles` (separate repo — committed there, **not pushed** without
separate confirmation):

```
hypr/.config/hypr/
    hyprland.conf
    hyprlock.conf
    hypridle.conf
    hyprpaper.conf
    exit.sh
    scripts/wg_toggle.sh
waybar/.config/waybar/
    config.jsonc
    style.css
scripts/.local/bin/
    chrome-session-launch
```

`sway/` is untouched and stays stowed. `wg_toggle.sh` is duplicated rather
than shared because the two `.config` trees are independent stow packages;
it is 17 lines and compositor-agnostic.

## Gaps do not translate 1:1

Sway's `gaps inner N` is the **total** gap between two adjacent windows.
Hyprland's `gaps_in` is applied to **each window edge**, so the visible gap
between two windows is `2 * gaps_in`.

| Sway | Hyprland |
|---|---|
| `gaps inner 10` | `gaps_in = 5` |
| `gaps outer 2` | `gaps_out = 2` |
| `default_border pixel 4` | `general { border_size = 4 }` |

Copying `10` into `gaps_in` produces double the intended spacing. This is the
single most likely way for the port to look subtly wrong.

## Keybinding translation

`set $mod Mod4` → `$mod = SUPER`. Bindings not listed below are mechanical
`bindsym X exec Y` → `bind = MOD, KEY, exec, Y` rewrites.

| Sway | Hyprland + hy3 |
|---|---|
| `$mod+h/j/k/l` focus | `hy3:movefocus, l\|d\|u\|r` |
| `$mod+Left/Down/Up/Right` focus | same dispatchers |
| `$mod+Shift+h/j/k/l` move | `hy3:movewindow, l\|d\|u\|r` |
| `$mod+Shift+v` `split h` | `hy3:makegroup, h, ephemeral` |
| `$mod+v` `split v` | `hy3:makegroup, v, ephemeral` |
| `$mod+e` `layout toggle split` | `hy3:changegroup, opposite` |
| `$mod+s` `layout stacking` | `hy3:changegroup, tab` — see Regressions |
| `$mod+a` `focus parent` | `hy3:changefocus, raise` |
| `$mod+q` `kill` | `hy3:killactive` (group-aware) |
| `$mod+f` `fullscreen toggle` | `fullscreen, 0` |
| `$mod+Shift+space` `floating toggle` | `togglefloating` |
| `$mod+N` workspace | `workspace, N` (`$mod+0` → `workspace, 10`) |
| `$mod+Shift+N` move to workspace | `hy3:movetoworkspace, N` — preserves group structure, unlike the bare `movetoworkspace` |
| `$mod+Ctrl+h/l` move ws to output | `movecurrentworkspacetomonitor, l\|r` |
| `$mod+Shift+c` `reload` | `exec, hyprctl reload` |
| `floating_modifier $mod` | `bindm = $mod, mouse:272, movewindow` and `bindm = $mod, mouse:273, resizewindow` |
| `mode "resize"` | `submap = resize` |
| `mode "$system"` | `submap = system` |
| `for_window [class=…] floating enable` | `windowrule { … match { class = … } }` — see the amendment below |

The exact hy3 dispatcher spellings above are taken from upstream
documentation and **must be verified against the installed plugin's README**
during implementation rather than trusted blind.

### Free improvements

Media and volume keys become `bindl` / `bindel`, so they work while the
session is locked and repeat when held. Sway's did neither. This is a
deliberate behavior *improvement*, not a translation error.

### Keyboard layout toggle

Sway needs `scripts/toggle_keyboard_layout.sh` (a `swaymsg -t get_inputs`
+ `jq` + substring-match script, itself carrying a `VERIFY:` comment about
the fragility of matching on `xkb_active_layout_name`).

Hyprland makes the script unnecessary. Declare both layouts once:

```
input { kb_layout = us,se }
```

and bind `$mod+i` to `exec, hyprctl switchxkblayout all next`. The script is
not ported. `lang.sh` is likewise replaced by Waybar's native
`hyprland/language` module.

## Autostart and wallpaper

Sway's four `exec` / `exec --no-startup-id` lines become `exec-once`:

| Sway | Hyprland |
|---|---|
| `exec swayidle -w …` | `exec-once = hypridle` (config-driven, see below) |
| `exec --no-startup-id nm-applet` | `exec-once = nm-applet` |
| `exec --no-startup-id mako` | `exec-once = mako` |
| `exec …/polkit-gnome-authentication-agent-1` | `exec-once = systemctl --user start hyprpolkitagent` |
| `exec-always waybar` *(new — replaces the `bar {}` block)* | `exec-once = waybar` |

Sway's `output * bg /usr/share/backgrounds/sway/Sway_Wallpaper_Blue_1920x1080.png fill`
becomes a `hyprpaper.conf` with the same image path and a `wallpaper = ,<path>`
entry (empty monitor field = all outputs), plus `exec-once = hyprpaper`. The
image ships with the `sway` package, which stays installed — so this creates a
real, if minor, dependency of the Hyprland session on the Sway package. That is
acceptable given Sway is retained deliberately as the fallback, and is noted
here so it is not mistaken for an oversight later.

## Behavioral regressions

Three bindings have no exact Hyprland equivalent. These are the only
intentional behavior losses:

1. **`$mod+s layout stacking`** — hy3 implements *tabbed* groups only; i3's
   *stacked* layout has no counterpart. Rebound to `hy3:changegroup, tab`.
2. **`$mod+Shift+t focus mode_toggle`** — no dispatcher moves focus between
   the tiling and floating stacks. Best-effort rebind: `cyclenext, floating`.
   Not equivalent.
3. **`$mod+Shift+e` swaynag exit confirmation** — `swaynag` is Sway-specific.
   Replaced with a `rofi -dmenu` yes/no confirmation, preserving the guard
   rather than binding a bare `exit`. Rofi is already installed and themed.

## Bar

Hyprland has no built-in bar, and nothing in the ecosystem consumes the
i3bar JSON protocol that `i3blocks` emits, so Sway's `bar { status_command
i3blocks }` cannot carry over. Waybar replaces it, using native modules
wherever one exists:

| i3blocks block | Waybar |
|---|---|
| *(none — Sway's built-in workspace list)* | native `hyprland/workspaces` |
| *(none)* | native `hyprland/window` |
| `spotify.sh` (`qdbus` + `pactl` grep) | native `mpris` |
| `[cpu]` (`mpstat` pipeline, 1 s interval) | native `cpu` |
| `lang.sh` | native `hyprland/language` |
| `battery.sh` | native `battery` |
| `[time]` | native `clock` |
| `tray_output *` | native `tray` |
| `coderkeeb.sh` | `custom/coderkeeb` — no native equivalent (`bluetoothctl`) |
| `mx_master.sh` | `custom/mx_master` — no native equivalent (`bluetoothctl`) |
| `wireguard.sh` | `custom/wireguard` — no native equivalent (`wg0` link state) |

The three surviving custom modules run the existing scripts from the
`i3blocks` stow package unchanged, at their current 10 s intervals. That
package stays the single source of truth for those three blocks across both
sessions.

`hyprland/workspaces` is not optional — without it the bar has no workspace
indicator at all, since that was Sway's `bar {}` doing the work.

### Incidental fix

`battery.sh` reads `/sys/class/power_supply/BAT0/capacity`, which does not
exist on this desktop (Ryzen 9950X, no battery). It has been failing silently
every 10 s. Waybar's native `battery` module hides itself when no battery is
present.

## Lock, idle and power

`swaylock`/`swayidle` are replaced by `hyprlock`/`hypridle` for the Hyprland
session. Sway's session keeps its own, unchanged.

The behavior to reproduce, from the Sway config:

- Lock on `$mod+Shift+q` → `l`, and before sleep.
- Blank the outputs 60 s after last input, **but only while locked** — Sway
  achieved this with a `pgrep -x swaylock` guard inside `swayidle`'s
  `timeout` action, so an unlocked idle session keeps its display.

hypridle expresses this natively with two `listener` blocks: a `lock_cmd`,
a `before_sleep_cmd`, and a 60 s listener whose `on-timeout` is
`hyprctl dispatch dpms off` guarded on `pgrep -x hyprlock`. `hyprlock.conf`
reuses the existing `lockscreen.png`.

`exit.sh` is copied into the `hypr` package with `swaylock` → `hyprlock` and
`swaymsg exit` → `hyprctl dispatch exit`. The `systemctl` suspend / hibernate
/ reboot / poweroff paths are unchanged.

**Verification requirement.** This machine is S3-only — `s2idle` hard-locks
it via amdgpu — and wakes from suspend via a Bluetooth dongle. hypridle takes
over the sleep-inhibitor hook that `swayidle -w` previously held, so the
lock-before-suspend chain must be explicitly re-tested rather than assumed to
carry over.

## Chrome

`~/.config/chrome-flags.conf` currently contains `--ozone-platform=x11`,
added 2026-07-26 to work around a `wl_shm` dispatch abort in
`wayland 1.25.0` ↔ `wlroots0.20 0.20.2`.

Two facts make this the wrong setting for the Hyprland session:

1. Hyprland 0.56 does not use wlroots. It renders through **aquamarine**, so
   the implicated wlroots code path is not in play.
2. Chrome is the primary suspect in an open lag-spike investigation
   (`UPDATES.md` §1). Leaving an X11 compatibility workaround in its launch
   path adds a confounding variable to that investigation.

`chrome-flags.conf` cannot express this itself. The launcher reads it with
`grep -v '^#' "$XDG_CONFIG_HOME/chrome-flags.conf"` — flat text, no shell
evaluation, no conditionals — so the decision must happen outside that file.

**Design:** a `chrome-session-launch` wrapper in the `scripts` stow package:

```sh
if [ -n "$SWAYSOCK" ]; then
    exec google-chrome-stable --ozone-platform=x11 "$@"
fi
exec google-chrome-stable "$@"      # native Wayland
```

The `--ozone-platform=x11` line is removed from `chrome-flags.conf`, leaving
that file free of platform flags. Both the Hyprland `$mod+c` binding **and**
a `~/.local/share/applications/google-chrome.desktop` override point at the
wrapper, so rofi launches, `.desktop` activations and link handlers from
other applications all take the same path. Covering only the keybinding would
produce a confusing mix of XWayland and native Wayland Chrome windows in one
session.

Sway's `$mod+c` binding is repointed at the wrapper too, so there is exactly
one place where the platform decision lives.

**Revert trigger** (inherited from the 2026-07-26 amendment): if native
Wayland Chrome aborts on `wl_shm` under Hyprland, the wrapper's fallback
branch is a one-line change. That would also disprove the aquamarine
reasoning above and should be recorded here.

### wezterm carries the identical problem

`$mod+Return` launches `wezterm`, which SIGABRTs with the same `wl_shm` error
and is suppressed by `config.enable_wayland = false` in `wezterm.lua`. The
same aquamarine reasoning applies, but `wezterm.lua` is shared with the
Debian/i3 path, so it cannot simply be flipped.

This spec ports `$mod+Return exec wezterm` **as-is**, with `enable_wayland =
false` left in place, and records native-Wayland wezterm under Hyprland as a
separate verification item. It is deliberately not bundled into the Chrome
change, which the user raised specifically.

Note also that the 2026-07-25 spec's amendments record a `wezterm` → `kitty`
switch for `$mod+Return` that **never landed** — the live Sway config still
binds `wezterm`. What is actually configured is what gets ported.

## Packages and the AUR

Added to the pacman branch of `env-install.sh`:

```
hyprland hyprlock hypridle hyprpaper hyprpolkitagent
xdg-desktop-portal-hyprland waybar
```

All are in `cachyos-extra-znver4`/`extra`. `hyprland-plugin-hy3` is AUR-only.

`polkit-gnome` stays installed for the Sway session; the Hyprland session
uses `hyprpolkitagent` instead. `mako` stays and is shared — it is
compositor-agnostic and `wg_toggle.sh` needs `notify-send`.
`xdg-desktop-portal-hyprland` is required for screen sharing and portal-based
file pickers.

**This repo has never installed from the AUR.** Every existing Arch path is
`pacman`, an upstream installer, or a git clone. `hyprland.inc` therefore
needs to bootstrap an AUR helper:

```sh
command -v yay >/dev/null 2>&1 || (clone yay-bin, makepkg -si)
yay -S --needed hyprland-plugin-hy3
```

The guard makes this a no-op on the current machine, which already has `yay`,
while keeping the script correct on a fresh box. `makepkg` must not run under
`sudo`, consistent with `env-install.sh`'s existing "run from repo root, not
via sudo" constraint.

## `hyprland.inc` structure

Follows the existing `.inc` conventions documented in `CLAUDE.md`. It is
**executed**, not sourced — like `k8s.inc`, `docker.inc` and `fonts.inc` —
because nothing in it needs to leak into the parent shell. It therefore sees
exported `PKG_MGR` but not `common.inc`'s `pkg_install`/`pkg_update`
functions, and must call `pacman`/`yay` directly.

It is a no-op on the apt path.

Ordering: after the main package install (so `hyprland` is present before the
plugin is built against it) and before the `stow` step.

The stow line gains `hypr` and `waybar` on the pacman branch only:

```sh
stow -R sway hypr waybar i3blocks nvim starship Xresources tmux kitty rofi scripts
```

## Manual steps

These cannot be scripted and must be documented for the user:

1. **Select the Hyprland session at the greeter.** The
   `hyprland.desktop` session file is installed by the package, but the
   greeter's remembered session must be switched by hand once.
2. **Log fully out and back in** rather than reloading — the session's
   environment (`XDG_CURRENT_DESKTOP`, portal registration) is set at
   session start.
3. **Confirm the hy3 plugin path.** `hyprland.conf` loads the plugin with an
   absolute `plugin = /usr/lib/libhy3.so` line; verify the real install path
   with `pacman -Ql hyprland-plugin-hy3` after installing and correct the
   config if it differs.
4. **Re-test suspend/resume** (S3, Bluetooth dongle wake) after the switch —
   see "Lock, idle and power".
5. **After every `hyprland` upgrade**, confirm hy3 still loads
   (`hyprctl plugin list`). If it does not, log into Sway and rebuild the
   plugin.

## Verification

- `hyprctl plugin list` shows hy3 loaded.
- Every binding in the translation table produces its Sway-equivalent
  behavior, checked by hand against the running Sway session.
- Waybar shows ten modules, with workspaces tracking correctly and the tray
  populated by `nm-applet`. The eleventh, `battery`, is expected to be
  absent — this box has no battery, and self-hiding is the module behaving
  correctly.
- `hyprctl clients -j` confirms Chrome is a native Wayland client, not
  XWayland (`xwayland: false`).
- Lock, idle-blank-while-locked, and lock-before-suspend all behave as they
  did under Sway.
- `bash -n`, `shellcheck` and `shfmt -d` pass on `hyprland.inc` and the
  Chrome wrapper — this repo's only static checks.

## Amendment (2026-08-01): window-rule syntax, and a verify gate

This spec was written before Hyprland had ever run on this machine, against
an older config API. One construct in it is invalid on the installed
0.56.1, and the way it was caught is worth recording.

**The defect.** The translation table originally specified
`windowrulev2 = float, class:^(…)$`. On Hyprland 0.56.1 that produces
`windowrulev2 is deprecated`. A bare rename to `windowrule` also fails, with
`invalid field float: missing a value` — the grammar changed, not just the
keyword.

**The real grammar.** `windowrule` is now a *special category keyed by
`name`*: action fields at the top level, match criteria in a nested `match`
block. Probing the parser established that `float`, `content`, `tag`,
`fullscreen`, `pin`, `opacity`, `workspace`, `monitor`, `size` and `move`
are valid top-level fields, while `class`, `title`, `initialClass`,
`initialTitle` and `xwayland` are not — those are match criteria. The
verified replacement, which makes the whole config return `config ok`:

```
windowrule {
    name = calculator
    float = true
    match {
        class = ^(org\.gnome\.Calculator|[Gg]nome-calculator)$
    }
}
```

**The gate.** `hyprland --verify-config --config <file>` parses a config
offline, launching no compositor, and is safe to run from inside a running
Sway session. It is now mandatory before any commit that touches
`hyprland.conf`. Nothing in the original spec called for it, which is why
the defect would otherwise have surfaced at first login.

**A second defect, which the gate could not catch.** This spec also specified
hy3 tab colors as `col.active` / `col.inactive` / `col.urgent`. Those keys do
not exist — `col.*` is Hyprland core's naming convention, not hy3's, which
nests them in their own block:

```
tabs {
    colors {
        active = rgb(a6d189)
        inactive = rgb(232634)
        urgent = rgb(e78284)
    }
}
```

`--verify-config` reported `config ok` on the wrong version, because plugin
config keys are registered by the plugin at load time and the verifier loads
no plugins. **Anything inside `plugin { }` is outside the gate** and must be
checked against the plugin binary instead:

```bash
strings /usr/lib/libhy3.so | grep -oE 'plugin:hy3:[a-z_:.]+' | sort -u
strings /usr/lib/libhy3.so | grep -oE '^hy3:[a-z]+' | sort -u
```

Run against the installed `hyprland-plugin-hy3 0.56.1-2.1`, this confirmed
all eight dispatchers the keybinding table relies on
(`hy3:movefocus`, `hy3:movewindow`, `hy3:makegroup`, `hy3:changegroup`,
`hy3:changefocus`, `hy3:killactive`, `hy3:movetoworkspace`,
`hy3:setephemeral`) do exist — so the translation table's dispatcher names
are sound even though its plugin config keys were not.

**The general lesson**, which applies to the rest of this document: where
this spec asserts Hyprland syntax, the installed parser is the authority,
not the spec — and where the parser has no opinion (plugin blocks), the
plugin binary is. Two of this spec's Hyprland constructs were wrong on first
contact with the real system; both were found by tooling rather than by
reading, which is the argument for running the checks before first login
rather than debugging a broken session afterwards.

## Risks

- **hy3 version coupling is permanent, not a migration cost.** It recurs on
  every Hyprland release. The Sway fallback is the mitigation; nothing
  removes the underlying fragility.
- **hy3 dispatcher names are from documentation, not from a running system.**
  The translation table is the highest-risk part of this spec and is
  verified during implementation, not before.
- **Native Wayland Chrome under Hyprland is reasoned, not tested.** The
  aquamarine argument is sound but unverified on this hardware; the wrapper
  makes the fallback a one-line change.
- **`hyprland/workspaces` is the only workspace indicator.** If the Waybar
  config is wrong, there is no built-in bar to fall back on as there was
  under Sway.
