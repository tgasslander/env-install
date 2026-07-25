# Sway as the i3 replacement on Arch Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace i3+X11+picom with Sway on Arch-based distros (env-install.sh's pacman path), translating the user's existing i3 config/keybindings/scripts to Sway/Wayland equivalents, while leaving the Debian/apt path (i3+X11+picom) untouched.

**Architecture:** Two repos change. (1) `env-install.sh` in this repo: the pacman package list swaps `i3-wm`/`i3lock`/`picom`/`xorg-server` for `sway`/`swaylock`/`swayidle`/`swaybg`/`brightnessctl`/`wlr-randr`/`jq`, and the `stow` line branches by `$PKG_MGR`. (2) `~/dotfiles` (separate repo, `tgasslander/dotfiles`): a new `sway` stow package is added mirroring the existing `i3` package's structure, with every X11-specific tool invocation (picom, xss-lock+i3lock, feh, xbacklight, setxkbmap, xrandr, xset) replaced by its Wayland/wlroots equivalent (swayidle+swaylock, native `output ... bg`, brightnessctl, swaymsg input, wlr-randr). One file, `i3blocks/.config/i3blocks/lang.sh`, is shared between the i3 and Sway setups (i3blocks itself is reused as the status-bar command under both) and gets a runtime `$SWAYSOCK` check added rather than being replaced outright, so the Debian/i3 path keeps working unmodified.

**Tech Stack:** Bash, Sway/wlroots (`sway`, `swaymsg`, `swaylock`, `swayidle`, `swaybg`, `wlr-randr`, `brightnessctl`), `jq`, GNU Stow, pacman.

## Global Constraints

- Debian/apt install path must not change at all (per spec: "Out of scope: Any change to the Debian/apt install path").
- No automated test suite exists for either repo; validation is `bash -n` (and `shellcheck` where available) for scripts, per this repo's `CLAUDE.md`, plus `sway --validate` for the Sway config and a manual on-hardware checklist for anything hardware-dependent (per the spec's Validation Plan).
- Do not push `~/dotfiles` commits to its remote without separate confirmation (per spec decision).
- Multi-monitor output names (`DP-1.1`, `HDMI-0`, `DP-3`, `DP-1.2`) used in the bar/workspace config are best-effort translations flagged with `VERIFY` comments — do not treat them as done until checked against `swaymsg -t get_outputs` on real hardware (per spec decision).
- **Scope change (mid-execution):** the DPMS/monitor-kick workaround scripts (`dpms-hdmi-fix.sh`, `kick_hdmi.sh`, `thunderbolt.sh`, `toga_screens_office.sh`) and their keybindings are dropped entirely, not ported — they're one-off hacks for the flaky dock/HDMI setup on this specific machine and aren't expected to be needed elsewhere. This superseded the original spec's "best-effort translate now" decision for these four scripts; the original Task 6 that ported them is cancelled.

---

## File Structure

**This repo (`env-install`):**
- Modify: `env-install.sh` — pacman package list, conditional `stow` line

**`~/dotfiles` (separate git repo):**
- Modify: `i3blocks/.config/i3blocks/lang.sh` — add `$SWAYSOCK` branch
- Create: `sway/.config/sway/config` — core Sway config, ported from `i3/.config/i3/config`
- Create: `sway/.config/sway/exit.sh` — ported from `i3/.config/i3/i3exit.sh`
- Create: `sway/.config/sway/lockscreen.png` — copy of `i3/.config/i3/lockscreen.png`
- Create: `sway/.config/sway/scripts/toggle_keyboard_layout.sh` — ported from `i3/.config/i3/scripts/toggle_keyboard_layout.sh`
- Create: `sway/.config/sway/scripts/wg_toggle.sh` — unchanged copy of `i3/.config/i3/scripts/wg_toggle.sh` (no X11 dependency)

