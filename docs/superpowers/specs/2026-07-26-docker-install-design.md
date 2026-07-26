# Docker in the dev bootstrap (Debian + Arch)

## Background

`env-install.sh` bootstraps a personal Linux dev environment but installs no
container runtime. Docker is needed as part of the dev install on both
supported distro families.

The repo already has a pattern for exactly this shape of problem: `k8s.inc`
handles CLI tooling that Arch ships natively but Debian does not, by
short-circuiting on non-apt systems and doing the third-party repo /
release-download work only on Debian. Docker follows that pattern.

## Decisions

These were confirmed with the user before writing this spec:

- **Debian installs from the official `download.docker.com` apt repo**, not
  the distro's `docker.io` package and not the `get.docker.com` convenience
  script. The official repo tracks upstream and ships the v2 `docker compose`
  and `buildx` plugins, which is the closest match to what Arch's native
  packages provide. Adding a signed third-party apt repo is already an
  established pattern here (WezTerm in `env-install.sh`).
- **Rootful Docker with group membership**, not rootless. The script adds
  `$USER` to the `docker` group and enables the daemon so Docker works
  immediately after the bootstrap. Membership in the `docker` group is
  effectively root-equivalent; this is accepted as normal for a personal dev
  box.
- **The daemon is enabled and started** (`systemctl enable --now docker`) on
  both distros, rather than left for manual start.

## Scope

In scope:
- New `docker.inc`, executed (not sourced) from `env-install.sh`
- Debian: docker apt repo + keyring + engine/CLI/plugin packages
- Arch: `docker`, `docker-compose`, `docker-buildx` added to the existing
  pacman package list in `env-install.sh`
- Shared post-install: `docker` group membership, service enablement, and a
  re-login reminder
- `CLAUDE.md` updates describing the new component and its position in the
  step order

Out of scope (deliberately not included):
- `lazydocker`, `ctop`, or other container TUIs
- A custom `/etc/docker/daemon.json` (log rotation, registry mirrors, storage
  driver overrides) — upstream defaults are kept
- Rootless mode (`docker-rootless-extras`, `uidmap`, per-user systemd)
- `docker-machine` / Docker Desktop

## Architecture

### Invocation

`docker.inc` is a standalone executable script run as `./docker.inc` from
`env-install.sh`, matching `k8s.inc`. Execution (a subshell) is correct here
because nothing it does needs to leak into the parent shell: no `cd`, no env
vars consumed by later steps.

It is called immediately after the existing Kubernetes tooling block. Docker
has no dependency on any other step, and nothing later in the script depends
on it, so this placement is purely about grouping container tooling together.

`docker.inc` relies on `PKG_MGR` being exported by `common.inc`, which
`env-install.sh` sources at the top — the same contract `k8s.inc` uses.

### Debian path (`PKG_MGR = apt`)

1. If `docker` is already on `PATH`, print a skip message and exit 0.
2. Install prerequisites: `ca-certificates`, `curl` (via `pkg_install`; both
   are typically present already, `--needed`/`-y` semantics make this a
   no-op).
3. Resolve the repo flavor from `/etc/os-release`: `ubuntu` when `ID=ubuntu`
   or `ID_LIKE` contains `ubuntu`, otherwise `debian`. This makes derivative
   distros (Mint, Pop!_OS) resolve to the Ubuntu repo instead of 404ing on a
   nonexistent Debian suite.
4. Resolve the suite: `VERSION_CODENAME` from `/etc/os-release`, falling back
   to `UBUNTU_CODENAME` (derivatives set their own `VERSION_CODENAME` but
   carry the upstream Ubuntu one in `UBUNTU_CODENAME`). If neither is set,
   print an error and exit non-zero rather than writing a broken sources file.
5. Fetch `https://download.docker.com/linux/<flavor>/gpg` and dearmor it to
   `/usr/share/keyrings/docker.gpg` (`gpg --yes --dearmor`, so reruns
   overwrite cleanly).
6. Write `/etc/apt/sources.list.d/docker.list`:
   `deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/docker.gpg] https://download.docker.com/linux/<flavor> <suite> stable`
7. `sudo apt update`, then install `docker-ce docker-ce-cli containerd.io
   docker-buildx-plugin docker-compose-plugin`.

### Arch path (`PKG_MGR = pacman`)

`docker.inc` performs no installation. `docker`, `docker-compose`, and
`docker-buildx` are added to the `pkg_install` list in the pacman branch of
`env-install.sh` — all three are in the `extra` repo, so no AUR helper is
needed. This mirrors how kubectl/k9s/kubectx are handled.

`docker.inc` prints a message saying the packages come from the native list,
then falls through to the shared post-install below (unlike `k8s.inc`, which
exits early, because Docker still needs group and service setup on Arch).

### Shared post-install (both distros)

Runs after the distro-specific install, and is idempotent:

1. **Group**: if `getent group docker` finds nothing, `sudo groupadd docker`.
   Both the Arch package and the Debian `docker-ce` package normally create
   this group, so this is defensive.
2. **Membership**: if `id -nG "$USER"` does not already list `docker`, run
   `sudo usermod -aG docker "$USER"` and note that it was added.
3. **Service**: `sudo systemctl enable --now docker`. Guarded by a check that
   `systemctl` exists, so the script degrades to a printed warning rather than
   an error on a non-systemd system.
4. **Reminder**: print that the new group membership only applies to sessions
   started after re-login, and that `newgrp docker` picks it up in the current
   shell — same style as the existing `~/.zshrc` reminder in
   `env-install.sh`.

## Idempotency

Reruns of the full bootstrap must be safe, matching the rest of the repo:

- Debian install is skipped entirely when `docker` is on `PATH`.
- `gpg --yes --dearmor` overwrites the keyring without prompting.
- The sources file is rewritten with identical content.
- `groupadd` is guarded by `getent`; `usermod -aG` is guarded by `id -nG`.
- `systemctl enable --now` is inherently idempotent.

## Error handling

- Missing codename in `/etc/os-release` → error message on stderr, non-zero
  exit, no partial repo config written.
- Key download or `apt update` failure → the command's own non-zero status
  propagates; `docker.inc` runs under the orchestrator without `set -e`, so
  this matches how the rest of the repo behaves (a failing step reports and
  the bootstrap continues).
- No `systemctl` → warning on stderr, group setup still applied.

## Testing

There is no test suite in this repo. Validation is:

- `bash -n docker.inc && bash -n env-install.sh` (syntax)
- `shellcheck docker.inc env-install.sh`
- `shfmt -d docker.inc env-install.sh` (repo uses tab indentation)
- Manual on-hardware check on the Arch box: `docker run --rm hello-world`
  after re-login, and `docker compose version` / `docker buildx version`.
- The Debian path cannot be verified on the user's current hardware; it is
  written to mirror Docker's documented install steps and is flagged as
  unverified.

## Documentation

`CLAUDE.md` is updated in two places:
- "What this is" — add Docker to the tooling list.
- "Architecture" — add `docker.inc` to the step-order list and to the note
  about which `.inc` files are executed vs sourced.
