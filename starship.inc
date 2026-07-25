#!/bin/bash

if command -v pacman >/dev/null 2>&1; then
	sudo pacman -S --needed --noconfirm starship
else
	curl -sS https://starship.rs/install.sh | sh
fi
