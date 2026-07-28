#!/bin/bash

source common.inc

# Third-party WezTerm repo is Debian-only; Arch ships wezterm in its repos.
if [ "$PKG_MGR" = "apt" ]; then
	echo "Adding the WezTerm repository"
	curl -fsSL https://apt.fury.io/wez/gpg.key | sudo gpg --yes --dearmor -o /usr/share/keyrings/wezterm-fury.gpg
	echo 'deb [signed-by=/usr/share/keyrings/wezterm-fury.gpg] https://apt.fury.io/wez/ * *' | sudo tee /etc/apt/sources.list.d/wezterm.list
fi

echo "Updating package database"
pkg_update

echo "Installing dev env"
echo

if [ "$PKG_MGR" = "apt" ]; then
	pkg_install \
		zsh tmux stow feh curl clang htop \
		i3 i3blocks i3lock vim \
		build-essential python3-venv \
		shellcheck shfmt \
		picom wezterm snapd wget rofi unzip \
		network-manager-gnome sysstat \
		postgresql-client xclip
else
	# Sway replaces i3+X11+picom on Arch: it's Wayland-native (no xorg-server
	# needed at all) and deliberately i3-config-compatible. See
	# docs/superpowers/specs/2026-07-25-sway-i3-replacement-design.md.
	pkg_install \
		zsh tmux stow curl clang htop \
		sway swaylock swayidle swaybg brightnessctl wlr-randr jq \
		i3blocks vim \
		base-devel python \
		shellcheck shfmt \
		wget rofi unzip \
		network-manager-applet sysstat mako polkit-gnome \
		kubectl k9s kubectx postgresql-libs \
		docker docker-compose docker-buildx \
		wl-clipboard
fi

echo "Installing kitty"
if [ "$PKG_MGR" = "apt" ]; then
	curl -L https://sw.kovidgoyal.net/kitty/installer.sh | sh /dev/stdin \
		launch=n
	mkdir -p ~/.local/bin
	sudo ln -sf ~/.local/kitty.app/bin/kitty ~/.local/kitty.app/bin/kitten /usr/bin/
else
	pkg_install kitty
fi

echo "Kubernetes tooling"
echo
# Executed, not sourced: nothing here needs to leak into the parent shell.
# No-op on Arch, where these are native packages installed above.
./k8s.inc

echo "Docker"
echo
# Executed, not sourced: nothing here needs to leak into the parent shell.
# On Arch the packages come from the list above; this still does the group
# and daemon setup on both distros.
./docker.inc

echo "Hack font"
echo
./fonts.inc

echo "i3 sources"
echo
source i3.inc

echo "nvm"
PROFILE=/dev/null bash -c 'curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.40.0/install.sh | bash'

echo "node.js"
NVM_DIR="$HOME/.nvm"
[ -s "$NVM_DIR/nvm.sh" ] && \. "$NVM_DIR/nvm.sh" # This loads nvm
nvm install node

echo "dotfiles"
if [ -d "${HOME}/dotfiles/.git" ]; then
	echo "dotfiles already cloned, pulling latest"
	git -C ~/dotfiles pull --ff-only
else
	# not a git repo (fresh box or leftover dir): start clean
	rm -rf ~/dotfiles
	git clone https://github.com/tgasslander/dotfiles.git ~/dotfiles
fi
cd ~/dotfiles || exit
# -R restows: idempotent, re-links without erroring on existing symlinks
if [ "$PKG_MGR" = "apt" ]; then
	stow -R i3 i3blocks nvim starship Xresources tmux kitty picom rofi wezterm scripts
else
	stow -R sway i3blocks nvim starship Xresources tmux kitty rofi scripts
fi
cd - || exit
echo "stowed configs from dotfiles"

echo "zsh"
echo
if [ "$(getent passwd "$USER" | cut -d: -f7)" != "$(command -v zsh)" ]; then
	source zsh.inc
else
	echo "zsh is already the default shell"
fi

echo "nvim"
echo
source nvim.inc

echo "oh-my-zsh"
echo
if [ -d "${HOME}/.oh-my-zsh" ]; then
	echo "oh-my-zsh already installed, skipping"
else
	source "oh-my-zsh.inc"
fi

echo "zsh-autosuggestions and zsh-syntax-highlighting plugins"
ZSH_CUSTOM="${ZSH_CUSTOM:-$HOME/.oh-my-zsh/custom}"
[ -d "${ZSH_CUSTOM}/plugins/zsh-autosuggestions" ] ||
	git clone https://github.com/zsh-users/zsh-autosuggestions "${ZSH_CUSTOM}/plugins/zsh-autosuggestions"
[ -d "${ZSH_CUSTOM}/plugins/zsh-syntax-highlighting" ] ||
	git clone https://github.com/zsh-users/zsh-syntax-highlighting "${ZSH_CUSTOM}/plugins/zsh-syntax-highlighting"

# Back up a real ~/.zshrc once (e.g. the one oh-my-zsh just wrote), never
# clobbering an existing backup. On reruns ~/.zshrc is already a stow symlink,
# so this is skipped and stow -R just re-links.
if [ -f ~/.zshrc ] && [ ! -L ~/.zshrc ]; then
	backup=~/.zshrc.bak
	[ -e "$backup" ] && backup=~/.zshrc.bak.$(date +%s)
	echo "Backing up existing ~/.zshrc to $backup"
	mv ~/.zshrc "$backup"
fi
cd ~/dotfiles || exit
stow -R zsh
cd - || exit

echo "Starship prompt"
echo
source starship.inc

echo "~/.zshrc is zsh-specific and can't be sourced from this bash script; open a new zsh shell (or restart your terminal) to pick it up"

if [ "$PKG_MGR" = "apt" ]; then
	sudo snap install --classic go
else
	pkg_install go
fi

echo "Done"
