# Hyprland Alongside Sway — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add Hyprland + the hy3 plugin as a second Wayland session that reproduces the existing Sway session's window behavior and keybindings, with Sway retained as a fallback.

**Architecture:** Two new stow packages in `~/dotfiles` (`hypr`, `waybar`) plus a Chrome launch wrapper in the existing `scripts` package. One new `hyprland.inc` in this repo bootstraps an AUR helper and installs the version-matched hy3 plugin; the repo-available packages go in `env-install.sh`'s existing pacman list. Nothing on the Debian/apt path changes.

**Tech Stack:** Hyprland 0.56.1 (aquamarine renderer), `hyprland-plugin-hy3` (AUR), Waybar, hyprlock/hypridle/hyprpaper/hyprpolkitagent, GNU stow, Bash.

**Spec:** `docs/superpowers/specs/2026-08-01-hyprland-setup-design.md`

## Global Constraints

- **Never run `sudo` from an agent session on this machine.** There is no TTY for the password prompt and failed attempts risk a `pam_faillock` lockout. Every `sudo`, `pacman`, `yay` and `makepkg -si` step is handed to the user to run via `! <command>`. Tasks below mark these **[USER RUNS]**.
- **Two repos are in play.** `~/Downloads/temp/env-install` (this repo) and `~/dotfiles` (separate, remote `git@github.com:tgasslander/dotfiles.git`, branch `master`). Commit in both; **never push `~/dotfiles`** without separate confirmation.
- **The Debian/apt path must not change at all.** All `env-install.sh` edits go inside the existing `else` (pacman) branch.
- **Sway stays installed and stowed.** It is the fallback when a `hyprland` upgrade outruns hy3.
- **Gap conversion:** `gaps_in = 5`, `gaps_out = 2`, `border_size = 4`. Sway's `gaps inner 10` is the total gap between two windows; Hyprland applies `gaps_in` per window edge, so it doubles.
- **Palette (Catppuccin Frappe), copied verbatim from the Sway config:** focus border `#a6d189`, unfocused `#232634`, highlight `#8caaee`, non-focused foreground `#babbf1`, urgent `#e78284`, active background `#51576d`, background `#000000`.
- **Static checks are this repo's only automated tests:** `bash -n <file>`, `shellcheck <file>`, `shfmt -d <file>`. There is no test suite. Config correctness is verified with `hyprctl configerrors` and on-hardware checks.
- **`hyprland.inc` is executed, not sourced** (like `k8s.inc`, `docker.inc`, `fonts.inc`). It sees exported `PKG_MGR` but **not** `common.inc`'s `pkg_install`/`pkg_update` helpers, so it calls `pacman`/`yay` directly.
- **hy3 dispatcher names in this plan come from upstream documentation, not a running system.** Task 4 verifies them against the installed plugin before they are trusted.
- **Mandatory config verify gate.** Every task that writes or edits `hyprland.conf` MUST run, and show the output of, this before committing:

  ```bash
  hyprland --verify-config --config /home/toga/dotfiles/hypr/.config/hypr/hyprland.conf
  ```

  It is offline, launches no compositor, and is safe while Sway is running.

  **Expected output depends on whether the file contains hy3 bindings.** `--verify-config` validates dispatcher names but loads no plugins, so every `hy3:*` binding is reported as `Invalid dispatcher, requested "hy3:…" does not exist`. That is expected and not a defect. The pass condition is therefore:

  - Before Task 4 (no hy3 bindings): `config ok`.
  - From Task 4 onward: **zero non-`hy3:` errors.** Check with
    ```bash
    hyprland --verify-config --config <file> 2>&1 | grep 'Config error' | grep -v 'hy3'
    ```
    which must print nothing. Any non-hy3 error must be fixed before the commit. This gate exists because the plan was authored against an older Hyprland config API: `windowrulev2` was specified here and is deprecated in the installed 0.56.1, which this check caught. Treat every other Hyprland construct in this plan as similarly suspect — the parser, not the plan, is the authority on syntax.

- **The verify gate does NOT cover `plugin { }` blocks.** Plugin config keys are registered by the plugin at load time, so `--verify-config` — which loads no plugins — reports `config ok` on a `plugin { hy3 { … } }` block full of names hy3 has never heard of. This is not hypothetical: the plan originally specified `col.active` / `col.inactive` / `col.urgent` for hy3 tab colors, which passed the gate and are wrong (hy3 nests them under `colors { }`). For anything inside `plugin { }`, verify names against the plugin binary instead:

  ```bash
  strings /usr/lib/libhy3.so | grep -oE 'plugin:hy3:[a-z_:.]+' | sort -u   # config keys
  strings /usr/lib/libhy3.so | grep -oE '^hy3:[a-z]+' | sort -u             # dispatchers
  ```

  Both were run against the installed `hyprland-plugin-hy3 0.56.1-2.1`. All eight dispatchers this plan uses (`hy3:movefocus`, `hy3:movewindow`, `hy3:makegroup`, `hy3:changegroup`, `hy3:changefocus`, `hy3:killactive`, `hy3:movetoworkspace`, `hy3:setephemeral`) are confirmed to exist.

---

### Task 1: Package installation — `env-install.sh` list + `hyprland.inc`

**Files:**
- Create: `hyprland.inc`
- Modify: `env-install.sh` (pacman package list; new `hyprland.inc` invocation; stow line)

**Interfaces:**
- Consumes: `PKG_MGR` exported by `common.inc` (values `pacman` or `apt`).
- Produces: `hyprland`, `hyprlock`, `hypridle`, `hyprpaper`, `hyprpolkitagent`, `xdg-desktop-portal-hyprland`, `waybar` from the repos; `hyprland-plugin-hy3` from the AUR; a `yay` binary on `PATH`. Later tasks depend on the hy3 shared object existing on disk so its path and dispatcher list can be read.

- [ ] **Step 1: Add the repo packages to the pacman branch of `env-install.sh`**

In the `else` branch of the package install (the Arch side, currently ending `wl-clipboard`), add these lines. Do not touch the `if [ "$PKG_MGR" = "apt" ]` branch.

```bash
		hyprland hyprlock hypridle hyprpaper hyprpolkitagent \
		xdg-desktop-portal-hyprland waybar \
```

Place them immediately after the existing `sway swaylock swayidle swaybg brightnessctl wlr-randr jq \` line, and update the comment above that block to read:

```bash
	# Sway replaces i3+X11+picom on Arch: it's Wayland-native (no xorg-server
	# needed at all) and deliberately i3-config-compatible. See
	# docs/superpowers/specs/2026-07-25-sway-i3-replacement-design.md.
	#
	# Hyprland is installed alongside it as the primary session, with Sway
	# kept as the fallback: hy3 (the plugin that gives Hyprland i3-style
	# manual tiling) is compiled against an exact Hyprland version and
	# refuses to load after a mismatched upgrade. See
	# docs/superpowers/specs/2026-08-01-hyprland-setup-design.md.
