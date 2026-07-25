#!/bin/bash
if command -v pacman >/dev/null 2>&1; then
	sudo pacman -S --needed --noconfirm neovim
else
	sudo snap install nvim --classic
fi

echo "nvim installed successfully"

echo "install yarn for some lsp:s etc"
npm install --global yarn

echo "yarn installed"
