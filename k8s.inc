#!/bin/bash

# Kubernetes CLI tooling that has no apt package.
#
# On Arch all of kubectl/k9s/kubectx (kubectx ships kubens too) come from the
# main package list in env-install.sh, so this file is a no-op there. Debian
# has none of them in apt, hence the snap + GitHub-release fallbacks below.

if [ "$PKG_MGR" != "apt" ]; then
	echo "kubectl/k9s/kubectx come from native packages on this distro, skipping"
	exit 0
fi

LOCAL_BIN="${HOME}/.local/bin"
mkdir -p "${LOCAL_BIN}"

# kubectl: snap rather than the pkgs.k8s.io apt repo, which pins a minor
# version in its URL and needs manual bumping at every k8s release.
if command -v kubectl >/dev/null 2>&1; then
	echo "kubectl already installed, skipping"
else
	echo "Installing kubectl (snap)"
	sudo snap install --classic kubectl
fi

# k9s: not in apt and the snap is community-maintained and lags upstream, so
# pull the release tarball. Bump K9S_VERSION to upgrade.
K9S_VERSION="v0.51.0"
if command -v k9s >/dev/null 2>&1; then
	echo "k9s already installed, skipping"
else
	case "$(dpkg --print-architecture)" in
		amd64) K9S_ARCH=amd64 ;;
		arm64) K9S_ARCH=arm64 ;;
		*)     K9S_ARCH="" ;;
	esac

	if [ -z "${K9S_ARCH}" ]; then
		echo "No k9s release build for $(dpkg --print-architecture), skipping" >&2
	else
		echo "Installing k9s ${K9S_VERSION}"
		k9s_tmp="$(mktemp -d)"
		if curl -fsSL \
			"https://github.com/derailed/k9s/releases/download/${K9S_VERSION}/k9s_Linux_${K9S_ARCH}.tar.gz" \
			| tar -xz -C "${k9s_tmp}" k9s
		then
			install -m 755 "${k9s_tmp}/k9s" "${LOCAL_BIN}/k9s"
			echo "k9s installed to ${LOCAL_BIN}/k9s"
		else
			echo "k9s download failed, skipping" >&2
		fi
		rm -rf "${k9s_tmp}"
	fi
fi

# kubectx/kubens: packaged on newer Debian/Ubuntu, but not everywhere. They are
# just two shell scripts upstream, so fall back to dropping them in ~/.local/bin.
if command -v kubectx >/dev/null 2>&1 && command -v kubens >/dev/null 2>&1; then
	echo "kubectx/kubens already installed, skipping"
elif apt-cache show kubectx >/dev/null 2>&1; then
	echo "Installing kubectx (apt)"
	sudo apt install -y kubectx
else
	echo "kubectx not in apt, installing the upstream scripts"
	for tool in kubectx kubens; do
		if curl -fsSL -o "${LOCAL_BIN}/${tool}" \
			"https://raw.githubusercontent.com/ahmetb/kubectx/master/${tool}"
		then
			chmod 755 "${LOCAL_BIN}/${tool}"
			echo "${tool} installed to ${LOCAL_BIN}/${tool}"
		else
			echo "${tool} download failed, skipping" >&2
		fi
	done
fi