```

- [ ] **Step 2: Create `hyprland.inc`**

```bash
#!/bin/bash

# Hyprland's AUR dependency. Arch/pacman only — no-op on Debian, which keeps
# i3 + X11.
#
# Executed, not sourced (like k8s.inc/docker.inc/fonts.inc): nothing here
# needs to leak into the parent shell. That also means common.inc's
# pkg_install/pkg_update are NOT in scope — only the exported PKG_MGR — so
# pacman and yay are called directly.
#
# The repo-available Hyprland packages are installed from env-install.sh's
# main pacman list. This file exists solely because hy3 is AUR-only, and is
# the first AUR dependency this repo has ever had.

set -eu

if [ "${PKG_MGR:-}" != "pacman" ]; then
	echo "Not an Arch-based system, skipping Hyprland plugin setup"
	exit 0
fi

# hy3 gives Hyprland i3/Sway-style manual tree tiling. It is compiled against
# an exact Hyprland version and refuses to load against any other, so it must
# be reinstalled after every hyprland upgrade. Track
# `hyprland-plugin-hy3` rather than upstream's own `hy3` package: the former
# is kept version-matched to the hyprland in the CachyOS repos, the latter
# lags (0.55.0 vs 0.56.1 at time of writing).
if ! command -v yay >/dev/null 2>&1; then
	echo "Bootstrapping the yay AUR helper"
	# makepkg refuses to run as root by design, which is consistent with this
	# repo's "run env-install.sh from the repo root, not via sudo" rule.
	tmpdir=$(mktemp -d)
	git clone --depth 1 https://aur.archlinux.org/yay-bin.git "${tmpdir}/yay-bin"
	(cd "${tmpdir}/yay-bin" && makepkg -si --noconfirm)
	rm -rf "${tmpdir}"
else
	echo "yay already installed, skipping bootstrap"
fi

echo "Installing the hy3 plugin (version-matched to hyprland)"
yay -S --needed --noconfirm hyprland-plugin-hy3

# hyprland.conf loads the plugin by absolute path; report it here so a
# packaging change is visible at install time rather than as a silent
# "plugin failed to load" at session start.
echo "hy3 plugin object installed at:"
pacman -Ql hyprland-plugin-hy3 | grep -F '.so' || echo "  WARNING: no .so found in hyprland-plugin-hy3"
```

- [ ] **Step 3: Invoke `hyprland.inc` from `env-install.sh`**

Insert immediately after the existing `source i3.inc` block and before the `echo "nvm"` line:

```bash
echo "Hyprland plugin (hy3)"
echo
# Executed, not sourced: nothing here needs to leak into the parent shell.
# No-op on Debian. Must run after the main package install so hyprland is
# already present for the plugin to build against.
./hyprland.inc
```

- [ ] **Step 4: Add the new stow packages to the pacman stow line**

Change the pacman branch stow line from:

```bash
	stow -R sway i3blocks nvim starship Xresources tmux kitty rofi scripts
```

to:

```bash
	stow -R sway hypr waybar i3blocks nvim starship Xresources tmux kitty rofi scripts
```

Leave the apt branch's `stow -R i3 i3blocks ...` line untouched.

**Ordering hazard:** `stow` errors out on a package directory that does not exist, and `hypr`/`waybar` are not created until Tasks 3 and 6. Between this step and Task 6, a full `./env-install.sh` run will fail at the stow line. Either finish Tasks 3 and 6 before running the bootstrap end-to-end, or create the two directories empty now with `mkdir -p ~/dotfiles/hypr ~/dotfiles/waybar`.

- [ ] **Step 5: Make `hyprland.inc` executable and run the static checks**

```bash
cd ~/Downloads/temp/env-install
chmod +x hyprland.inc
bash -n hyprland.inc && bash -n env-install.sh
shellcheck hyprland.inc env-install.sh
shfmt -d hyprland.inc env-install.sh
```

Expected: `bash -n` silent, `shellcheck` clean, `shfmt -d` prints no diff. If `shfmt -d` prints a diff, apply it with `shfmt -w` and re-run.

- [ ] **Step 6: Commit**

```bash
cd ~/Downloads/temp/env-install
git add hyprland.inc env-install.sh
git commit -m "Install Hyprland, Waybar and the hy3 plugin on Arch"
```

- [ ] **Step 7: [USER RUNS] Install the packages**

The agent must not run these — no TTY for sudo. Hand them to the user:

```
! sudo pacman -S --needed hyprland hyprlock hypridle hyprpaper hyprpolkitagent xdg-desktop-portal-hyprland waybar
! yay -S --needed hyprland-plugin-hy3
```

- [ ] **Step 8: Record the real hy3 plugin path**

```bash
pacman -Ql hyprland-plugin-hy3 | grep -F '.so'
```

Expected: one line ending in `libhy3.so`. **Write the exact path down** — Task 3 hardcodes it into `hyprland.conf`, and the plan's assumed `/usr/lib/libhy3.so` may be wrong.

---

### Task 2: Chrome per-session launch wrapper

**Files:**
- Create: `~/dotfiles/scripts/.local/bin/chrome-session-launch`
- Create: `~/dotfiles/scripts/.local/share/applications/google-chrome.desktop`
- Modify: `~/.config/chrome-flags.conf` (machine-local, not stowed — remove the platform line)
- Modify: `~/dotfiles/sway/.config/sway/config:192`

**Interfaces:**
- Produces: a `chrome-session-launch` executable on `PATH` (via `~/.local/bin`) taking the same arguments as `google-chrome-stable`. Task 4 binds `$mod+c` to it.

- [ ] **Step 1: Create the wrapper**

`~/dotfiles/scripts/.local/bin/chrome-session-launch`:

```sh
#!/bin/sh
# Chrome's ozone platform is chosen per session, not in chrome-flags.conf.
# That file is read by /usr/bin/google-chrome-stable with
# `grep -v '^#' "$XDG_CONFIG_HOME/chrome-flags.conf"` — flat text, no shell
# evaluation — so it cannot express a conditional.
#
# Sway: wayland 1.25.0 + wlroots0.20 0.20.2 abort native-Wayland Chrome on
# wl_shm dispatch (see the 2026-07-26 amendment in the sway spec), so that
# session forces XWayland.
#
# Hyprland: renders through aquamarine, not wlroots, so the implicated code
# path is not in play and Chrome runs native Wayland with no flags at all.
# This is deliberate — Chrome is the primary suspect in an open lag-spike
# investigation (UPDATES.md) and must not carry an X11 workaround that
# confounds it.
#
# Debian/i3 on X11: no WAYLAND_DISPLAY, so Chrome's own ozone auto-detection
# picks X11. The no-flag path is correct there too.
if [ -n "${SWAYSOCK:-}" ]; then
	exec google-chrome-stable --ozone-platform=x11 "$@"
