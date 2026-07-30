# Neovim plugin provisioning in the bootstrap

## Background

`env-install.sh` installs Neovim and stows the config, but never installs the
Neovim *plugins*. `lazy.nvim` bootstraps itself and installs plugins on the
first interactive `nvim` launch, which means every plugin's `build` step runs
unsupervised, outside the bootstrap, with no one watching the output.

That gap produced a real failure. `markdown-preview.nvim` was specified with
`build = "cd app && yarn install"`. Its `app/node_modules` was never
installed, so `<Leader>md` (`:MarkdownPreviewToggle`) silently did nothing:

- The plugin looks for a prebuilt server at
  `app/bin/markdown-preview-<platform>`, which did not exist.
- It fell back to `node app/index.js`, which died instantly with
  `Cannot find module 'tslib'`.
- The keymap uses `silent = true` and the server runs as a background job, so
  the Node stack trace never reached `:messages`.

The root cause of the failed build is that `node` and `yarn` come from **nvm**,
which is only sourced from `~/.zshrc`. Any `nvim` launched without an
interactive zsh's PATH — a Sway keybind, a desktop entry, a `.desktop` action —
cannot see them. Under a bare PATH the plugin's own diagnostic is
`Pre build and node is not found`.

The dotfiles side of the fix — switching `build` to the plugin's own prebuilt
binary downloader, which needs neither node nor yarn at build or run time — is
handled in a separate commit to the dotfiles repo. This spec covers the
bootstrap side: making plugin provisioning happen *during* the install, where
failures are visible.

### Two traps found while verifying this

Both were found by actually running a fresh install in an isolated
`XDG_*` prefix, not by reading docs. Both produce a *silent* failure:

1. **`mkdp#util#install()` is asynchronous.** It dispatches the download via
   `mkdp#util#open_terminal()` (`autoload/mkdp/util.vim:141`). Under
   `nvim --headless ... +qa` the editor exits before the job finishes and
   nothing lands. `mkdp#util#install_sync()` (util.vim:162) routes to
   `install(v:true)`, which uses a blocking `execute '!' . cmd` — that is the
   variant a non-interactive bootstrap needs.
2. **An `ft`-lazy-loaded plugin is not on the runtimepath when its `build`
   task runs.** The plugin's `autoload/` is therefore unreachable and the build
   fails with `Vim:E117: Unknown function: mkdp#util#install_sync`. The build
   function must `vim.opt.runtimepath:append(plugin.dir)` first.

**Critically, lazy.nvim logs a failing build task but still exits 0.** A green
exit from the headless run does not mean the builds succeeded. This is the same
silence that hid the original bug, and it is why the explicit binary assertion
below — not the sync's exit status — is the real gate.

## Decisions

These were confirmed with the user before writing this spec:

- **Provision all plugins headlessly during the bootstrap**, not just the
  markdown-preview binary. The narrow fix would leave every other plugin's
  build silent on failure — the same class of bug, one plugin over.
- **Retry once, then warn.** A failed server-binary download is most likely a
  network blip, so it is retried once before being reported.
- **A failed build must not abort the bootstrap.** `nvim.inc` is *sourced* by
  the orchestrator, so a bare `exit` would kill `env-install.sh` and lose the
  remaining steps (starship, go). Warnings are recorded and reprinted in a
  final summary instead.
- **`Lazy! install`, not `Lazy! sync`.** `sync` also *updates* every plugin,
  so rerunning the bootstrap would silently move plugin versions. The
  bootstrap's job is to provision, not to upgrade. `install` still runs the
  `build` step for each newly installed plugin, which is the part that
  matters. `Lazy! restore` is not an option: the dotfiles `.gitignore`
  excludes `nvim/lazy-lock.json`, so a fresh clone has no lockfile.
- **The verification is an explicit filesystem assertion, not an exit-code
  check.** lazy.nvim exits 0 even when a build task errors, so only checking
  for the binary on disk actually proves anything.

### Cross-repo dependency

