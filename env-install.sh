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
		picom wezterm snapd wget rofi unzip
else
	pkg_install \
		zsh tmux stow feh curl clang htop \
		i3-wm i3blocks i3lock vim \
		base-devel python \
		picom wezterm wget rofi unzip
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

echo "Hack font"
echo
./fonts.inc


echo "i3 sources"
echo
source i3.inc

echo "nvm"
PROFILE=/dev/null bash -c 'curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.40.0/install.sh | bash'

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
stow -R i3 i3blocks nvim starship Xresources tmux kitty picom rofi wezterm
cd - || exit
echo "stowed configs from dotfiles"

echo "zsh"
echo
if ! command -v zsh >/dev/null 2>&1; then
	source zsh.inc
else
	echo "ZSH already installed"
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

# shellcheck disable=SC1090
source ~/.zshrc

echo "node.js"
NVM_DIR="$HOME/.nvm"
[ -s "$NVM_DIR/nvm.sh" ] && \. "$NVM_DIR/nvm.sh"  # This loads nvm
nvm install node

if [ "$PKG_MGR" = "apt" ]; then
	sudo snap install --classic go
else
	pkg_install go
fi

echo "Done"