fi

exec google-chrome-stable "$@"
```

- [ ] **Step 2: Create the `.desktop` override**

Covering only the keybinding would leave rofi launches, `.desktop` activations and link handlers from other applications on the old path — producing a confusing mix of XWayland and native Wayland Chrome windows in one session.

Generate it from the packaged file rather than hand-writing it, so `MimeType`, actions and localised names are preserved:

```bash
mkdir -p ~/dotfiles/scripts/.local/share/applications
sed 's|^Exec=/usr/bin/google-chrome-stable|Exec=chrome-session-launch|' \
	/usr/share/applications/google-chrome.desktop \
	> ~/dotfiles/scripts/.local/share/applications/google-chrome.desktop
```

- [ ] **Step 3: Verify every `Exec=` line was rewritten**

```bash
grep '^Exec=' ~/dotfiles/scripts/.local/share/applications/google-chrome.desktop
```

Expected: every line starts `Exec=chrome-session-launch`. The packaged file has several (`[Desktop Entry]` plus `[Desktop Action ...]` groups). If any still reads `/usr/bin/google-chrome-stable`, the `sed` pattern did not match that line's exact form — inspect `/usr/share/applications/google-chrome.desktop` and fix by hand.

- [ ] **Step 4: Strip the platform flag from `chrome-flags.conf`**

This file is machine-local and not stowed. Edit `~/.config/chrome-flags.conf` to remove the `--ozone-platform=x11` line, leaving the explanatory comment rewritten to point at the wrapper:

```
# Ozone platform is no longer set here — it is chosen per session by
# ~/.local/bin/chrome-session-launch, because this file cannot express a
# conditional (the launcher reads it with plain `grep -v '^#'`).
```

- [ ] **Step 5: Repoint Sway's `$mod+c` at the wrapper**

So there is exactly one place the platform decision lives. In `~/dotfiles/sway/.config/sway/config`, replace line 192 and the comment above it:

```
# Chrome's ozone platform is picked per session by the wrapper — see
# ~/.local/bin/chrome-session-launch. Under Sway that means XWayland, which
# works around the wl_shm abort on wlroots 0.20.2.
bindsym $mod+c exec chrome-session-launch
```

- [ ] **Step 6: Static-check the wrapper**

```bash
chmod +x ~/dotfiles/scripts/.local/bin/chrome-session-launch
sh -n ~/dotfiles/scripts/.local/bin/chrome-session-launch
shellcheck ~/dotfiles/scripts/.local/bin/chrome-session-launch
shfmt -d ~/dotfiles/scripts/.local/bin/chrome-session-launch
```

Expected: all silent / no diff.

- [ ] **Step 7: Verify it still works in the current Sway session**

```bash
cd ~/dotfiles && stow -R scripts sway
swaymsg reload
SWAYSOCK="$SWAYSOCK" chrome-session-launch --version
```

Expected: `Google Chrome <version>` prints without aborting. Then press `$mod+c` and confirm a Chrome window opens and does **not** crash. Confirm it is still on XWayland under Sway:

```bash
swaymsg -t get_tree | jq -r '.. | objects | select(.app_id == null and .window_properties.class == "Google-chrome") | .name' | head -1
```

Expected: a window title prints (a non-null `window_properties.class` means XWayland, which is correct here).

- [ ] **Step 8: Commit in `~/dotfiles` (do not push)**

```bash
cd ~/dotfiles
git add scripts/.local/bin/chrome-session-launch \
	scripts/.local/share/applications/google-chrome.desktop \
	sway/.config/sway/config
git commit -m "Pick Chrome's ozone platform per session via a wrapper"
```

---

### Task 3: `hypr` stow package — base config, look, autostart, wallpaper

**Files:**
- Create: `~/dotfiles/hypr/.config/hypr/hyprland.conf`
- Create: `~/dotfiles/hypr/.config/hypr/hyprpaper.conf`
- Create: `~/dotfiles/hypr/.config/hypr/scripts/wg_toggle.sh`
- Copy: `~/dotfiles/hypr/.config/hypr/lockscreen.png` (from the sway package)

**Interfaces:**
- Consumes: the hy3 `.so` path recorded in Task 1 Step 8.
- Produces: a `hyprland.conf` with `$mod` defined, `layout = hy3`, and the plugin loaded. Task 4 appends the keybinding section to this same file. Task 5 supplies `hypridle`/`hyprlock` configs referenced by the `exec-once` lines written here.

- [ ] **Step 1: Create the package directories and copy the shared assets**

`wg_toggle.sh` is duplicated rather than shared because the two `.config` trees are independent stow packages. It is 17 lines and compositor-agnostic.

```bash
mkdir -p ~/dotfiles/hypr/.config/hypr/scripts
cp ~/dotfiles/sway/.config/sway/lockscreen.png ~/dotfiles/hypr/.config/hypr/
cp ~/dotfiles/sway/.config/sway/scripts/wg_toggle.sh ~/dotfiles/hypr/.config/hypr/scripts/
chmod +x ~/dotfiles/hypr/.config/hypr/scripts/wg_toggle.sh
```

`toggle_keyboard_layout.sh` is deliberately **not** copied — Hyprland declares both layouts in `input {}` and cycles them with `hyprctl switchxkblayout`, making the script unnecessary.

- [ ] **Step 2: Write `hyprland.conf` (base sections only)**

Substitute the real plugin path from Task 1 Step 8 if it differs from `/usr/lib/libhy3.so`.

```
# Hyprland config — ported from ../../../sway/.config/sway/config.
#
# Window behavior and keybindings mirror the Sway session; aesthetics
# deliberately do not (rounding, blur, shadows and animations are on here).
# Sway remains installed as the fallback session.
#
# Spec: env-install/docs/superpowers/specs/2026-08-01-hyprland-setup-design.md

# hy3 restores i3/Sway-style manual tree tiling, which neither of Hyprland's
# native layouts (dwindle, master) provides. It is compiled against an exact
# Hyprland version and refuses to load against any other — after a hyprland
# upgrade, check `hyprctl plugin list` and reinstall hyprland-plugin-hy3 if
# it is missing. Verify this path with `pacman -Ql hyprland-plugin-hy3`.
plugin = /usr/lib/libhy3.so

$mod = SUPER

general {
    layout = hy3

    # Sway's `gaps inner 10` is the TOTAL gap between two adjacent windows.
    # Hyprland applies gaps_in to EACH window edge, so it doubles — 5 here
    # renders the same as 10 there. `gaps outer 2` maps 1:1.
    gaps_in = 5
    gaps_out = 2

    # Sway: `default_border pixel 4` + client.focused/client.unfocused.
    border_size = 4
    col.active_border = rgb(a6d189)
    col.inactive_border = rgb(232634)
}

