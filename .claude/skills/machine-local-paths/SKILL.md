---
name: machine-local-paths
description: The machine-local path invariant and its CI gate (scripts/check-machine-paths.mjs) — what it forbids, how to run it CORRECTLY (untracked files are invisible to it), the four legitimate ways to answer a violation, where machine facts live (.ruitk-local.json), the engine-root resolution chain, and the copy-to-a-differently-named-folder portability test. Use when the gate fails, when adding or editing a .vscode config / script / workflow / csproj, after any rename or repo-wide sweep, when wiring a new external tool or engine version, or when moving/renaming the checkout.
---

# Machine-local paths

## The invariant

**No tracked file may name a path that exists only on one machine.** Repo locations are DERIVED, never
written down. External tools are PROBED, with an override chain. The irreducible machine values live in
one gitignored file. A CI gate enforces it.

Why it exists — and this repo is the origin story: the rebrand sweep rewrote the repo-folder segment
*inside* a hardcoded absolute path in `.vscode/launch.json` (`…/ReactiveUI-Unreal/…` →
`…/ruitk-unreal/…`), so F5 opened a workspace that did not exist. Three independent audits read that
line, classified it "owner machine path — leave it", and moved on. **There were two copies of that
config** and the first fix caught only one. Judgment missed it repeatedly; hence a gate.

## The two rules

- **R1 — personal roots.** A drive-absolute path (`<drive>:\…`) or an explicit user-home POSIX path
  (`/home/<u>/`, `/Users/<u>/`, `/mnt/<d>/`) whose root is not in `ALLOWED_ROOTS`. Shared platform and
  CI roots are **deliberately legal**: `clangdProxy.ts` probing `C:\Program Files\LLVM`,
  `package-plugin.ps1` enumerating `C:\Program Files\Epic Games\UE_*`, and `engine-tests.yml` naming
  `/home/ue4` (Epic's prebuilt Linux container — a fact about the IMAGE, identical on every runner)
  are all correct and must keep naming them.
- **R2 — portability-critical files.** `.vscode/*.json` **at any depth**, `*.csproj`, `*.sln`,
  `*.code-workspace` must contain **zero** drive-absolute paths, even standard ones. This leg has a
  nested `ide-extensions/vscode-uetkx/.vscode/launch.json` — **edit one launch config, edit the
  other**; that pair is exactly how the original bug survived its first fix.

## Running it

```bash
node scripts/check-machine-paths.mjs          # the gate (exit 1 on violation)
node scripts/check-machine-paths.mjs --list   # every absolute path found, with a verdict each
```

**THE TRAP — new files are invisible.** The gate enumerates `git ls-files`, i.e. tracked files only. A
brand-new file is untracked, so the gate skips it and reports green — then turns red on the commit
that adds it. When your change ADDS files, test post-commit reality:

```bash
git add -N <the new files>
node scripts/check-machine-paths.mjs
git reset
```

## A violation has exactly four legitimate answers

1. **Derive it** — repo root via `git rev-parse --show-toplevel` or a script's own `..`; worktrees via
   `git worktree list` (convention: a sibling `…-work-<topic>` worktree, branched off `origin/dev`);
   `${workspaceFolder}` in VS Code configs. Positional workspace paths beat `--file-uri=`: they also
   survive spaces in the checkout path.
2. **Probe + override** — `$ENV_VAR` → `.ruitk-local.json` → PATH / standard roots → an error naming
   all three rungs. `scripts/package-plugin.ps1` is the worked example: `-EngineRoots` → `$UE_ROOT` →
   `.ruitk-local.json` `engineRoot` → Launcher enumeration.
3. **Exempt it, with a reason** — `EXEMPT` entries carry a `why`. Earned by frozen tiers
   (`plans/archive/**`, `research/**`, `plans/BENCH_BASELINES.md` — bench numbers are only readable
   *with* the machine context they were taken on — shipped CHANGELOG bodies and their plugin mirror,
   old DISCORD posts) and test trees (`ide-extensions/lsp-server/src/test/**`, `scripts/smoke.js`'s
   synthetic `file:///tmp/…` document URIs).
4. **Mark the line** — trailing `path-gate-allow: <reason>`, as used in `uri.ts` where a doc-comment
   must show a literal Windows path to explain URI conversion.

**Never widen `ALLOWED_ROOTS` to make a violation pass.**

## Machine facts: `.ruitk-local.json`

Gitignored, beside `publisher-secrets.json` in `.gitignore`; copy `.ruitk-local.example.json`.
**Nothing may require it** — discovery must work without it. Keys here: `engineRoot`, `clangFormat`
(the pinned formatter is **19.1.5**; PATH is probed first, and on a box without it on PATH this file
pins the VS2022-bundled binary). The authoritative resolution table lives in `CLAUDE.md`'s
"Machine-local paths" section; skills reference it rather than restating it — that is the pattern the
already-clean skills use, and the reason `test-run` and `CLAUDE.md` no longer drift.

Precedent worth knowing: `.ruitk-seller-repo` is a *tracked* dot-file sentinel read at runtime by the
codegen. Repo-identity facts can be tracked marker files; machine facts cannot.

## The portability acceptance test

Proves the tree works elsewhere, including pending uncommitted edits a `git clone` cannot see:

```bash
mkdir -p <scratch>/wholly-other-name
tar --exclude=node_modules --exclude=Binaries --exclude=Intermediate --exclude=DerivedDataCache \
    --exclude=Saved --exclude=out -cf - . | tar -C <scratch>/wholly-other-name -xf -   # keep .git
cd <scratch>/wholly-other-name && git add -N scripts/check-machine-paths.mjs .ruitk-local.example.json
node scripts/check-machine-paths.mjs        # must be green HERE
grep -c "C:" .vscode/launch.json ide-extensions/vscode-uetkx/.vscode/launch.json   # both must be 0
```

Confirm `pwd` is inside the copy before trusting the result (see scar tissue).

## Scar tissue

- **A failed copy produced a fake green.** When `robocopy` failed, the following `cd` failed too, so
  the gate ran in the ORIGINAL folder and printed ✓. Always verify `pwd`. `tar | tar` works in Git Bash.
- **The gate scans itself.** Specimens in it are written `<drive>:` on purpose; a self-exemption would
  be a permanent blind spot. Don't "fix" them into literal drive letters.
- **Escaped backslashes.** JS/TS/JSON spell a separator `\\`; each allowed root is admitted twice
  (plain + doubled), derived in the engine section, never hand-typed. This leg needs it most — every
  clangd/bsdtar probe and the user-facing setting description live in TypeScript or JSON.
- **`/home/ue4` has no trailing slash on purpose** — `engine-tests.yml` also spells the bare
  `HOME=/home/ue4` with nothing after it, and the root test is a prefix match on raw line text.
- **Space-containing roots.** `C:\Program Files (x86)\…` — the check tests the raw line from the match
  offset, because captured hits stop at the first space.
- **`x:\n` is not a path.** Codegen emitting indented C++ (`"%scase %s:\n%s{"`) looks drive-absolute;
  filtered by rule (escape letter followed by a non-word char). A naive `\b[A-Za-z]:` matcher fires on
  `%s:` because `%s` ends on a word char.
- **A rename sweep once injected the product's DISPLAY name into a filesystem path** — a worktree path
  under `<personal-root>\Reactive UI Toolkit\…` (spaces and all) that had never existed, because the
  sweep rewrote the parent folder segment into the product name. When a sweep touches path strings,
  re-read them as paths, not as tokens.
