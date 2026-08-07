#!/bin/bash

# The Hyprland setup's AUR dependencies. Arch/pacman only — no-op on Debian,
# which keeps i3 + X11.
#
# Executed, not sourced (like k8s.inc/docker.inc/fonts.inc): nothing here
# needs to leak into the parent shell. That also means common.inc's
# pkg_install/pkg_update are NOT in scope — only the exported PKG_MGR — so
# pacman and yay are called directly.
#
# The repo-available Hyprland packages are installed from env-install.sh's
# main pacman list. This file exists solely for packages only available on
# the AUR: hy3 (compiled against Hyprland) and overskride (no `extra`/
# `cachyos-extra` build).

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

echo "Installing overskride (Bluetooth client GUI, AUR-only bin package)"
yay -S --needed --noconfirm overskride-bin