# Aesthetics diverge from Sway deliberately — this is the one area where
# matching the old session was NOT the goal.
decoration {
    rounding = 10

    blur {
        enabled = true
        size = 6
        passes = 3
        new_optimizations = true
    }

    shadow {
        enabled = true
        range = 20
        render_power = 3
        color = rgba(1a1a1aee)
    }
}

animations {
    enabled = true
    bezier = easeOutQuint, 0.23, 1, 0.32, 1
    animation = windows, 1, 4, easeOutQuint, popin 80%
    animation = windowsOut, 1, 4, easeOutQuint, popin 80%
    animation = border, 1, 8, default
    animation = fade, 1, 4, default
    animation = workspaces, 1, 4, easeOutQuint, slide
}

input {
    # Replaces sway/scripts/toggle_keyboard_layout.sh entirely: both layouts
    # are declared once here and $mod+i cycles them via
    # `hyprctl switchxkblayout`. The Sway script had to parse
    # `swaymsg -t get_inputs` and substring-match a human-readable layout
    # name, which it carried a VERIFY comment about.
    kb_layout = us,se
    follow_mouse = 1
}

misc {
    disable_hyprland_logo = true
    disable_splash_rendering = true
}

plugin {
    hy3 {
        tabs {
            height = 5
            padding = 8
            render_text = true

            # hy3 nests tab colors in their own block. There is no
            # `col.active`-style key here — that is Hyprland core's
            # convention, not hy3's. Verified against the plugin's own
            # registered keys (plugin:hy3:tabs:colors:*).
            colors {
                active = rgb(a6d189)
                inactive = rgb(232634)
                urgent = rgb(e78284)
            }
        }

        autotile {
            # Off: Sway did not autotile, and the goal is matching behavior.
            enable = false
        }
    }
}

# Sway needed two rules here — `class` criteria only match XWayland windows,
# so a separate `app_id` rule was required for the native Wayland case.
# Hyprland's `class` matches both, so one regex covers it.
#
# Hyprland 0.56.1 grammar: `windowrule` is a special category keyed by `name`,
# with action fields at the top level and criteria in a nested `match` block.
# `windowrulev2` is deprecated, and a bare rename to `windowrule = float,
# class:...` also fails ("invalid field float: missing a value") — the grammar
# changed, not just the keyword.
windowrule {
    name = calculator
    float = true
    match {
        class = ^(org\.gnome\.Calculator|[Gg]nome-calculator)$
    }
}

# Autostart. Sway's `exec` / `exec --no-startup-id` lines become exec-once.
# mako is shared with the Sway session (compositor-agnostic, and
# wg_toggle.sh needs notify-send); hyprpolkitagent replaces polkit-gnome,
# which stays installed for Sway.
exec-once = hyprpaper
exec-once = waybar
exec-once = hypridle
exec-once = nm-applet
exec-once = mako
exec-once = systemctl --user start hyprpolkitagent
```

- [ ] **Step 3: Write `hyprpaper.conf`**

```
# Same image Sway used via `output * bg ... fill`. It ships with the `sway`
# package, which stays installed as the fallback session — so this is a real,
# if minor, dependency of the Hyprland session on the Sway package. Noted so
# it is not mistaken for an oversight later.
preload = /usr/share/backgrounds/sway/Sway_Wallpaper_Blue_1920x1080.png

# Empty monitor field = all outputs, matching Sway's `output *`.
wallpaper = ,/usr/share/backgrounds/sway/Sway_Wallpaper_Blue_1920x1080.png

splash = false
```

- [ ] **Step 4: Confirm the wallpaper file actually exists**

```bash
ls -l /usr/share/backgrounds/sway/Sway_Wallpaper_Blue_1920x1080.png
```

Expected: the file lists. If it does not, the `sway` package layout changed — find the replacement with `pacman -Ql sway | grep -i background` and update both lines in `hyprpaper.conf`.

- [ ] **Step 5: Commit in `~/dotfiles` (do not push)**

```bash
cd ~/dotfiles
git add hypr/
git commit -m "Add hypr stow package: base config, look, autostart, wallpaper"
```

---

### Task 4: Keybindings and submaps

**Files:**
- Modify: `~/dotfiles/hypr/.config/hypr/hyprland.conf` (append the keybinding sections)
- Create: `~/dotfiles/hypr/.config/hypr/scripts/confirm-exit.sh`

**Interfaces:**
- Consumes: `$mod` and `layout = hy3` from Task 3; `chrome-session-launch` from Task 2; `~/.config/hypr/exit.sh` from Task 5 (bound here, created there).
- Produces: the complete binding set. Nothing later consumes it.

- [ ] **Step 1: Verify the hy3 dispatcher names against the installed plugin**

This is the highest-risk part of the whole port — the names below come from upstream documentation, not from a running system. Check them before writing:

```bash
pacman -Ql hyprland-plugin-hy3 | grep -iE 'readme|doc'
hyprctl dispatch -j 2>&1 | head -20
```

If a README is packaged, read its dispatcher list. If not, fetch it:

```bash
curl -s https://raw.githubusercontent.com/outfoxxed/hy3/master/README.md | sed -n '/dispatcher/,/^##/p'
```

Confirm these eight exist and take the argument forms used below: `hy3:movefocus`, `hy3:movewindow`, `hy3:makegroup`, `hy3:changegroup`, `hy3:changefocus`, `hy3:killactive`, `hy3:movetoworkspace`, `hy3:setephemeral`. **Correct the bindings below to match reality before proceeding** — do not write bindings you have not confirmed.

- [ ] **Step 2: Write `confirm-exit.sh`**

`swaynag` is Sway-specific and has no Hyprland equivalent. Rofi is already installed and themed, and preserves the guard rather than binding a bare `exit`.

```sh
#!/bin/sh
# Replaces Sway's `swaynag -t warning -m '... really want to exit sway?'`.
choice=$(printf 'No\nYes, exit Hyprland\n' |
	rofi -dmenu -i -p 'Exit Hyprland?' -selected-row 0)

if [ "$choice" = "Yes, exit Hyprland" ]; then
	hyprctl dispatch exit
fi

exit 0
```

```bash
chmod +x ~/dotfiles/hypr/.config/hypr/scripts/confirm-exit.sh
sh -n ~/dotfiles/hypr/.config/hypr/scripts/confirm-exit.sh
shellcheck ~/dotfiles/hypr/.config/hypr/scripts/confirm-exit.sh
```

Expected: silent.

- [ ] **Step 3: Append the launcher, window and focus bindings to `hyprland.conf`**

```
# ---------------------------------------------------------------------------
# Keybindings — a 1:1 port of the Sway config unless commented otherwise.
# ---------------------------------------------------------------------------

