# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Working style

Be concise in your answers.

## What this is

A set of Bash scripts that bootstrap a personal Linux dev environment from scratch: terminal emulators (WezTerm, kitty), window manager (i3 + i3blocks/i3lock/picom/rofi/feh), shell (zsh + oh-my-zsh + starship), editor (Neovim), Hack Nerd Font, tmux, GNU stow, language toolchains (node via nvm, Go via snap), and Kubernetes/DB CLI tooling (kubectl, k9s, kubectx/kubens, the psql client). It clones the user's dotfiles repo and applies them with `stow`.

## Running

```bash
./env-install.sh          # full bootstrap; run from repo root, not via sudo
```

The script is idempotent: reruns `git pull` instead of re-cloning `~/dotfiles`, skip oh-my-zsh/font install if already present, `stow -R` re-links without erroring, and `~/.zshrc` is only backed up once (guarded so a rerun never clobbers an existing `~/.zshrc.bak`). There is no lint or test suite; validate changes with `bash -n <file>` (syntax) and `shellcheck` if available.

## Architecture

- `env-install.sh` is the **orchestrator**. It runs package installs itself, then pulls in each `*.inc` file for a specific component. The `.inc` files are plain Bash sourced or executed from the orchestrator, not a plugin system — order matters (k8s → fonts → i3 → nvm → dotfiles → zsh → nvim → oh-my-zsh → stow zsh → starship → node → go).
- `.inc` files are invoked inconsistently: some with `source` (`i3.inc`, `zsh.inc`, `nvim.inc`, `oh-my-zsh.inc`, `starship.inc`) and two by execution (`./fonts.inc`, `./k8s.inc`). `source` runs in the current shell (env changes persist, `cd` leaks); execution runs in a subshell. Preserve the existing mechanism for a given step unless intentionally changing it, since some steps rely on shared state (e.g. nvm/node).
- Step ordering has real dependencies: `nvim.inc` runs `npm install --global yarn`, so `nvm install node` must run first (it does, right after nvm itself is installed); `stow` steps require `~/dotfiles` to already be cloned; `starship.inc` assumes the zsh + oh-my-zsh + stowed `.zshrc` chain completed. The script never sources `~/.zshrc` itself (it's zsh syntax, and the script runs under bash) — it just prints a reminder to open a new shell.

## Cross-platform goal (Debian + Arch)

The scripts are currently **Debian/Ubuntu-only**: they hardcode `apt`, `apt-key`, `/etc/apt/sources.list.d/`, and `.deb` third-party repos (WezTerm apt.fury.io, i3 baltocdn). Work in this repo is aimed at making it also run on **Arch-based** systems. When touching install logic:
- Detect the distro/package manager rather than assuming `apt` (e.g. presence of `apt` vs `pacman`, or `/etc/os-release`).
- Map package names across managers — they differ (`build-essential` → `base-devel`; `python3-venv` is bundled with `python` on Arch; i3 gaps are mainline in `i3-wm` on Arch, so the custom apt repo in `i3.inc` is Debian-specific and should be skipped there).
- Prefer native packages over the apt-repo / snap / curl-pipe installers where an Arch package exists (WezTerm, Go, Neovim, starship are all in Arch repos/AUR), while keeping the Debian path working.
