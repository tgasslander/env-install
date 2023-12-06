#!/bin/bash
APP_IMG_DIR="appimages"

echo "Using directory ${HOME}/${APP_IMG_DIR}" 
mkdir -p ${HOME}/${APP_IMG_DIR}

echo "Downloading nvim"
echo
curl -LO https://github.com/neovim/neovim/releases/latest/download/nvim.appimage
echo "Installing the appimage"
chmod u+x nvim.appimage
mv nvim.appimage ${HOME}/${APP_IMG_DIR} 

ln -s ${HOME}/${APP_IMG_DIR}/nvim.appimage ${HOME}/${APP_IMG_DIR}/nvim