# --- launchers ---
bind = $mod, Return, exec, wezterm
bind = $mod, C, exec, chrome-session-launch
bind = $mod, Space, exec, rofi -show drun
bind = $mod, D, exec, rofi -show run
bind = $mod, W, exec, kitty -e sudo $HOME/.config/hypr/scripts/wg_toggle.sh

# --- window management ---
bind = $mod, Q, hy3:killactive
bind = $mod, F, fullscreen, 0
bind = $mod SHIFT, Space, togglefloating
bind = $mod, A, hy3:changefocus, raise

# REGRESSION: Sway had `layout stacking` here. hy3 implements tabbed groups
# only — i3's stacked layout has no counterpart. Tabs are the closest
# equivalent.
bind = $mod, S, hy3:changegroup, tab

bind = $mod, E, hy3:changegroup, opposite
bind = $mod SHIFT, V, hy3:makegroup, h, ephemeral
bind = $mod, V, hy3:makegroup, v, ephemeral

# REGRESSION: Sway had `focus mode_toggle`, which jumps focus between the
# tiling and floating stacks. No Hyprland dispatcher does that. This cycles
# floating windows instead — the closest available behavior, not equivalent.
bind = $mod SHIFT, T, cyclenext, floating

# --- focus ---
bind = $mod, H, hy3:movefocus, l
bind = $mod, J, hy3:movefocus, d
bind = $mod, K, hy3:movefocus, u
bind = $mod, L, hy3:movefocus, r

bind = $mod, Left, hy3:movefocus, l
bind = $mod, Down, hy3:movefocus, d
bind = $mod, Up, hy3:movefocus, u
bind = $mod, Right, hy3:movefocus, r

# --- move ---
bind = $mod SHIFT, H, hy3:movewindow, l
bind = $mod SHIFT, J, hy3:movewindow, d
bind = $mod SHIFT, K, hy3:movewindow, u
bind = $mod SHIFT, L, hy3:movewindow, r

bind = $mod SHIFT, Left, hy3:movewindow, l
bind = $mod SHIFT, Down, hy3:movewindow, d
bind = $mod SHIFT, Up, hy3:movewindow, u
bind = $mod SHIFT, Right, hy3:movewindow, r

# Sway's `floating_modifier $mod`: 272 = left button, 273 = right button.
bindm = $mod, mouse:272, movewindow
bindm = $mod, mouse:273, resizewindow
```

- [ ] **Step 4: Append the workspace bindings**

`hy3:movetoworkspace` is used rather than the bare `movetoworkspace` because it preserves group structure when moving a window out of a tabbed or split group.

```
# --- workspaces ---
bind = $mod, 1, workspace, 1
bind = $mod, 2, workspace, 2
bind = $mod, 3, workspace, 3
bind = $mod, 4, workspace, 4
bind = $mod, 5, workspace, 5
bind = $mod, 6, workspace, 6
bind = $mod, 7, workspace, 7
bind = $mod, 8, workspace, 8
bind = $mod, 9, workspace, 9
bind = $mod, 0, workspace, 10

bind = $mod SHIFT, 1, hy3:movetoworkspace, 1
bind = $mod SHIFT, 2, hy3:movetoworkspace, 2
bind = $mod SHIFT, 3, hy3:movetoworkspace, 3
bind = $mod SHIFT, 4, hy3:movetoworkspace, 4
bind = $mod SHIFT, 5, hy3:movetoworkspace, 5
bind = $mod SHIFT, 6, hy3:movetoworkspace, 6
bind = $mod SHIFT, 7, hy3:movetoworkspace, 7
bind = $mod SHIFT, 8, hy3:movetoworkspace, 8
bind = $mod SHIFT, 9, hy3:movetoworkspace, 9
bind = $mod SHIFT, 0, hy3:movetoworkspace, 10

bind = $mod CTRL, H, movecurrentworkspacetomonitor, l
bind = $mod CTRL, L, movecurrentworkspacetomonitor, r
```

- [ ] **Step 5: Append the session and media bindings**

```
# --- session ---
bind = $mod SHIFT, C, exec, hyprctl reload
bind = $mod SHIFT, E, exec, $HOME/.config/hypr/scripts/confirm-exit.sh

# Replaces sway/scripts/toggle_keyboard_layout.sh — both layouts are declared
# in input {} above.
bind = $mod, I, exec, hyprctl switchxkblayout all next

# --- media and brightness ---
# IMPROVEMENT over Sway: bindl fires while the session is locked, and bindel
# additionally repeats when the key is held. Sway's plain bindsym did neither.
bindel = , XF86AudioRaiseVolume, exec, pactl set-sink-volume @DEFAULT_SINK@ +5%
bindel = , XF86AudioLowerVolume, exec, pactl set-sink-volume @DEFAULT_SINK@ -5%
bindl = , XF86AudioMute, exec, pactl set-sink-mute @DEFAULT_SINK@ toggle
bindl = , XF86AudioMicMute, exec, pactl set-source-mute @DEFAULT_SOURCE@ toggle
bindl = , XF86AudioPlay, exec, dbus-send --print-reply --dest=org.mpris.MediaPlayer2.spotify /org/mpris/MediaPlayer2 org.mpris.MediaPlayer2.Player.PlayPause
bindl = , XF86AudioNext, exec, dbus-send --print-reply --dest=org.mpris.MediaPlayer2.spotify /org/mpris/MediaPlayer2 org.mpris.MediaPlayer2.Player.Next
bindl = , XF86AudioPrev, exec, dbus-send --print-reply --dest=org.mpris.MediaPlayer2.spotify /org/mpris/MediaPlayer2 org.mpris.MediaPlayer2.Player.Previous

bindel = , XF86MonBrightnessUp, exec, brightnessctl set 20%+
bindel = , XF86MonBrightnessDown, exec, brightnessctl set 20%-
```

- [ ] **Step 6: Append the submaps**

Submaps must come last in the file — every `bind` after a `submap =` line belongs to that submap until the next `submap = reset`.

Hyprland fires **all** binds matching a key, which is how each system-mode key both acts and leaves the submap in two lines.

```
# ---------------------------------------------------------------------------
# Submaps — Sway's `mode` blocks. These MUST stay at the end of the file:
# every bind after a `submap =` line belongs to that submap.
# ---------------------------------------------------------------------------

bind = $mod, R, submap, resize
submap = resize

# Sway's resize semantics are preserved exactly, including the asymmetry:
# h grows width, l shrinks width, j shrinks height, k grows height.
# binde repeats while held.
binde = , H, resizeactive, 10 0
binde = , L, resizeactive, -10 0
binde = , J, resizeactive, 0 -10
binde = , K, resizeactive, 0 10

