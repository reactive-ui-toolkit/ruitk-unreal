# Archived plans

Finished-or-dead plans move here (via the `plan-progress` skill) and get a row below — kept as
design/root-cause record, never as TODO. The row captures the outcome honestly, including
reversals and consciously-deferred scope (and where that scope is tracked now).

| Plan | Why archived |
|---|---|
| `AUDIT_2026-07-14.md` | Two-pass repo audit; every phase carries a FIXES APPLIED banner (evidence shas inside). Open remainder = §9 owner questions, carried to `plans/REMAINING.md` §3/§7 |
| `BUGHUNT_2026-07-12.md` | Round-1 bug hunt; all findings fixed (`ReactiveUI.Bugfix.*` suites are the proof) |
| `BUGHUNT_2026-07-12_round2.md` | Round-2 bug hunt (51 findings); fixed + headless-proven (`ReactiveUI.Bugfix2.*` + LSP suites) |
| `HMR_V2_PLAN.md` | HMR v2 shipped (TD-027 RESOLVED): Live-Coding-driven whole-project hot reload; the dev-loop interpreter it replaced was deleted |
| `IMPORT_EXPORT_PLAN.md` + `IMPORT_EXPORT_MASTER_PLAN.md` | Strict static imports/exports shipped (grammar, resolver, aggregators, `RUIMigrateImports` codemod, LSP import intelligence); tails resolved as TD-023/24/25 |
| `UETKX_DECLARATIONS_PLAN.md` | `hook`/`module` companion declarations shipped (TD-017 RESOLVED 2026-07-11) |
| `EXIT_ANIMATION_DESIGN.md` | The `<Presence>` boundary shipped as designed (TD-003 RESOLVED 2026-07-12; suite `ReactiveUI.Core.Presence`) |
| `PR_DESCRIPTION_uetkx-compiler.md` + `PR_DESCRIPTION_uetkx-imports.md` | Historical PR bodies for merged campaigns |
| ~~`OWNER_ACCEPTANCE_CHECKLIST.md`~~ + ~~`OWNER_ACCEPTANCE_CHECKLIST_v2.md`~~ | UN-ARCHIVED back to `plans/` 2026-07-16 for the post-widget-completion manual acceptance pass (owner request) |
| `WIDGET_COMPLETION_PLAN.md` | Widget-completion campaign COMPLETE (waves 0–4 + G, plugin 0.5.0→0.9.0, 63 tags): every clause of the v1 widget gate met; `plans/WIDGET_INVENTORY.md` (still live) is the ongoing tracker |
| `EXTENSION_LISTING_PLAN.md` | Marketplace-listing overhaul shipped (extensions 0.3.1): distinguishable display names + structured page bodies on both marketplaces, README generated via `changelog.mjs extract-overview` |
| `INCLUDE_RETIREMENT_PLAN.md` | Include retirement shipped (plugin 0.10.0, extensions 0.4.0): imports-only preambles, `import "@Header.h"` host includes, auto-included prelude, UETKX2317 hint, `-tidy` codemod; family follow-up tracked as TD-030 |
| `MARKUP_EVERYWHERE_PLAN.md` | The third IDE pass shipped (plugin 0.11.0, extensions 0.5.0): §1 crash-hardening + E2E, §2 markup-expression lifting, §3 Tools menu entry, §4 markup-everywhere grammar/codegen parity, §5 bundled clangd — all Verify green, release out (archived 2026-07-21) |
| `ES_MODULES_GENERAL_PLAN.md` + `ES_MODULES_EXECUTION_PLAN.md` | ES-modules redesign shipped (plugin 0.12.0, extensions 0.6.0): the Unreal leg (M0–M9) executed 2026-07-18 — banner + final evidence inside (128/128 battery, 49/49 LSP); the general plan is the verbatim family spec (do not edit — sibling copies must match) |
| `LSP_COMPLETION_PLAN.md` | TD-033/TD-034 complete 2026-07-18: N0–N7 landed (scope-aware rename/refs/completion + the clangd-rename stretch) — banner + commit shas inside |

## Still live in `plans/`

- `ROADMAP.md` — the living status source of truth (one row per phase).
- `MASTER_PLAN.md` — the AI-first execution plan (decisions D-01..D-33, 10 phases, ship gate).
- `REMAINING.md` — the consolidated backlog: every open item, categorized (the "what is left?" file).
- `TECH_DEBT.md` — the TD-### register (deliberate deferrals with resolutions).
- `WIDGET_INVENTORY.md` — the authoritative per-widget tracker (batches, engine-diff feedstock).
- `BENCH_BASELINES.md` — all bench numbers, with machine/config context.
- `PENDING_CHANGELOG.md` — staged release-note bullets between releases.
- `DISCORD_CHANGELOG.md` — community release notes (≤2000 chars/entry).
- `family-corpus.hash` — the mirrored grammar-corpus hash (CI tripwire input).
