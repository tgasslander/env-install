#!/bin/bash

FONTS_DIR="${HOME}/.local/share/fonts"
FONT_NAME="Hack"
DL_DIR="fonts"

echo "Installing font: ${FONT_NAME}"
echo

# Check if the directory exists
mkdir -p "${FONTS_DIR}"
echo "Using font directory: ${FONTS_DIR}"

# Download fonts
cd ~/Downloads
mkdir -p "${DL_DIR}"

cd ${DL_DIR}
echo "Downloading ${FONT_NAME} into ${DL_DIR}..."
wget https://github.com/ryanoasis/nerd-fonts/releases/download/v3.1.1/Hack.zip
echo "Downloading done"

echo "Unzipping..."
unzip Hack.zip
echo "Unzipping done"


# Copy to fonts to all users
echo "Copying ${FONT_NAME} to ${FONTS_DIR}"
cp -r *.ttf "${FONTS_DIR}"
echo "Copying done"

echo "Cleaning up"
cd ..
rm -Rf ${DL_DIR} 

echo "Clearing the font cache"
fc-cache -f -v > /dev/null 2>&1

echo
echo "Font: ${FONT_NAME} is installed"