bind = , Return, submap, reset
bind = , Escape, submap, reset
bind = $mod, R, submap, reset

submap = reset

# Sway: mode "$system" — (l) lock, (e) logout, (s) suspend, (h) hibernate,
# (r) reboot, (Shift+s) shutdown. Hyprland has no mode-name display, so the
# prompt text is dropped; the keys are unchanged.
bind = $mod SHIFT, Q, submap, system
submap = system

bind = , L, exec, $HOME/.config/hypr/exit.sh lock
bind = , L, submap, reset
bind = , E, exec, $HOME/.config/hypr/exit.sh logout
bind = , E, submap, reset
bind = , S, exec, $HOME/.config/hypr/exit.sh suspend
bind = , S, submap, reset
bind = , H, exec, $HOME/.config/hypr/exit.sh hibernate
bind = , H, submap, reset
bind = , R, exec, $HOME/.config/hypr/exit.sh reboot
bind = , R, submap, reset
bind = SHIFT, S, exec, $HOME/.config/hypr/exit.sh shutdown
bind = SHIFT, S, submap, reset

bind = , Return, submap, reset
bind = , Escape, submap, reset

submap = reset
```

- [ ] **Step 7: Check every Sway binding is accounted for**

```bash
grep -c '^bindsym' ~/dotfiles/sway/.config/sway/config
grep -cE '^(bind|binde|bindl|bindel|bindm) =' ~/dotfiles/hypr/.config/hypr/hyprland.conf
```

Expected: the Hyprland count is **higher** than Sway's, because each system-submap key takes two lines and `bindm` adds two mouse bindings Sway expressed as a single `floating_modifier`. Then diff the key lists by hand:

```bash
grep -oE '^bindsym [^ ]+' ~/dotfiles/sway/.config/sway/config | sort -u
```

Walk that list and confirm each key appears in `hyprland.conf`. Any missing key is a port bug.

- [ ] **Step 8: Commit in `~/dotfiles` (do not push)**

```bash
cd ~/dotfiles
git add hypr/
git commit -m "Port Sway keybindings and modes to Hyprland submaps"
```

---

### Task 5: Lock, idle and the exit script

**Files:**
- Create: `~/dotfiles/hypr/.config/hypr/hyprlock.conf`
- Create: `~/dotfiles/hypr/.config/hypr/hypridle.conf`
- Create: `~/dotfiles/hypr/.config/hypr/exit.sh`

**Interfaces:**
- Consumes: `lockscreen.png` copied in Task 3; the `exec-once = hypridle` line written in Task 3; the system-submap bindings from Task 4 that call `exit.sh`.
- Produces: `exit.sh` accepting `lock|logout|suspend|hibernate|reboot|shutdown`, same interface as the Sway version.

- [ ] **Step 1: Write `exit.sh`**

A copy of `~/dotfiles/sway/.config/sway/exit.sh` with `swaylock` → `hyprlock` and `swaymsg exit` → `hyprctl dispatch exit`. The `systemctl` paths are unchanged.

```sh
#!/bin/sh
lock() {
	hyprlock
}

case "$1" in
lock)
	lock
	;;
logout)
	hyprctl dispatch exit
	;;
suspend)
	lock && systemctl suspend
	;;
hibernate)
	lock && systemctl hibernate
	;;
reboot)
	systemctl reboot
	;;
shutdown)
	systemctl poweroff
	;;
*)
	echo "Usage: $0 {lock|logout|suspend|hibernate|reboot|shutdown}"
	exit 2
	;;
esac

exit 0
```

- [ ] **Step 2: Write `hyprlock.conf`**

```
# Reuses the same lockscreen.png the Sway session used.
background {
    monitor =
    path = $HOME/.config/hypr/lockscreen.png
    blur_passes = 0
}

input-field {
    monitor =
    size = 300, 50
    outline_thickness = 2
    dots_center = true
    outer_color = rgb(a6d189)
    inner_color = rgb(232634)
    font_color = rgb(babbf1)
    fail_color = rgb(e78284)
    placeholder_text = <i>Password…</i>
    fade_on_empty = false
}
```

- [ ] **Step 3: Write `hypridle.conf`**

```
general {
    lock_cmd = pidof hyprlock || hyprlock
    before_sleep_cmd = loginctl lock-session
    after_sleep_cmd = hyprctl dispatch dpms on
}

