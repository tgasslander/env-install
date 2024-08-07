#!/bin/bash

echo "apt upate"
sudo apt update

echo "Installing dev env"
echo

echo "Installing kitty"
curl -L https://sw.kovidgoyal.net/kitty/installer.sh | sh /dev/stdin \
    launch=n
mkdir -p ~/.local/bin
ln -sf ~/.local/kitty.app/bin/kitty ~/.local/kitty.app/bin/kitten ~/.local/bin/

echo "Hack font"
echo
./fonts.inc


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
	i3 \
	i3blocks \
	i3lock \
	vim \
	build-essential \
	dmenu

echo "nvm"
PROFILE=/dev/null bash -c 'curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.40.0/install.sh --no-use | bash'
nvm install node

echo "dotfiles"
if [ -d "${HOME}/dotfiles" ]; then
	rm -rf ~/dotfiles
fi
git clone https://github.com/tgasslander/dotfiles.git ~/dotfiles
cd ~/dotfiles
stow i3 i3blocks nvim p10k Xresources tmux kitty
cd -
echo "stowed configs from dotfiles"

echo "zsh"
echo
if [ ! -d "${SZH}" ]; then
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
	echo "removing old oh-my-zsh"
	rm -rf "${HOME}/.oh-my-zsh"
fi
source "oh-my-zsh.inc"

mv ~/.zshrc ~/.zshrc.bak
cd ~/dotfiles
stow zsh
cd -

echo "Powerlevel10k theme"
echo
source powerlevel10k.inc

source ~/.zshrc
echo "Done"
