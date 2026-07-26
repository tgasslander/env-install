# Docker Install Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Install Docker Engine (plus the compose and buildx plugins) as part of the dev bootstrap on both Debian/Ubuntu and Arch, with the daemon enabled and the user in the `docker` group.

**Architecture:** A new executable `docker.inc`, called from `env-install.sh` right after the Kubernetes tooling block. On Debian it adds the official `download.docker.com` apt repo and installs `docker-ce` and friends; on Arch it installs nothing (the packages join the existing pacman list in `env-install.sh`). Both distros then run the same idempotent post-install: create/join the `docker` group and `systemctl enable --now docker`.

**Tech Stack:** Bash, apt + `gpg --dearmor` keyrings, pacman, systemd. Validation via `bash -n`, `shellcheck`, `shfmt`.

**Spec:** `docs/superpowers/specs/2026-07-26-docker-install-design.md`

## Global Constraints

- **Indentation is tabs.** Every existing `.sh`/`.inc` file in this repo uses hard tabs. `shfmt -d` will flag spaces.
- **`docker.inc` is executed, not sourced** (`./docker.inc`, like `./k8s.inc`). It runs in a child process, so it inherits the **exported `PKG_MGR`** from `common.inc` but **not** the `pkg_update`/`pkg_install` shell functions. Call `sudo apt install -y` / `sudo pacman` directly, exactly as `k8s.inc` does. Do not source `common.inc` from `docker.inc`.
- **No `set -e`** anywhere in this repo. A failing step reports and the bootstrap continues. Do not add `set -e` to `docker.inc`.
- **Idempotency is required.** The full bootstrap is designed to be re-runnable; every new command must be safe on a second run.
- **No test suite exists.** "Tests" in this plan means `bash -n`, `shellcheck`, and `shfmt -d`.
- **The user runs `sudo` commands themselves.** Do not attempt to execute `sudo`, `apt`, or `pacman` from a tool call — they need a real terminal for the password prompt. Verification steps that need root are handed to the user to run and report back.
- Arch packages are `docker`, `docker-compose`, `docker-buildx` (all in `extra`, no AUR).
- Debian packages are `docker-ce`, `docker-ce-cli`, `containerd.io`, `docker-buildx-plugin`, `docker-compose-plugin`.

---

### Task 1: Create `docker.inc`

**Files:**
- Create: `docker.inc` (mode 755)
- Test: none (no suite; validated with `bash -n`, `shellcheck`, `shfmt -d`)

**Interfaces:**
- Consumes: `PKG_MGR` (`apt` or `pacman`), exported by `common.inc` and inherited from `env-install.sh`'s environment. Nothing else.
- Produces: an executable `./docker.inc` that Task 2 calls from `env-install.sh`. It takes no arguments, reads no other variables, and leaves nothing behind for later steps.

- [ ] **Step 1: Write `docker.inc`**

Create `docker.inc` with exactly this content (tabs for indentation):

```bash
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
		*)        flavor=debian ;;
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
	curl -fsSL "https://download.docker.com/linux/${flavor}/gpg" \
		| sudo gpg --yes --dearmor -o /usr/share/keyrings/docker.gpg
	echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/docker.gpg] https://download.docker.com/linux/${flavor} ${suite} stable" \
		| sudo tee /etc/apt/sources.list.d/docker.list

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
```

- [ ] **Step 2: Make it executable**

```bash
chmod 755 docker.inc
```

- [ ] **Step 3: Verify syntax**

Run: `bash -n docker.inc`
Expected: no output, exit 0.

- [ ] **Step 4: Verify lint**

Run: `shellcheck docker.inc`
Expected: no output, exit 0.

Note: `. /etc/os-release` would normally raise SC1091 ("not following"); the inline `# shellcheck disable=SC1091` above it handles that. `$PKG_MGR` is not assigned in this file, so SC2154 ("referenced but not assigned") may fire depending on shellcheck version — if it does, resolve it exactly the way `k8s.inc` already resolves the same situation. Check first: `shellcheck k8s.inc`. If `k8s.inc` is clean with no directive, the same usage here should be too.

- [ ] **Step 5: Verify formatting**

Run: `shfmt -d docker.inc`
Expected: no diff output, exit 0. If it prints a diff, apply it with `shfmt -w docker.inc` and re-run `shellcheck`.

- [ ] **Step 6: Confirm the executable bit survived**

Run: `ls -l docker.inc`
Expected: mode `-rwxr-xr-x`, matching `k8s.inc` and `fonts.inc`.

- [ ] **Step 7: Commit**

```bash
git add docker.inc
git commit -m "Add docker.inc: docker engine, compose and buildx plugins"
```

---

### Task 2: Wire `docker.inc` into `env-install.sh`

**Files:**
- Modify: `env-install.sh` (Arch package list around lines 31-39; new call after the k8s block at lines 52-56)

**Interfaces:**
- Consumes: `./docker.inc` from Task 1 — an executable taking no arguments.
- Produces: nothing consumed by later tasks. Task 3 documents these two edits.

- [ ] **Step 1: Add the Arch packages**

In the `else` (pacman) branch of the main `pkg_install` call, append `docker docker-compose docker-buildx` to the last line. Change:

```bash
		network-manager-applet sysstat mako polkit-gnome \
		kubectl k9s kubectx postgresql-libs
```

to:

```bash
		network-manager-applet sysstat mako polkit-gnome \
		kubectl k9s kubectx postgresql-libs \
		docker docker-compose docker-buildx
```

Do **not** add anything to the apt branch — the Debian packages come from the docker repo, which does not exist yet at that point in the script.

- [ ] **Step 2: Add the `docker.inc` call**

Immediately after the existing Kubernetes block:

