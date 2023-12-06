#!/bin/bash

echo "Installing dev env"
echo

echo "zsh"
echo
if [ ! -d "${SZH}" ]; then
	source zsh.inc
else
	echo "ZSH already installed"
fi

echo "Hack font"
echo
./fonts.inc

echo "oh-my-zsh"
echo
if [ ! -d "${HOME}/.oh-my-zsh" ]; then
	source "oh-my-zsh.inc"
else 
	echo "oh-my-zsh already installed"
fi

echo "Powerlevel10k theme"
echo
source powerlevel10k.inc

sudo apt install -y \
	tmux \
	stow \
	feh

git clone https://github.com/tgasslander/dotfiles.git ~/dotfiles
