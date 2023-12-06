#!/bin/bash

if [ ! -d "${HOME}/.oh-my-zsh/custom/themes/powerlevel10k" ]; then
	git clone --depth=1 https://github.com/romkatv/powerlevel10k.git ${ZSH_CUSTOM:-$HOME/.oh-my-zsh/custom}/themes/powerlevel10k
else
	echo "powerlevel10k already installed"
fi