This only works in combination with the dotfiles commit, which must be on
`origin/master` before the next machine is provisioned (`env-install.sh:90`
clones from GitHub). The `build` there must be:

```lua
build = function(plugin)
	vim.opt.runtimepath:append(plugin.dir)
	vim.fn["mkdp#util#install_sync"]()
end,
```

Neither half is sufficient alone: without the dotfiles change the bootstrap
provisions a plugin whose build is broken, and without the bootstrap change a
broken build stays invisible until someone presses `<Leader>md`.

## Scope

In scope:
- A plugin-provisioning block appended to `nvim.inc`
- An explicit post-install assertion that the markdown-preview server binary
  exists, with one retry
- A deferred-warning mechanism (`warn` + `WARNINGS`) in `common.inc`, and a
  warning summary in `env-install.sh` before `Done`
- `CLAUDE.md` updates describing `nvim.inc`'s expanded role
- A `docs/TODO.md` entry for installing `uv` via the bootstrap

Out of scope (deliberately not included):
- Making `node` available outside nvm (a system/distro node package) so
  GUI-launched `nvim` has it on PATH. This is a real remaining instance of the
  same PATH class — mason-installed LSP servers and copilot do need node at
  runtime — but the prebuilt markdown-preview binary sidesteps it entirely,
  and widening the change to the nvm setup is a separate decision.
- Pre-seeding mason LSP servers or treesitter parsers as explicit steps. They
  are installed as a side effect of the headless run if the config asks for
  them.
- Committing a `lazy-lock.json` to pin plugin versions.

## Architecture

### Placement

The new block goes at the end of `nvim.inc`, which `env-install.sh` sources at
line 125. That position already satisfies both preconditions, so no reordering
is needed:

- **Config present**: `stow -R ... nvim ...` ran at lines 95–98, so
  `~/.config/nvim` symlinks resolve into `~/dotfiles`.
- **node/yarn on PATH**: nvm was installed and `nvm.sh` sourced into *this*
  shell at lines 79–81, and `nvim.inc` is sourced rather than executed, so it
  inherits them. `nvim.inc` also installs `yarn` itself, immediately above.

Neovim itself is installed at the top of `nvim.inc`, so the binary exists by
the time the block runs.

### Provisioning step

```
timeout 900 nvim --headless "+Lazy! install" +qa
```

The `!` makes the command blocking rather than opening the interactive UI. This
was confirmed rather than assumed: in the isolated run the clone *and* the build
task both completed before nvim exited. On a fresh box `init.lua` git-clones
lazy.nvim itself first (init.lua:2–12), which also works headlessly.

The `timeout` is a guard against an unattended hang — a bootstrap that stops
forever on a plugin waiting for input is worse than one that warns. A non-zero
status is recorded as a warning rather than treated as fatal, matching the
rest of the repo's behavior under no `set -e`.

A *zero* status, however, proves nothing: lazy.nvim reports a failed build task
in its log and exits 0 anyway. The step is therefore treated as best-effort
provisioning, and the assertion below is what actually decides success.

### markdown-preview assertion

The one plugin whose breakage motivated this gets an explicit check, because
"the headless install exited 0" does not prove its server binary landed:

```
MKDP_BIN_DIR="${HOME}/.local/share/nvim/lazy/markdown-preview.nvim/app/bin"

mkdp_installed() {
	[ -n "$(find "${MKDP_BIN_DIR}" -maxdepth 1 -name 'markdown-preview-*' \
		-type f -print -quit 2>/dev/null)" ]
}
```

`find` is used rather than a glob so a missing directory is a clean false
instead of an unexpanded pattern.

1. If not installed, print a message and retry once. The plugin must be pulled
   onto the runtimepath first, for the same E117 reason as the build task, and
   the retry uses the blocking variant:

   ```
   nvim --headless "+Lazy! load markdown-preview.nvim" \
        "+call mkdp#util#install_sync()" +qa
   ```

2. If still not installed, `warn` with that exact command so it can be rerun by
   hand.

