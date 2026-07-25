#!/bin/bash

# i3 v4.22+ is required for gaps. On Arch this is mainline in i3-wm (installed
# from the main package list), so the extra repo below is Debian-only.
if ! command -v pacman >/dev/null 2>&1; then
	echo "Adding i3 development repository (Debian)"
	curl https://baltocdn.com/i3-window-manager/signing.asc | sudo apt-key add -
	sudo apt install apt-transport-https --yes
	echo "deb https://baltocdn.com/i3-window-manager/i3/i3-autobuild-ubuntu/ all main" | sudo tee /etc/apt/sources.list.d/i3-autobuild.list
	sudo apt update
	# sudo apt install i3
fi
