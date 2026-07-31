# Pending changelog ledger

One bullet per merged user-relevant change, staged by the `plan-progress` skill at
phase/milestone completion — while the knowledge is fresh. **Drained by `release-process` §0**
into the real lanes (A: root CHANGELOG.md · B: `changelog.mjs add` · C: Discord) and then
EMPTIED. Bullets that never reach a lane are how release notes go missing — drain, don't append
around.

**Format:** `- [lane A|B|C] [artifact] one-line summary (PR #n / sha)`

<!-- Drained 2026-07-16 for the widget-completion releases (plugin 0.5.0→0.9.0 + extensions
     0.3.0): every bullet verified present in Lane A ([0.5.0]/[0.6.0]/[0.7.0]/[0.8.0]/[0.9.0]
     sections), Lane B (changelog.json 0.3.0 — schema catch-up + sinceUE + wave G), or
     Lane C (DISCORD_CHANGELOG [0.5.0]→[0.9.0]). -->
<!-- Drained 2026-07-16 for the extension-listing release (extensions 0.3.1, no plugin
     bump): both bullets went straight into Lane B (changelog.json 0.3.1 — listing overhaul
     shared bullet + vscode README bullet) via plans/archive/EXTENSION_LISTING_PLAN.md's execution;
     no Lane A/C entries apply (listing-only change, not a Discord-worthy release). -->
<!-- Drained 2026-07-16 for the include-retirement release (plugin 0.10.0, extensions 0.4.0):
     the single bullet went straight into Lane A ([0.10.0]), Lane B (changelog.json 0.4.0 —
     shared entry), and Lane C (DISCORD_CHANGELOG [0.10.0]) as it was authored — nothing was
     staged here mid-campaign, so there was nothing to drain from THIS file, only to verify. -->
<!-- Drained 2026-07-17 for the markup-everywhere release (plugin 0.11.0, extensions 0.5.0):
     the §4 grammar bullet went into Lane A ([0.11.0] Added/Changed) + Lane C
     (DISCORD_CHANGELOG [0.11.0]); the Message Log menu bullet into Lane A ([0.11.0] Added);
     the LSP-mirror, crash-proofing/§2-channel, and §5 bundled-clangd bullets into Lane B
     (changelog.json 2026-07-17 shared entries, vscode+vs2022 0.5.0). -->
<!-- Drained 2026-07-18 for the ES-modules release (plugin 0.12.0, extensions 0.6.0): the
     campaign wrote its lanes directly at M8 (nothing was staged here mid-campaign) — Lane A
     ([0.12.0] Added/Changed/Fixed), Lane B (changelog.json shared entries: ES-modules LSP,
     TD-024 sidecar gate, Value.-payload ordering — vscode+vs2022 0.6.0), Lane C
     (DISCORD_CHANGELOG [0.12.0], 1895 chars). -->
(empty — drained 2026-07-18)

<!-- Drained 2026-07-25 for the field-test-campaign release (plugin 0.14.0, extensions 0.8.0):
     every staged bullet verified present in Lane A (CHANGELOG.md [0.14.0] — FSE, return null,
     the 0106/0109-0112/2311-2313/2329 validation wave, TB-13/14/15/21/23/26/29 fixes, DoomHUD),
     Lane B (changelog.json 2026-07-25 entry, 13 shared bullets — vscode+vs2022 0.8.0), or
     Lane C (DISCORD_CHANGELOG [0.14.0], 1868 chars). Field-test sessions: UE 5.6 full matrix
     owner-PASS 2026-07-25; UE 5.7 battery 132/132 same day (matrix pending); 5.8 pending
     engine repair. -->
(empty — drained 2026-07-25)

<!-- Drained 2026-07-28 for the rebrand release (plugin 0.15.0, extensions 0.9.0): nothing was
     staged here (the ledger was empty since 2026-07-25); the rebrand campaign wrote its lanes
     directly — Lane A (CHANGELOG.md [0.15.0] BREAKING section + mirror), Lane B
     (changelog.json 2026-07-28 entry, 4 shared bullets — vscode+vs2022 0.9.0), Lane C
     (DISCORD_CHANGELOG [0.15.0]). -->

<!-- Drained 2026-07-31 for the unified-settings release (plugin 0.16.0, extensions 0.9.1,
     staged-unpublished): both lane-A bullets verified present in Lane A (CHANGELOG.md
     [0.16.0] — URuitkSettings/persistable CVars under Added, the settings window under
     Added, the HMR-window slimdown + "Reactive UI Toolkit — Editor" page rename under
     Changed; mirror resynced); the lane-C bullet became DISCORD_CHANGELOG [0.16.0],
     written at staging time per the 0.15.0 precedent (owner pastes after publishing).
     Lane B this wave (changelog.json 2026-07-31, vscode+vs2022 0.9.1 — the TB-31
     embedded-clangd false-positive suppression) was authored directly at release prep;
     nothing extension-side was staged here, so there was nothing to drain for B. -->
<!-- Drained 2026-07-31 for the family-parity fold (OWNER'S FOLD RULING: the campaign folds
     INTO the staged-unpublished plugin 0.16.0 — no 0.17.0 anywhere; the [0.16.0] section was
     reshaped in place from the settings-wave entries into the one settings+parity wave):
     all eight lane-A bullets (M1 FlushSync fix → Fixed; M2 scheduler, M5 strict diagnostics,
     M6 environment, M7 trace ladder → Added; M3 defer-don't-restart, M4 FrameBudgetMs
     re-point + TimeSliceMs, M8 THE FLIP (loud, top of section) → Changed) verified present
     in Lane A (CHANGELOG.md [0.16.0] + byte-identical plugin mirror); the lane-C
     family-parity summary was folded into the REPLACED DISCORD_CHANGELOG [0.16.0] entry
     (1998 chars — settings + parity, battery 167/167 both worlds), owner pastes after
     publishing. Lane B: extensions untouched by this campaign — nothing to drain. -->
(empty — drained 2026-07-31)