# Sway blanked the outputs 60s after the last input, but ONLY while locked —
# the `pgrep -x swaylock` guard inside swayidle's timeout action is what made
# it "1 min into the lockscreen" rather than a general 1 min screen blank, so
# an unlocked session left sitting there kept its display. Guarding on the
# lock process (rather than hooking the lock path) means every way in gets
# it: the $mod+Shift+q system submap and the before-sleep lock alike.
listener {
    timeout = 60
    on-timeout = pgrep -x hyprlock > /dev/null && hyprctl dispatch dpms off
    on-resume = hyprctl dispatch dpms on
}
```

- [ ] **Step 4: Note the `$HOME` expansion risk in `hyprlock.conf`**

Hyprland's config parser uses `$name` for its own variables (`$mod` in
`hyprland.conf`), and its handling of environment variables differs by
context. If Task 7 Step 8 shows hyprlock with a blank or black background
instead of the lockscreen image, that is this — replace `$HOME/.config/...`
with the literal absolute path `/home/toga/.config/hypr/lockscreen.png` and
re-test. Do not pre-emptively hardcode it; the `$HOME` form is preferred if
it works.

- [ ] **Step 5: Static-check `exit.sh`**

```bash
chmod +x ~/dotfiles/hypr/.config/hypr/exit.sh
sh -n ~/dotfiles/hypr/.config/hypr/exit.sh
shellcheck ~/dotfiles/hypr/.config/hypr/exit.sh
shfmt -d ~/dotfiles/hypr/.config/hypr/exit.sh
```

Expected: all silent / no diff.

- [ ] **Step 6: Commit in `~/dotfiles` (do not push)**

```bash
cd ~/dotfiles
git add hypr/
git commit -m "Add hyprlock, hypridle and the Hyprland exit script"
```

---

### Task 6: Waybar

**Files:**
- Create: `~/dotfiles/waybar/.config/waybar/config.jsonc`
- Create: `~/dotfiles/waybar/.config/waybar/style.css`

**Interfaces:**
- Consumes: the `exec-once = waybar` line from Task 3; the three surviving scripts in the existing `i3blocks` stow package (`coderkeeb.sh`, `mx_master.sh`, `wireguard.sh`), which stay the single source of truth for those blocks across both sessions.
- Produces: the bar. Nothing later consumes it.

- [ ] **Step 1: Write `config.jsonc`**

Sway's `bar {}` block did the workspace indicator itself; `hyprland/workspaces` is what replaces it, and without it the bar has no workspace display at all.

```jsonc
// Replaces Sway's `bar { status_command i3blocks }`. Nothing in the Hyprland
// ecosystem consumes the i3bar JSON protocol that i3blocks emits, so the
// blocks are re-expressed as Waybar modules — native wherever one exists.
{
	"layer": "top",
	"position": "top",
	"height": 34,
	"spacing": 12,

	"modules-left": ["hyprland/workspaces"],
	"modules-center": ["hyprland/window"],
	"modules-right": [
		"mpris",
		"custom/mx_master",
		"custom/coderkeeb",
		"custom/wireguard",
		"cpu",
		"hyprland/language",
		"battery",
		"clock",
		"tray"
	],

	"hyprland/workspaces": {
		"format": "{id}",
		"on-click": "activate"
	},

	"hyprland/window": {
		"max-length": 80,
		"separate-outputs": true
	},

	// Replaces spotify.sh, which shelled out to qdbus and grepped
	// `pactl list sink-inputs` to infer play/pause state.
	"mpris": {
		"player": "spotify",
		"format": "{status_icon} {title} - {artist}",
		"status-icons": {
			"playing": "",
			"paused": ""
		},
		"max-length": 60
	},

	// Replaces the [cpu] block's `mpstat 1 1 | grep -A 5 | tail | awk`
	// pipeline, which spawned four processes every second.
	"cpu": {
		"format": " {usage}%",
		"interval": 1
	},

	// Replaces lang.sh, which parsed `swaymsg -t get_inputs` with jq and
	// substring-matched a human-readable layout name.
	"hyprland/language": {
		"format": " {}",
		"format-en": "US",
		"format-sv": "SE"
	},

	// Replaces battery.sh, which read /sys/class/power_supply/BAT0/capacity —
	// a path that does not exist on this desktop, so it had been failing
	// silently every 10s. This module hides itself when there is no battery.
	"battery": {
		"format": " {capacity}%",
		"format-charging": " {capacity}%",
		"states": {
			"warning": 25,
			"critical": 10
		}
	},

	"clock": {
		"format": " {:%d %b %H:%M}",
		"interval": 5,
		"tooltip-format": "<tt>{calendar}</tt>"
	},

	"tray": {
		"spacing": 10
	},

	// No native equivalent — these three keep running the existing scripts
	// from the i3blocks stow package, at their current 10s intervals.
	"custom/mx_master": {
		"exec": "$HOME/.config/i3blocks/mx_master.sh",
		"interval": 10
	},

	"custom/coderkeeb": {
		"exec": "$HOME/.config/i3blocks/coderkeeb.sh",
		"interval": 10
	},

	"custom/wireguard": {
		"exec": "$HOME/.config/i3blocks/wireguard.sh",
		"interval": 10
	}
}
```

- [ ] **Step 2: Write `style.css`**

Translucent and rounded to match the "full Hyprland look" decision, rather than Sway's flat black bar. Palette values are the same ones the Sway config used.

```css
/* Catppuccin Frappe, matching the palette in the Sway config. */
@define-color bg          rgba(35, 38, 52, 0.75);
@define-color fg          #babbf1;
@define-color accent      #a6d189;
@define-color highlight   #8caaee;
@define-color urgent      #e78284;
@define-color active_bg   #51576d;

* {
	font-family: "Hack Nerd Font", monospace;
	font-size: 14px;
	border: none;
	border-radius: 0;
	min-height: 0;
}

window#waybar {
	background: transparent;
	color: @fg;
}

.modules-left,
.modules-center,
.modules-right {
	background: @bg;
	border-radius: 12px;
	margin: 6px 8px;
	padding: 0 10px;
}

#workspaces button {
	color: @fg;
	padding: 0 8px;
	border-radius: 8px;
}

#workspaces button.active {
	background: @highlight;
	color: #ffffff;
}

#workspaces button.urgent {
	background: @urgent;
	color: #ffffff;
}

#window {
	color: @fg;
	padding: 0 8px;
}

#mpris,
#custom-mx_master,
#custom-coderkeeb,
#custom-wireguard,
#cpu,
#language,
#battery,
#clock,
#tray {
	padding: 0 10px;
	color: @fg;
}

#battery.warning {
	color: #ff9000;
}

#battery.critical {
	color: @urgent;
}
```

- [ ] **Step 3: Verify the JSON parses**

`config.jsonc` allows `//` comments, which `jq` rejects — strip them first:

```bash
sed 's://.*::' ~/dotfiles/waybar/.config/waybar/config.jsonc | jq empty
```

Expected: no output. Any parse error prints a line number — fix and re-run.

- [ ] **Step 4: Confirm the three referenced scripts exist and are executable**

```bash
ls -l ~/.config/i3blocks/mx_master.sh ~/.config/i3blocks/coderkeeb.sh ~/.config/i3blocks/wireguard.sh
```

Expected: all three list with an executable bit. They are already stowed from the `i3blocks` package.

- [ ] **Step 5: Commit in `~/dotfiles` (do not push)**

```bash
cd ~/dotfiles
git add waybar/
git commit -m "Add waybar stow package replacing Sway's i3blocks bar"
```

---

### Task 7: Stow, first login and on-hardware verification

**Files:**
- Modify: none — this task runs and checks what the previous tasks built.

**Interfaces:**
- Consumes: everything from Tasks 1–6.

Most of this task is **[USER RUNS]** — it requires a Hyprland session, which cannot be entered from inside the current Sway session.

- [ ] **Step 1: Stow the new packages**

```bash
cd ~/dotfiles && stow -R hypr waybar scripts
ls -l ~/.config/hypr/hyprland.conf ~/.config/waybar/config.jsonc ~/.local/bin/chrome-session-launch
```

Expected: three symlinks into `~/dotfiles`.

- [ ] **Step 2: [USER RUNS] Log out and select the Hyprland session**

The `hyprland.desktop` session file is installed by the package, but the greeter's remembered session must be changed by hand once. Log out fully rather than reloading — `XDG_CURRENT_DESKTOP` and portal registration are set at session start.

- [ ] **Step 3: Check for config parse errors first**

Before testing anything else. This is the fastest way to catch a typo in a 200-line config:

```bash
hyprctl configerrors
```

Expected: `no errors`. Anything else must be fixed before continuing.

- [ ] **Step 4: Confirm hy3 actually loaded**

```bash
hyprctl plugin list
```

Expected: `hy3` appears. If the list is empty, the `plugin =` path in `hyprland.conf` is wrong — recheck `pacman -Ql hyprland-plugin-hy3 | grep -F '.so'`. If it reports a version mismatch, `hyprland` was upgraded past the plugin: log into Sway and rebuild.