The `i3/`, `picom/` stow packages in `~/dotfiles` are left in place untouched (still needed for the Debian path) — only unstowed on this Arch machine in Task 8.

---

### Task 1: Fix `env-install.sh` package list and stow line

**Files:**
- Modify: `env-install.sh:24-30` (pacman package list), `env-install.sh:70` (stow line)

**Interfaces:**
- Produces: the pacman branch installs `sway swaylock swayidle swaybg brightnessctl wlr-randr jq` instead of `i3-wm i3lock picom xorg-server`; the stow line becomes conditional on `$PKG_MGR`.

- [ ] **Step 1: Revert the earlier `xorg-server` addition and rewrite the pacman package list**

Replace the current pacman branch (lines 24-30):
```bash
else
	# Debian's i3 package pulls in xserver-xorg via Recommends; Arch's i3-wm has
	# no dependency on xorg-server at all, so it must be listed explicitly here
	# or i3 shows up as a login-greeter option with no X server to run it.
	pkg_install \
		zsh tmux stow feh curl clang htop \
		i3-wm i3blocks i3lock vim \
		xorg-server \
		base-devel python \
		picom wezterm wget rofi unzip
fi
```
with:
```bash
else
	# Sway replaces i3+X11+picom on Arch: it's Wayland-native (no xorg-server
	# needed at all) and deliberately i3-config-compatible. See
	# docs/superpowers/specs/2026-07-25-sway-i3-replacement-design.md.
	pkg_install \
		zsh tmux stow curl clang htop \
		sway swaylock swayidle swaybg brightnessctl wlr-randr jq \
		i3blocks vim \
		base-devel python \
		wezterm wget rofi unzip
fi
```
(`feh` and `picom` are dropped — `swaybg` and Sway's native compositing replace them; `xorg-server` is dropped per the design decision to go Sway-only, not fallback.)

- [ ] **Step 2: Make the `stow` line conditional on `$PKG_MGR`**

Replace the single `stow -R` line:
```bash
stow -R i3 i3blocks nvim starship Xresources tmux kitty picom rofi wezterm
```
with:
```bash
if [ "$PKG_MGR" = "apt" ]; then
	stow -R i3 i3blocks nvim starship Xresources tmux kitty picom rofi wezterm
else
	stow -R sway i3blocks nvim starship Xresources tmux kitty rofi wezterm
fi
```

- [ ] **Step 3: Verify syntax**

Run: `bash -n env-install.sh`
Expected: no output, exit code 0.

- [ ] **Step 4: Commit**

```bash
git add env-install.sh
git commit -m "$(cat <<'EOF'
Replace i3+picom+xorg-server with Sway on the Arch install path

Sway is Wayland-native and i3-config-compatible, so it avoids the
Xorg dependency gap that caused the black-screen-on-login bug, and
keeps the same keybindings/workspace behavior. Debian path unchanged.
EOF
)"
```

---

### Task 2: Install the Sway toolchain on this machine

This installs the packages Task 1 added to the script, directly on this machine, so later tasks can validate the config with real binaries (`sway --validate`, `swaymsg`, `wlr-randr`).

**Files:** none (system package install only)

- [ ] **Step 1: Install packages**

```bash
sudo pacman -S --needed sway swaylock swayidle swaybg brightnessctl wlr-randr jq
```

- [ ] **Step 2: Verify each binary is present**

```bash
for bin in sway swaymsg swaylock swayidle swaybg brightnessctl wlr-randr jq; do
	command -v "$bin" >/dev/null && echo "OK: $bin" || echo "MISSING: $bin"
done
```
Expected: `OK: <name>` for all seven.

---

### Task 3: Add Wayland/X11 detection to the shared `lang.sh`

`i3blocks` is reused as the status-bar command by both the i3 (X11) and Sway configs, so `lang.sh` — which currently shells out to `setxkbmap` — must branch at runtime instead of being replaced outright, or the Debian/i3 path breaks.

**Files:**
- Modify: `~/dotfiles/i3blocks/.config/i3blocks/lang.sh` (full rewrite of the file, same responsibility)

**Interfaces:**
- Produces: on Sway, reads the active layout via `swaymsg -t get_inputs | jq`; on i3/X11, keeps using `setxkbmap -print` exactly as before. Detection is `[ -n "$SWAYSOCK" ]`, the same convention Task 7's `toggle_keyboard_layout.sh` relies on implicitly (that script only ever runs under Sway, so it doesn't need the branch itself).

