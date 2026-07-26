#!/bin/bash

# Docker Engine, CLI, and the compose + buildx plugins.
#
# On Arch, docker/docker-compose/docker-buildx come from the main package list
# in env-install.sh, so this file only runs the shared post-install there.
# Debian's own docker.io package lags upstream and ships no compose v2 plugin,
# hence the download.docker.com repo below.
#
# Executed (not sourced) by env-install.sh, so common.inc's pkg_install is not
# available here — only the exported PKG_MGR. apt is called directly, the same
# way k8s.inc does it.

install_docker_apt() {
	if command -v docker >/dev/null 2>&1; then
		echo "docker already installed, skipping repo setup"
		return 0
	fi

	sudo apt install -y ca-certificates curl

	# shellcheck disable=SC1091
	. /etc/os-release

	# Docker publishes only debian and ubuntu repos. Derivatives (Mint, Pop)
	# put their own name in ID but list ubuntu in ID_LIKE.
	case "${ID:-}${ID_LIKE:-}" in
	*ubuntu*) flavor=ubuntu ;;
	*) flavor=debian ;;
	esac

	# Derivatives put their own release name in VERSION_CODENAME and the
	# upstream Ubuntu suite in UBUNTU_CODENAME, so prefer the latter. Plain
	# Ubuntu sets both the same; Debian sets only VERSION_CODENAME.
	suite="${UBUNTU_CODENAME:-${VERSION_CODENAME:-}}"
	if [ -z "${suite}" ]; then
		echo "No UBUNTU_CODENAME/VERSION_CODENAME in /etc/os-release, cannot pick a docker suite" >&2
		return 1
	fi

	echo "Adding the Docker repository (${flavor} ${suite})"
	curl -fsSL "https://download.docker.com/linux/${flavor}/gpg" |
		sudo gpg --yes --dearmor -o /usr/share/keyrings/docker.gpg
	echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/docker.gpg] https://download.docker.com/linux/${flavor} ${suite} stable" |
		sudo tee /etc/apt/sources.list.d/docker.list

	sudo apt update
	sudo apt install -y docker-ce docker-ce-cli containerd.io \
		docker-buildx-plugin docker-compose-plugin
}

if [ "$PKG_MGR" = "apt" ]; then
	install_docker_apt
else
	echo "docker/docker-compose/docker-buildx come from native packages on this distro, skipping install"
fi

# Post-install, both distros. The docker package normally creates the group
# already, so groupadd here is defensive.
if getent group docker >/dev/null 2>&1; then
	echo "docker group already exists"
else
	echo "Creating the docker group"
	sudo groupadd docker
fi

if id -nG "$USER" | tr ' ' '\n' | grep -qx docker; then
	echo "$USER is already in the docker group"
else
	echo "Adding $USER to the docker group"
	sudo usermod -aG docker "$USER"
fi

if command -v systemctl >/dev/null 2>&1; then
	echo "Enabling the docker daemon"
	sudo systemctl enable --now docker
else
	echo "No systemctl found, start the docker daemon yourself" >&2
fi

echo "docker group membership only applies to new login sessions; run 'newgrp docker' to use docker in this shell without re-logging in"