- [ ] **Step 5: Walk the full keybinding list by hand**

Open two terminals and work through every binding from Task 4, comparing against the Sway session's behavior. Pay particular attention to the hy3 ones, which are the least certain: `$mod+v` / `$mod+Shift+v` splits, `$mod+s` tabs, `$mod+e` toggle, `$mod+a` focus parent, `$mod+Shift+<N>` moving a window out of a tabbed group with its structure intact.

Record any binding that misbehaves — Task 8 writes the deltas into the spec.

- [ ] **Step 6: Verify Chrome is a native Wayland client**

This is the point of the Chrome work — confirm it, do not assume it:

```bash
hyprctl clients -j | jq -r '.[] | select(.class | test("chrome"; "i")) | "\(.class) xwayland=\(.xwayland)"'
```

Expected: `xwayland=false`. If Chrome aborts on startup instead, the aquamarine reasoning was wrong — add `--ozone-platform=x11` to the wrapper's fallback branch and record it in the spec's "Revert trigger" section.

- [ ] **Step 7: Verify the bar**

Expected: ten modules visible — workspaces, window title, mpris, mx_master, coderkeeb, wireguard, cpu, language, clock, tray. The eleventh, `battery`, is **expected to be absent**: this box has no battery and self-hiding is the module behaving correctly. Confirm the tray is populated by `nm-applet`, and that `$mod+i` flips the language module between US and SE.

If waybar is missing entirely, run it in the foreground to see why:

```bash
waybar -l debug
```

- [ ] **Step 8: Verify lock and idle**

Three separate behaviors:

1. `$mod+Shift+q` then `l` → hyprlock appears with the lockscreen image; the password unlocks it.
2. Locked and left idle 60s → outputs blank; input wakes them.
3. **Unlocked** and left idle 60s → outputs stay on. This is the `pgrep` guard working; if the screen blanks here, the guard is broken.

- [ ] **Step 9: [USER RUNS] Verify suspend and resume**

Called out separately because this machine is S3-only (`s2idle` hard-locks it via amdgpu) and wakes via a Bluetooth dongle, and because hypridle has taken over the sleep-inhibitor hook that `swayidle -w` previously held.

`$mod+Shift+q` then `s`. Expected: locks, then suspends; Bluetooth keyboard/mouse input wakes it; the session comes back locked with the display on. Any deviation is a real regression, not a cosmetic one.

- [ ] **Step 10: Verify the Sway fallback still works**

The whole reason Sway was retained. Log out, select the Sway session, and confirm it still starts, the bar renders, and `$mod+c` opens Chrome without crashing (it should be on XWayland there).

---

### Task 8: Documentation

**Files:**
- Modify: `CLAUDE.md`
- Modify: `docs/TODO.md`
- Modify: `docs/superpowers/specs/2026-08-01-hyprland-setup-design.md` (append an amendments section)

- [ ] **Step 1: Record on-hardware deltas in the spec**

Following the precedent set by the sway spec, append a section rather than rewriting the body — the sway spec's "Amendments" section is the model:

```markdown
## Amendments (2026-08-01): on-hardware findings

Written before Hyprland had ever run on this machine. This section records
where reality differed:

- [one bullet per delta found in Task 7 — corrected hy3 dispatcher names,
  the real plugin .so path, whether native Wayland Chrome held up, any
  binding that did not port cleanly]
```

If Task 7 found no deltas, write that explicitly rather than omitting the section.

- [ ] **Step 2: Update `CLAUDE.md`**

Two edits. In the "What this is" paragraph, change `window manager (i3 + i3blocks/i3lock/picom/rofi/feh)` to note the Arch split:

```
window manager (Debian: i3 + i3blocks/i3lock/picom/rofi/feh; Arch: Hyprland
+ hy3/Waybar as the primary session with Sway kept as a fallback)
```

In the "Architecture" section's step-ordering list, add `hyprland` to the sequence, and add a bullet:

```
- `hyprland.inc` is the repo's **only AUR dependency**. It bootstraps `yay`
  (guarded on `command -v yay`) and installs `hyprland-plugin-hy3`, which is
  compiled against an exact Hyprland version and refuses to load against any
  other. The repo-available Hyprland packages come from the main pacman list;
  this file exists only for the AUR part. Sway is deliberately still
  installed and stowed on Arch as the fallback session for when a `hyprland`
  upgrade outruns the plugin.
```

- [ ] **Step 3: Add the recurring maintenance item to `docs/TODO.md`**

Under a new `## Hyprland` heading:

```markdown
## Hyprland
- [ ] After every `hyprland` upgrade, check `hyprctl plugin list` and reinstall `hyprland-plugin-hy3` if hy3 is missing. This recurs indefinitely — it is not a one-time migration cost.
- [ ] Test native-Wayland `wezterm` under Hyprland. `$mod+Return` still launches it with `config.enable_wayland = false` (set 2026-07-26 for the wlroots `wl_shm` abort). Hyprland uses aquamarine, not wlroots, so the flag is likely unnecessary there — but `wezterm.lua` is shared with the Debian/i3 path, so it cannot simply be flipped.
- [ ] The 2026-07-25 spec's amendments record a `wezterm` → `kitty` switch for `$mod+Return` that never landed in the Sway config. Decide which is intended and make both sessions agree.
```

- [ ] **Step 4: Commit**

```bash
cd ~/Downloads/temp/env-install
git add CLAUDE.md docs/TODO.md docs/superpowers/specs/2026-08-01-hyprland-setup-design.md
git commit -m "Document the Hyprland session and its hy3 maintenance burden"
```

- [ ] **Step 5: Report the `~/dotfiles` push decision to the user**

`~/dotfiles` now has four unpushed commits on `master` (Tasks 2, 3, 4, 5, 6). Per the global constraints these are **not** pushed. Tell the user they are staged locally and ask whether to push.

---

## Manual steps summary

Consolidated answer to the plan's originating question, "what do I need to do manually?":

1. **Install the packages** — `sudo pacman -S ...` and `yay -S hyprland-plugin-hy3` (Task 1 Step 7). The agent cannot run these.
2. **Select the Hyprland session at the greeter**, and log out fully rather than reloading (Task 7 Step 2).
3. **Walk the keybinding list by hand** against the Sway session (Task 7 Step 5) — the hy3 dispatchers are the least-certain part of the port.
4. **Test suspend/resume** (Task 7 Step 9) — S3-only box, Bluetooth wake, and hypridle has taken over the sleep inhibitor.
5. **Confirm the Sway fallback still boots** (Task 7 Step 10).
6. **After every `hyprland` upgrade, run `hyprctl plugin list`.** If hy3 is gone, log into Sway and reinstall it. This is permanent, not a migration cost.
7. **Decide whether to push `~/dotfiles`** (Task 8 Step 5).