- [ ] **Step 1: Replace the file contents**

```bash
#!/bin/bash

# IMPORTANT!
# Make sure to edit this file with an editor
# that can display font awesome

ICON=

if [ -n "$SWAYSOCK" ]; then
	# Running under Sway (Wayland) — swaymsg replaces setxkbmap.
	# VERIFY: xkb_active_layout_name is a human-readable string (e.g.
	# "English (US)" / "Swedish"), not a raw layout code. Check
	# `swaymsg -t get_inputs | jq` on this hardware if this stops matching.
	CURR_NAME=$(swaymsg -t get_inputs | jq -r '[.[] | select(.type == "keyboard")][0].xkb_active_layout_name')
	case "$CURR_NAME" in
		*Swedish*) echo "$ICON SE" ;;
		*English*) echo "$ICON US" ;;
	esac
else
	# Running under i3 (X11) — unchanged from the original script.
	CURR_LANG="$(setxkbmap -print | grep xkb_symbols | awk '{print $4}' | awk -F"+" '{print $2}')"
	case "$CURR_LANG" in
		se) echo "$ICON SE" ;;
		us) echo "$ICON US" ;;
	esac
fi

exit 0
```

- [ ] **Step 2: Verify syntax**

Run: `bash -n ~/dotfiles/i3blocks/.config/i3blocks/lang.sh`
Expected: no output, exit code 0.

- [ ] **Step 3: Verify the i3/X11 branch still behaves identically**