```bash
echo "Kubernetes tooling"
echo
# Executed, not sourced: nothing here needs to leak into the parent shell.
# No-op on Arch, where these are native packages installed above.
./k8s.inc
```

insert:

```bash

echo "Docker"
echo
# Executed, not sourced: nothing here needs to leak into the parent shell.
# On Arch the packages come from the list above; this still does the group
# and daemon setup on both distros.
./docker.inc
```

- [ ] **Step 3: Verify syntax**

Run: `bash -n env-install.sh`
Expected: no output, exit 0.

- [ ] **Step 4: Verify lint and formatting**

Run: `shellcheck env-install.sh && shfmt -d env-install.sh`
Expected: no output from either, exit 0.

- [ ] **Step 5: Confirm the call site reads correctly**

Run: `grep -n "docker" env-install.sh`
Expected: exactly two hits — the pacman package line and the `./docker.inc` call (plus the surrounding `echo "Docker"`).

- [ ] **Step 6: Commit**

```bash
git add env-install.sh
git commit -m "Run docker.inc from the bootstrap and add docker to the Arch package list"
```

---

### Task 3: Update `CLAUDE.md`

**Files:**
- Modify: `CLAUDE.md` ("What this is" paragraph; the two Architecture bullets covering execution mechanism and step order)

**Interfaces:**
- Consumes: the file layout produced by Tasks 1 and 2.
- Produces: nothing.

- [ ] **Step 1: Update "What this is"**

In the "What this is" paragraph, change the trailing clause:

```
and Kubernetes/DB CLI tooling (kubectl, k9s, kubectx/kubens, the psql client).
```

to:

```
Kubernetes/DB CLI tooling (kubectl, k9s, kubectx/kubens, the psql client), and
Docker (engine + compose/buildx plugins).
```

- [ ] **Step 2: Update the execution-mechanism bullet**

Change:

```
- `.inc` files are invoked inconsistently: some with `source` (`i3.inc`, `zsh.inc`, `nvim.inc`, `oh-my-zsh.inc`, `starship.inc`) and two by execution (`./fonts.inc`, `./k8s.inc`).
```

to:

```
- `.inc` files are invoked inconsistently: some with `source` (`i3.inc`, `zsh.inc`, `nvim.inc`, `oh-my-zsh.inc`, `starship.inc`) and three by execution (`./fonts.inc`, `./k8s.inc`, `./docker.inc`). Executed files run in a child process, so they see exported variables like `PKG_MGR` but not `common.inc`'s `pkg_install`/`pkg_update` functions.
```

- [ ] **Step 3: Update the step order**

Change the parenthesised order in the orchestrator bullet:

```
(k8s → fonts → i3 → nvm → dotfiles → zsh → nvim → oh-my-zsh → stow zsh → starship → node → go)
```

to:

```
(k8s → docker → fonts → i3 → nvm → dotfiles → zsh → nvim → oh-my-zsh → stow zsh → starship → node → go)
```

- [ ] **Step 4: Verify the edits landed**

Run: `grep -n -i "docker" CLAUDE.md`
Expected: three hits, one per step above.

- [ ] **Step 5: Commit**

```bash
git add CLAUDE.md
git commit -m "Document docker in CLAUDE.md"
```

---

### Task 4: Verify on hardware (Arch)

This task is run **by the user** in a real terminal — the commands need a sudo password prompt, and the group change needs a re-login. Hand them the commands and wait for the output; do not run them from a tool call.

**Files:** none (verification only)

**Interfaces:**
- Consumes: everything from Tasks 1-3.
- Produces: confirmation, or a bug to fix before the branch is finished.

- [ ] **Step 1: Ask the user to run the docker step**

```bash
cd ~/Downloads/temp/env-install
sudo pacman -S --needed --noconfirm docker docker-compose docker-buildx
PKG_MGR=pacman ./docker.inc
```

Expected output, in order: the "come from native packages" skip line; either "docker group already exists" or "Creating the docker group"; either "already in the docker group" or "Adding <user> to the docker group"; "Enabling the docker daemon"; the `newgrp` reminder. No errors.

- [ ] **Step 2: Ask the user to confirm the daemon is up**

```bash
systemctl is-active docker
systemctl is-enabled docker
```

Expected: `active` and `enabled`.

- [ ] **Step 3: Ask the user to confirm docker works as a non-root user**

In a shell that has picked up the new group (a fresh login, or `newgrp docker` first):

```bash
docker run --rm hello-world
docker compose version
docker buildx version
```

Expected: the hello-world greeting with no `permission denied ... /var/run/docker.sock`, then a compose v2 version string and a buildx version string.

- [ ] **Step 4: Confirm the rerun is idempotent**

```bash
PKG_MGR=pacman ./docker.inc
```

Expected: same output as Step 1, but with the "already exists" / "already in the docker group" branches taken. No errors, no duplicate group entries.

- [ ] **Step 5: Record the Debian path as unverified**

The apt path cannot be exercised on this hardware. When reporting completion, state plainly that Debian was validated by `bash -n`/`shellcheck`/`shfmt` and review against Docker's documented install steps only, and was not run.

---

## Notes for the implementer

- `common.inc` exits 1 when neither `apt` nor `pacman` is found. `docker.inc` does not repeat that check — by the time it runs, `env-install.sh` has already sourced `common.inc` and would have bailed.
- `id -nG | tr ' ' '\n' | grep -qx docker` is deliberate rather than `grep -q docker`: a plain substring match would also hit a group named `dockerless` or `docker-users`.
- `gpg --yes --dearmor` (not plain `--dearmor`) matters for idempotency — without `--yes`, gpg prompts on the second run when the keyring already exists, which would hang an unattended bootstrap.
