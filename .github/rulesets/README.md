# Branch rulesets — the canonical pair

`protect-dev.json` + `protect-master.json` are the family branch protection rulesets, versioned
here so the Settings-level state has an in-repo source of truth (same pair in every family repo).
Import via **Settings → Rules → Rulesets → Import a ruleset**; re-import after editing a file
here — the JSON does not apply itself.

- **The four required check contexts are load-bearing:** `gates`, `tests`, `extensions`, `docs`
  must match the job `name:`s in `.github/workflows/test.yml` exactly. Rename either side alone
  and every PR into dev becomes permanently un-mergeable (a required check that never reports).
- **`protect-master.json` deliberately has NO linear-history rule:** master takes merge commits
  (the merge-commit workflow), so requiring linear history would block the release flow. It only
  forbids deletion and non-fast-forward pushes.
- **`protect-dev.json`** additionally requires a PR (merge method: merge) with the four checks
  green and up to date (`strict_required_status_checks_policy`).