Note the prebuilt binary is `markdown-preview-linux` (x64). On arm64 it would
not execute and the plugin would fall back to `node`; this is recorded as a code
comment, not handled, since the machines currently in use are amd64.

### Deferred warnings

`common.inc` gains:

```
WARNINGS=()

warn() {
	echo "WARNING: $*" >&2
	WARNINGS+=("$*")
}
```

`common.inc` is sourced by `env-install.sh` at line 3, and `nvim.inc` is
sourced too, so both share one shell and the array accumulates correctly.

This aggregation only works for **sourced** steps. `fonts.inc`, `k8s.inc`, and
`docker.inc` are executed in subshells; a `warn` call there would print but
not propagate to the summary. Those files are unchanged here, so this is a
documented limitation of the helper, not a live bug.

`nvim.inc` defines a fallback so it stays runnable standalone:

```
if ! declare -F warn >/dev/null; then
	warn() { echo "WARNING: $*" >&2; }
fi
```

CLAUDE.md:24 explicitly calls out the sourced-vs-executed footgun; without
this guard, running `./nvim.inc` directly would fail on an undefined function.

`env-install.sh` prints the summary immediately before `Done`:

```
if [ ${#WARNINGS[@]} -gt 0 ]; then
	echo
	echo "Completed with ${#WARNINGS[@]} warning(s):"
	for w in "${WARNINGS[@]}"; do
		echo "  - $w"
	done
fi
```

## Idempotency

- `Lazy! install` only installs plugins that are missing; already-installed
  plugins are untouched and not updated.
- The binary check short-circuits the download when the binary is already
  present, so a rerun does not refetch 45 MB.
- `mkdp#util#install()` early-returns when the on-disk binary's reported
  `--version` already matches `app/package.json` (util.vim:142–146), so even a
  direct invocation is a no-op once provisioned.

## Error handling

- Headless run exits non-zero or times out → `warn`, bootstrap continues.
- Binary missing after the headless install → one retry, then `warn` carrying
  the manual rerun command.
- No warnings → no summary block printed, output is unchanged from today.

## Testing

There is no test suite in this repo.

### Static checks

- `bash -n nvim.inc common.inc env-install.sh` (syntax)
- `shellcheck nvim.inc common.inc env-install.sh`
- `shfmt -d nvim.inc common.inc env-install.sh` (repo uses tab indentation)

### Already verified

Verified before writing this spec, by simulating a fresh machine in an
isolated `XDG_CONFIG_HOME`/`XDG_DATA_HOME`/`XDG_STATE_HOME`/`XDG_CACHE_HOME`
prefix so nothing touched the live `~/.local/share/nvim`:

- `build` using bare `mkdp#util#install()` → **no binary** (async, editor exits
  first).
- `build` using `install_sync()` without the rtp append → **no binary**,
  `Vim:E117`, lazy exits 0 regardless.
- `build` using `runtimepath:append(plugin.dir)` + `install_sync()` → binary
  produced in 749 ms, headless, from a clean clone.
- That isolated install then served `http://localhost:8315/page/1` on
  `:MarkdownPreviewToggle` **with nvm stripped from PATH**, proving the
  provisioned result needs no node at run time.

### Still to verify during implementation

- The retry command (`+Lazy! load ...` then `+call mkdp#util#install_sync()`)
  repairs a deliberately deleted `app/bin`.
- The `WARNINGS` array actually accumulates across the sourced `nvim.inc` into
  the orchestrator's summary.

## Documentation

`CLAUDE.md` is updated in two places:
- "Architecture" — `nvim.inc` now provisions plugins, not just Neovim and
  yarn; note that this is why it must run after the `stow` step.
- "Architecture" — document `common.inc`'s `warn`/`WARNINGS` contract and its
  sourced-only limitation.

`docs/TODO.md` gains an entry: `uv`/`uvx` are currently 55 MB binaries the
installer drops into the stowed `scripts/.local/bin/`, now gitignored in
dotfiles; the bootstrap should install `uv` instead.
