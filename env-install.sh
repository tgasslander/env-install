#!/bin/bash

echo "apt upate"
sudo apt update

echo "Installing dev env"
echo

echo "Installing kitty"
curl -L https://sw.kovidgoyal.net/kitty/installer.sh | sh /dev/stdin

echo "Hack font"
echo
./fonts.inc

echo "Powerlevel10k theme"
echo
source powerlevel10k.inc

echo "i3 sources"
echo
source i3.inc

sudo apt install -y \
	zsh \
	tmux \
	stow \
	feh \
	curl \
	clang \
	htop \
	i3-wm \
	i3blocks \
	i3lock \
	vim \
	dmenu

echo "zsh"
echo
if [ ! -d "${SZH}" ]; then
	source zsh.inc
else
	echo "ZSH already installed"
fi

git clone https://github.com/tgasslander/dotfiles.git ~/dotfiles
cd ~/dotfiles
stow i3 i3blocks nvim zsh p10k Xresources tmux kitty
cd -

echo "oh-my-zsh"
echo
if [ ! -d "${HOME}/.oh-my-zsh" ]; then
	rm -rf ~/.oh-my-zsh
	source "oh-my-zsh.inc"
else
	echo "oh-my-zsh already installed"
fi

echo "nvim"
echo
source nvim.inc


