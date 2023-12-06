#!/bin/bash
curl -LO https://github.com/neovim/neovim/releases/latest/download/nvim.appimage ~/appimages/
chmod u+x nvim.appimage
ln -s ~/appimages/nvim.appimage ~/appmages/nvim
