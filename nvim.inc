#!/bin/bash
APP_IMG_DIR="appimages"

echo "Using directory ${HOME}/${APP_IMG_DIR}" 
if [ ! -d ${HOME}/${APP_IMG_DIR} ]; then
	echo "Creating directory ${HOME}/${APP_IMG_DIR}"
	mkdir -p ${HOME}/${APP_IMG_DIR}
fi

if [ -f ${HOME}/${APP_IMG_DIR}/nvim.appimage ]; then
	echo "nvim appimage is already installed"
else
	echo "Downloading nvim"
	echo
	curl -LO https://github.com/neovim/neovim/releases/latest/download/nvim.appimage
	echo "Installing the appimage"
	chmod u+x nvim.appimage
	mv nvim.appimage ${HOME}/${APP_IMG_DIR}

fi

ln -s ${HOME}/${APP_IMG_DIR}/nvim.appimage ${HOME}/${APP_IMG_DIR}/nvim

echo "nvim installed successfully"
