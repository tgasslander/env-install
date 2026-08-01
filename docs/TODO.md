# TODO

Transient list of tasks to be done. Not documentation — delete entries as they're completed.

## Arch support (Debian + Arch cross-platform)
- [ ] Test the full run end-to-end on an actual Arch system (only syntax-checked so far).
- [ ] Confirm `wezterm` is in the official Arch repos on the target box; fall back to AUR if not.
- [ ] `nvm`/`oh-my-zsh`/`fonts.inc` curl-pipe installers are cross-distro but unverified on Arch — sanity-check.

## Hyprland
- [ ] After every `hyprland` upgrade, check `hyprctl plugin list` and reinstall `hyprland-plugin-hy3` if hy3 is missing. This recurs indefinitely — it is not a one-time migration cost.
- [ ] Test native-Wayland `wezterm` under Hyprland. `$mod+Return` still launches it with `config.enable_wayland = false` (set 2026-07-26 for the wlroots `wl_shm` abort). Hyprland uses aquamarine, not wlroots, so the flag is likely unnecessary there — but `wezterm.lua` is shared with the Debian/i3 path, so it cannot simply be flipped.
- [ ] The 2026-07-25 spec's amendments record a `wezterm` → `kitty` switch for `$mod+Return` that never landed in the Sway config. Decide which is intended and make both sessions agree.
- [ ] `exit.sh`'s suspend/hibernate rely on `hypridle`'s `before_sleep_cmd` for locking, because `hyprlock` cannot daemonize. If `hypridle` is not running, suspend happens UNLOCKED. Verify on first login.

## Kubernetes tooling
- [ ] Verify the Debian path of `k8s.inc` on an actual Debian/Ubuntu box (Arch path tested, Debian path only URL-checked).
- [ ] `K9S_VERSION` in `k8s.inc` is pinned (currently v0.51.0) — bump periodically; the Arch side tracks the repo automatically.

## Docker
- [ ] Verify the Debian path of `docker.inc` on an actual Debian/Ubuntu box (Arch path tested; Debian path only static-checked against Docker's documented install steps).
- [ ] Debian-*based* derivatives (Kali `kali-rolling`, LMDE `faye`) resolve to `flavor=debian` with a suite Docker doesn't publish → 404 on `apt update`. Only Ubuntu derivatives are handled. Consider a suite allowlist.
- [ ] The `command -v docker` skip means a box that already has docker from `docker.io`, `podman-docker`, or `get.docker.com` silently never gets the compose/buildx plugins — the whole reason the official repo was chosen.
- [ ] `docker.inc` run standalone with `PKG_MGR` unset takes the Arch branch and installs nothing; `k8s.inc`'s inverted test fails safe by comparison.

## Done
- [x] Fix unset `SZH` guard in `env-install.sh` (now checks `command -v zsh`).
- [x] Fix invalid `//` comment in `i3.inc` (now `#`).
- [x] Add `common.inc` with `PKG_MGR` detection + `pkg_update`/`pkg_install` helpers.
- [x] Branch package lists per distro (`build-essential`→`base-devel`, drop `python3-venv`, `i3`→`i3-wm`).
- [x] Gate Debian-only third-party repos (WezTerm apt.fury.io, i3 baltocdn) behind the apt path.
- [x] Use native Arch packages for kitty, neovim, go, starship instead of snap / curl-pipe.
