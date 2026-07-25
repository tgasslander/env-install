#!/bin/bash

# Shared package-manager abstraction, sourced by env-install.sh.
# Sets PKG_MGR and defines pkg_update / pkg_install for the detected distro.

if command -v apt >/dev/null 2>&1; then
	PKG_MGR=apt
elif command -v pacman >/dev/null 2>&1; then
	PKG_MGR=pacman
else
	echo "Unsupported system: neither apt nor pacman found" >&2
	exit 1
fi
export PKG_MGR

echo "Detected package manager: ${PKG_MGR}"

pkg_update() {
	case "$PKG_MGR" in
		apt)    sudo apt update ;;
		pacman) sudo pacman -Syu --noconfirm ;;
	esac
}

pkg_install() {
	case "$PKG_MGR" in
		apt)    sudo apt install -y "$@" ;;
		pacman) sudo pacman -S --needed --noconfirm "$@" ;;
	esac
}