Run (simulating no Sway socket): `env -u SWAYSOCK bash -c 'setxkbmap -print | grep xkb_symbols' `
Confirm this still produces output on the current system (it's the same command the `else` branch runs), so the unchanged branch has real input to work with.

- [ ] **Step 4: Commit (in the dotfiles repo)**

```bash
cd ~/dotfiles
git add i3blocks/.config/i3blocks/lang.sh
git commit -m "$(cat <<'EOF'
Add Sway/Wayland branch to lang.sh, keep i3/X11 branch unchanged

i3blocks is reused as-is under Sway, so this file is shared between
the i3 and sway stow packages. Detect via $SWAYSOCK rather than
replacing setxkbmap outright, so the Debian/i3 path keeps working.
EOF
)"
cd -
```

---

### Task 4: Write `sway/.config/sway/config`

The core Sway config, ported from `~/dotfiles/i3/.config/i3/config`. Same gaps/mod/keybindings/workspace/resize-mode/colors/bar structure (Sway is config-syntax-compatible with i3 for all of that); X11-specific `exec` lines and bindings are replaced per the spec's translation table.

**Files:**
- Create: `~/dotfiles/sway/.config/sway/config`

**Interfaces:**
- Consumes: `~/.config/sway/exit.sh` (Task 5), `~/.config/sway/scripts/{toggle_keyboard_layout,wg_toggle}.sh` (Task 7), `~/.config/sway/lockscreen.png` (Task 5) — this task references all of them by path but they don't need to exist yet for this task's own verification step.
- Produces: the `$mod` keybindings, workspace numbers `$ws1`-`$ws10`, and `mode "$system"` / `mode "resize"` names, unchanged from the i3 config, for consistency with muscle memory.

- [ ] **Step 1: Create the file**

```bash
mkdir -p ~/dotfiles/sway/.config/sway/scripts
```

Write `~/dotfiles/sway/.config/sway/config`:

```
# Sway config — ported from ../i3/.config/i3/config (the i3/X11 version).
# i3 is X11-only; this replaces the X11-specific bits (picom, xss-lock+i3lock,
# feh, xbacklight, setxkbmap, xrandr) with their Sway/Wayland equivalents.
# See docs/superpowers/specs/2026-07-25-sway-i3-replacement-design.md in the
# env-install repo for the full rationale.
#
# VERIFY: output names below (DP-1.1, HDMI-0, DP-3, DP-1.2) are carried over
# from the X11 RandR names in the old i3 config. Wlroots may enumerate them
# differently — run `swaymsg -t get_outputs` after first login and fix any
# output name below that doesn't match.

gaps inner 12
gaps outer 2

set $mod Mod4

font pango:monospace 8

# swayidle+swaylock replaces xss-lock+i3lock. The original config had no
# idle-timeout lock at all — both its xss-lock lines used
# --transfer-sleep-lock (suspend-only); the second one also chained a
# kick_hdmi.sh HDMI-wake hack, which is dropped here (machine-specific,
# not ported — see Global Constraints).
exec swayidle -w \
	before-sleep 'swaylock -f -i ~/.config/sway/lockscreen.png'

# NetworkManager tray icon. Sway's bar has native tray support (unlike
# i3bar), see `tray_output` in the bar {} blocks below.
exec --no-startup-id nm-applet

set $refresh_i3status killall -SIGUSR1 i3status
bindsym XF86AudioRaiseVolume exec --no-startup-id pactl set-sink-volume @DEFAULT_SINK@ +5% && $refresh_i3status
bindsym XF86AudioLowerVolume exec --no-startup-id pactl set-sink-volume @DEFAULT_SINK@ -5% && $refresh_i3status
bindsym XF86AudioMute exec --no-startup-id pactl set-sink-mute @DEFAULT_SINK@ toggle && $refresh_i3status
bindsym XF86AudioMicMute exec --no-startup-id pactl set-source-mute @DEFAULT_SOURCE@ toggle && $refresh_i3status
bindsym XF86AudioPlay exec --no-startup-id "dbus-send --print-reply --dest=org.mpris.MediaPlayer2.spotify /org/mpris/MediaPlayer2 org.mpris.MediaPlayer2.Player.PlayPause"
bindsym XF86AudioNext exec --no-startup-id "dbus-send --print-reply --dest=org.mpris.MediaPlayer2.spotify /org/mpris/MediaPlayer2 org.mpris.MediaPlayer2.Player.Next"
bindsym XF86AudioPrev exec --no-startup-id "dbus-send --print-reply --dest=org.mpris.MediaPlayer2.spotify /org/mpris/MediaPlayer2 org.mpris.MediaPlayer2.Player.Previous"

floating_modifier $mod

bindsym $mod+Return exec wezterm

bindsym $mod+q kill

bindsym $mod+space exec --no-startup-id rofi -show drun
bindsym $mod+d exec --no-startup-id rofi -show run

bindsym $mod+h focus left
bindsym $mod+j focus down
bindsym $mod+k focus up
bindsym $mod+l focus right

bindsym $mod+Left focus left
bindsym $mod+Down focus down
bindsym $mod+Up focus up
bindsym $mod+Right focus right

bindsym $mod+Shift+h move left
bindsym $mod+Shift+j move down
bindsym $mod+Shift+k move up
bindsym $mod+Shift+l move right

bindsym $mod+Shift+Left move left
bindsym $mod+Shift+Down move down
bindsym $mod+Shift+Up move up
bindsym $mod+Shift+Right move right

bindsym $mod+Shift+v split h
bindsym $mod+v split v

bindsym $mod+f fullscreen toggle

bindsym $mod+s layout stacking
bindsym $mod+e layout toggle split

bindsym $mod+Shift+space floating toggle

bindsym $mod+Shift+t focus mode_toggle

bindsym $mod+a focus parent

bindsym $mod+w exec i3-sensible-terminal -e "sudo ${HOME}/.config/sway/scripts/wg_toggle.sh"

set $ws1 "1"
set $ws2 "2"
set $ws3 "3"
set $ws4 "4"
set $ws5 "5"
set $ws6 "6"
set $ws7 "7"
set $ws8 "8"
set $ws9 "9"
set $ws10 "10"

bindsym $mod+1 workspace number $ws1
bindsym $mod+2 workspace number $ws2
bindsym $mod+3 workspace number $ws3
bindsym $mod+4 workspace number $ws4
bindsym $mod+5 workspace number $ws5
bindsym $mod+6 workspace number $ws6
bindsym $mod+7 workspace number $ws7
bindsym $mod+8 workspace number $ws8
bindsym $mod+9 workspace number $ws9
bindsym $mod+0 workspace number $ws10

bindsym $mod+Shift+1 move container to workspace number $ws1
bindsym $mod+Shift+2 move container to workspace number $ws2
bindsym $mod+Shift+3 move container to workspace number $ws3
bindsym $mod+Shift+4 move container to workspace number $ws4
bindsym $mod+Shift+5 move container to workspace number $ws5
bindsym $mod+Shift+6 move container to workspace number $ws6
bindsym $mod+Shift+7 move container to workspace number $ws7
bindsym $mod+Shift+8 move container to workspace number $ws8
bindsym $mod+Shift+9 move container to workspace number $ws9
bindsym $mod+Shift+0 move container to workspace number $ws10

bindsym $mod+Control+h move workspace to output left
bindsym $mod+Control+l move workspace to output right

bindsym $mod+Shift+c reload
bindsym $mod+Shift+r restart
bindsym $mod+Shift+e exec swaynag -t warning -m 'You pressed the exit shortcut. Do you really want to exit sway? This will end your Wayland session.' -B 'Yes, exit sway' 'swaymsg exit'

bindsym XF86MonBrightnessUp exec brightnessctl set 20%+
bindsym XF86MonBrightnessDown exec brightnessctl set 20%-

mode "resize" {
    bindsym h resize grow width 10 px or 10 ppt
    bindsym j resize shrink height 10 px or 10 ppt
    bindsym k resize grow height 10 px or 10 ppt
    bindsym l resize shrink width 10 px or 10 ppt

    bindsym Return mode "default"
    bindsym Escape mode "default"
    bindsym $mod+r mode "default"
}

bindsym $mod+r mode "resize"

set $color_background #000000
set $color_highlight #8caaee
set $color_highlight_foreground #ffffff
set $color_non_focused_foreground #babbf1
set $color_non_focused_background #232634
set $color_urgent_foreground #e78284
set $color_active_background #51576d

for_window [class="Gnome-calculator"] floating enable

bar {
	output DP-1.1
	output HDMI-0
	output primary
	font pango:DejaVu 16
	tray_output primary
	colors {
		background $color_background
		separator #666666
		focused_workspace $color_background $color_highlight $color_highlight_foreground
		active_workspace $color_active_background $color_active_background $color_highlight_foreground
		inactive_workspace $color_background $color_non_focused_background $color_non_focused_foreground
		urgent_workspace $color_background $color_urgent_foreground #ffffff
	}
	status_command i3blocks
	position top
}

bar {
	output DP-3
	font pango:DejaVu 24
	tray_output DP-3
	colors {
		background $color_background
		separator #666666
		focused_workspace $color_background $color_highlight $color_highlight_foreground
		active_workspace $color_active_background $color_active_background $color_highlight_foreground
		inactive_workspace $color_background $color_non_focused_background $color_non_focused_foreground
		urgent_workspace $color_background $color_urgent_foreground #ffffff
	}
	status_command i3blocks
	position top
}

# toga - workspaces in multimonitor mode
workspace 1 output DP-3
workspace 2 output primary
workspace 3 output primary

# screw thunderbolt for this setup
exec_always swaymsg output DP-1.1 disable
exec_always swaymsg output DP-1.2 disable

# Standard Sway default wallpaper (shipped by the sway package itself),
# used on all outputs — avoids depending on user-specific image files that
# only existed on a previous machine.
output * bg /usr/share/backgrounds/sway/Sway_Wallpaper_Blue_1920x1080.png fill

bindsym $mod+c exec google-chrome
default_border pixel 0

set $system system (l) lock, (e) logout, (s) suspend, (h) hibernate, (r) reboot, (Shift+s) shutdown
mode "$system" {
    bindsym l exec --no-startup-id ${HOME}/.config/sway/exit.sh lock, mode "default"
    bindsym e exec --no-startup-id ${HOME}/.config/sway/exit.sh logout, mode "default"
    bindsym h exec --no-startup-id ${HOME}/.config/sway/exit.sh hibernate, mode "default"
    bindsym s exec --no-startup-id ${HOME}/.config/sway/exit.sh suspend, mode "default"
    bindsym r exec --no-startup-id ${HOME}/.config/sway/exit.sh reboot, mode "default"
    bindsym Shift+s exec --no-startup-id ${HOME}/.config/sway/exit.sh shutdown, mode "default"

    bindsym Return mode "default"
    bindsym Escape mode "default"
}
bindsym $mod+Shift+q mode "$system"

bindsym $mod+i exec ~/.config/sway/scripts/toggle_keyboard_layout.sh
```

- [ ] **Step 2: Verify config syntax** (once Task 2's packages are installed)

Run: `sway -c ~/dotfiles/sway/.config/sway/config -C`
Expected: exits 0 with no "Error on line N" output. If it fails because no Wayland/DRM session is reachable from this shell at all (rather than a config syntax error), fall back to a manual line-by-line read-through against the i3 original and note that automated validation had to be deferred to Task 9's on-hardware check.

- [ ] **Step 3: Commit**

```bash
cd ~/dotfiles
git add sway/.config/sway/config
git commit -m "$(cat <<'EOF'
Add sway config, ported from the i3 config

Same keybindings/workspaces/gaps/bar as i3; X11-specific bits
(picom, xss-lock+i3lock, feh, xbacklight, xrandr) replaced with
swayidle+swaylock, native `output ... bg`, brightnessctl, and
swaymsg respectively. Output names are flagged VERIFY pending
on-hardware testing. The old dpms-hdmi-fix/kick_hdmi/thunderbolt/
toga_screens_office hacks and their keybindings are dropped, not
ported — machine-specific, not needed elsewhere.
EOF
)"
cd -
```

---

### Task 5: Write `exit.sh` and copy `lockscreen.png`

**Files:**
- Create: `~/dotfiles/sway/.config/sway/exit.sh`
- Create: `~/dotfiles/sway/.config/sway/lockscreen.png` (binary copy)

**Interfaces:**
- Consumes: nothing
- Produces: `exit.sh {lock|logout|suspend|hibernate|reboot|shutdown}`, the same CLI contract as `i3exit.sh`, used by Task 4's `mode "$system"` bindings.

- [ ] **Step 1: Write `exit.sh`**

```bash
#!/bin/sh
lock() {
    swaylock -f -i ${HOME}/.config/sway/lockscreen.png
}

case "$1" in
    lock)
        lock
        ;;
    logout)
        swaymsg exit
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
esac

exit 0
```

- [ ] **Step 2: Make it executable and copy the lockscreen image**

```bash
chmod +x ~/dotfiles/sway/.config/sway/exit.sh
cp ~/dotfiles/i3/.config/i3/lockscreen.png ~/dotfiles/sway/.config/sway/lockscreen.png
```

- [ ] **Step 3: Verify**

Run: `bash -n ~/dotfiles/sway/.config/sway/exit.sh && ls -la ~/dotfiles/sway/.config/sway/lockscreen.png`
Expected: no syntax errors, and the file listing shows the copied PNG (same size as `i3/.config/i3/lockscreen.png`, ~2.9M).

- [ ] **Step 4: Commit**

```bash
cd ~/dotfiles
git add sway/.config/sway/exit.sh sway/.config/sway/lockscreen.png
git commit -m "Add sway exit.sh (swaymsg/swaylock port of i3exit.sh) and lockscreen image"
cd -
```

---

### Task 6: CANCELLED — monitor-hack scripts dropped, not ported

**Decision (mid-execution scope change):** `dpms-hdmi-fix.sh`, `kick_hdmi.sh`, `thunderbolt.sh`, and `toga_screens_office.sh` are one-off workarounds for the flaky HDMI/dock setup on this specific machine, and aren't expected to be needed on any other machine this dotfiles repo gets stowed on. Rather than porting them to `wlr-randr` (superseding the original spec's "best-effort translate now" decision for these four scripts), they're dropped entirely from the `sway` package, along with their keybindings (`$mod+m`, `$mod+shift+m`, `$mod+n`) and the `kick_hdmi.sh` chain in `swayidle`'s `before-sleep` hook — see the updated Task 4 content above, which no longer references any of these.

They remain untouched in `~/dotfiles/i3/.config/i3/` and `i3/.config/i3/scripts/` for the Debian/i3 path, which is unaffected by this change.

No files are created by this task. If an earlier dispatch already created any of `sway/.config/sway/dpms-hdmi-fix.sh`, `sway/.config/sway/scripts/kick_hdmi.sh`, `sway/.config/sway/scripts/thunderbolt.sh`, `sway/.config/sway/scripts/toga_screens_office.sh` before this cancellation, remove them and any commit that added them.

---

### Task 7: Write `toggle_keyboard_layout.sh` and copy `wg_toggle.sh`

**Files:**
- Create: `~/dotfiles/sway/.config/sway/scripts/toggle_keyboard_layout.sh`
- Create: `~/dotfiles/sway/.config/sway/scripts/wg_toggle.sh` (unchanged copy)

**Interfaces:**
- Consumes: nothing
- Produces: `~/.config/sway/scripts/toggle_keyboard_layout.sh` (bound to `$mod+i` in Task 4), `~/.config/sway/scripts/wg_toggle.sh` (invoked by Task 4's `$mod+w` binding)

- [ ] **Step 1: Write `scripts/toggle_keyboard_layout.sh`**

```bash
#!/bin/bash
# Uses `swaymsg input type:keyboard xkb_layout` instead of setxkbmap.
# VERIFY: xkb_active_layout_name is a human-readable string (e.g.
# "English (US)" / "Swedish"), not a raw layout code — the substring match
# below assumes those names; check `swaymsg -t get_inputs | jq` on this
# hardware if toggling doesn't work as expected.
CURR_LAYOUT=$(swaymsg -t get_inputs | jq -r '[.[] | select(.type == "keyboard")][0].xkb_active_layout_name')

if [[ "$CURR_LAYOUT" == *"Swedish"* ]]; then
    swaymsg input type:keyboard xkb_layout us
else
    swaymsg input type:keyboard xkb_layout se
fi
```

- [ ] **Step 2: Copy `wg_toggle.sh` unchanged** (no X11 dependency — `ip`, `wg-quick`, `dhclient`, `notify-send` are all DE-agnostic)

```bash
cp ~/dotfiles/i3/.config/i3/scripts/wg_toggle.sh ~/dotfiles/sway/.config/sway/scripts/wg_toggle.sh
```

- [ ] **Step 3: Make executable and verify syntax**

```bash
chmod +x ~/dotfiles/sway/.config/sway/scripts/toggle_keyboard_layout.sh \
	~/dotfiles/sway/.config/sway/scripts/wg_toggle.sh

bash -n ~/dotfiles/sway/.config/sway/scripts/toggle_keyboard_layout.sh && echo OK
bash -n ~/dotfiles/sway/.config/sway/scripts/wg_toggle.sh && echo OK
```
Expected: `OK` printed twice.

- [ ] **Step 4: Commit**

```bash
cd ~/dotfiles
git add sway/.config/sway/scripts/toggle_keyboard_layout.sh sway/.config/sway/scripts/wg_toggle.sh
git commit -m "Add sway keyboard-layout toggle (swaymsg-based) and carry over wg_toggle.sh unchanged"
cd -
```

---

### Task 8: Stow the new package on this machine and validate

**Files:** none (stow symlink operations + validation)

- [ ] **Step 1: Unstow the old i3/picom packages, stow the new sway package**

```bash
cd ~/dotfiles
stow -D i3 picom
stow -R sway i3blocks nvim starship Xresources tmux kitty rofi wezterm
cd -
```

- [ ] **Step 2: Confirm the symlinks landed correctly**

```bash
ls -la ~/.config/sway/config ~/.config/sway/exit.sh \
	~/.config/sway/lockscreen.png ~/.config/sway/scripts/
```
Expected: every path is a symlink (`l...` in the `ls -la` listing) pointing into `~/dotfiles/sway/...`, and `~/.config/i3` / `~/.config/picom.conf` are gone (no longer symlinked).

- [ ] **Step 3: Validate the Sway config**

Run: `sway -c ~/.config/sway/config -C`
Expected: exit 0, no "Error on line N" output. If this can't run cleanly outside a real Sway session, note it and defer to Task 9's live login test.

- [ ] **Step 4: Confirm the sway session file exists in the greeter**

```bash
ls /usr/share/wayland-sessions/sway.desktop
```
Expected: file exists (shipped by the `sway` package itself — no work needed to create it).

---

### Task 9: Manual on-hardware validation

This task is inherently manual — the multi-monitor output names can only be confirmed by actually logging into Sway on this hardware, per the spec's Validation Plan. Do this task interactively with the user, not unattended.

- [ ] **Step 1: Log into the Sway session** from the login greeter (`plasmalogin`), selecting "Sway".

- [ ] **Step 2: Check real output names**

Run inside the Sway session: `swaymsg -t get_outputs | jq '.[] | {name, active}'`
Compare the `name` values against `DP-1.1`, `HDMI-0`, `DP-3`, `DP-1.2` used in `sway/.config/sway/config`'s bar/workspace/output blocks. Fix every mismatch.

- [ ] **Step 3: Confirm core behavior**
  - Keybindings: focus/move/split/resize/workspace switches match old i3 muscle memory.
  - Bar: `i3blocks` renders in the Sway bar, both `bar {}` blocks show on the right outputs, NetworkManager tray icon appears (`tray_output`).
  - Lock/idle: `swayidle` locks the screen before sleep; `$mod+Shift+q` → `l` locks immediately via `exit.sh`.
  - Brightness keys adjust brightness via `brightnessctl`.
  - `$mod+space` / `$mod+d` open rofi (via XWayland) with the existing catppuccin theme.
  - `$mod+i` toggles keyboard layout; the `lang` block in the bar updates.
  - Sway's standard default wallpaper appears on all outputs (the config was switched away from machine-specific image paths — see the wallpaper-fix commit after Task 8).

- [ ] **Step 4: Commit any on-hardware fixes**

```bash
cd ~/dotfiles
git add -A
git commit -m "Fix sway output names/positions based on real hardware (swaymsg -t get_outputs)"
cd -
```

- [ ] **Step 5: Report back** — summarize what worked out of the box vs. what needed adjustment, so the env-install repo's spec doc can be updated if any Sway/wlroots behavior differed from what was assumed during design.
