#!/usr/bin/env node
// Copyright (c) 2026 Yaniv Kalfa. All Rights Reserved.
/**
 * CI gate: docs-drift tripwire (MASTER_PLAN Phase 0 step 3; consumed by the docs-sync skill
 * and Phase 8's Verify). Greps user-facing claimed counts ("N hooks", "M widgets") against
 * the actual registries, so the README can never again claim numbers the code doesn't have —
 * the automated fix for the sibling repo's "stale README claimed MVP / 10 elements / 6 hooks"
 * episode.
 *
 * CHECKS is intentionally empty in Phase 0: the registries it reads (the prop-map schema's
 * widget list, the hooks registry) don't exist yet, and the README deliberately makes no
 * count claims. WIRE A CHECK IN THE SAME PR THAT INTRODUCES A COUNT CLAIM — that's the rule
 * docs-sync enforces. Each check: { file, pattern (regex with one capture group = the claimed
 * number), source (function returning the true count) }.
 *
 * Example (Phase 2+, when the prop-map schema exists):
 *   {
 *     file: 'README.md',
 *     pattern: /(\d+) wrapped Slate widgets/,
 *     source: () => JSON.parse(readFileSync(resolve(REPO_ROOT, 'templates/prop-map.schema.json'), 'utf8')).widgets.length,
 *   }
 */
import { readFileSync, existsSync } from 'fs';
import { resolve, dirname } from 'path';
import { fileURLToPath } from 'url';
import { readdirSync } from 'fs';

const REPO_ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '..');

// ── registry readers (each derives the TRUE count from the artifact that defines it) ──────

/** The compiler-exported vocabulary the LSP + the docs catalog both read. */
function readUetkxSchema() {
  const raw = readFileSync(
    resolve(REPO_ROOT, 'ide-extensions/lsp-server/src/uetkx-schema.json'),
    'utf8',
  ).replace(/^\uFEFF/, '');
  return JSON.parse(raw);
}

/** Core hooks = the Ctx member hooks in RuitkContext.h (Use* + ProvideContext, minus *Impl)
 *  + UseSignal/UseSignalKey (RuitkSignal.h) + UsePresence (RuitkPresence.h) — the audited 23. */
function countCoreHooks() {
  const ctx = readFileSync(
    resolve(REPO_ROOT, 'Plugins/ReactiveUIToolkit/Source/RuitkCore/Public/RuitkContext.h'),
    'utf8',
  );
  const names = new Set();
  for (const m of ctx.matchAll(/\b(Use[A-Z]\w+|ProvideContext)\s*\(/g)) {
    if (!m[1].endsWith('Impl')) names.add(m[1]);
  }
  return names.size + 2 /* UseSignal, UseSignalKey */ + 1 /* UsePresence */;
}

/** Wrapped Slate widgets = the public FRuitkNode factories in RuitkSlate + core TextBlock. */
function countWidgetFactories() {
  const dir = resolve(REPO_ROOT, 'Plugins/ReactiveUIToolkit/Source/RuitkSlate/Public');
  const names = new Set();
  for (const f of readdirSync(dir)) {
    if (!f.endsWith('.h')) continue;
    const text = readFileSync(resolve(dir, f), 'utf8');
    for (const m of text.matchAll(/RUITKSLATE_API FRuitkNode ([A-Z]\w+)\s*\(/g)) {
      names.add(m[1]);
    }
  }
  return names.size + 1; // + Ruitk::TextBlock (core)
}

/** Gallery screens = the screen directories under Source/RuitkDemo/Screens. */
function countGalleryScreens() {
  return readdirSync(resolve(REPO_ROOT, 'Source/RuitkDemo/Screens'), { withFileTypes: true }).filter(
    (e) => e.isDirectory(),
  ).length;
}

/** Router hooks = the Use* free functions in RuitkRouter.h — matched by their PARAMETER LIST.
 *
 * A router hook is `Use…(FRuitkContext& Ctx, …)`, and that is what this keys on, because the
 * obvious alternative does not work: matching forward from `RUITKCORE_API` across the return type
 * needs a character class, and four of the seventeen hooks return types that contain parentheses —
 * `TFunction<void(const FString&, bool)> UseNavigate(…)`, `UseGo`, `UseBackStack`,
 * `UseSearchParams`. The old class excluded `(`, so the scan stopped inside the return type and
 * those four were never counted.
 *
 * IT READ 13 AND THE GATE STAYED GREEN, because the two catalog checks below used to fall back to
 * the catalog's own number whenever it disagreed with the registry — so a broken registry reader
 * and a drifted catalog were indistinguishable from a healthy pair. Both halves are fixed. */
function countRouterHooks() {
  const text = readFileSync(
    resolve(REPO_ROOT, 'Plugins/ReactiveUIToolkit/Source/RuitkCore/Public/RuitkRouter.h'),
    'utf8',
  );
  const names = new Set();
  for (const m of text.matchAll(/\b(Use[A-Z]\w*)\s*\(\s*FRuitkContext/g)) {
    names.add(m[1]);
  }
  return names.size;
}

/** Hook catalog entries by category — the generated per-hook docs pages read this file.
 *  The trailing comma distinguishes data entries from the interface's union-type line. */
function countHooksCatalog(category) {
  const text = readFileSync(resolve(REPO_ROOT, 'RuitkUnrealDocs~/src/hooksCatalog.ts'), 'utf8');
  return (text.match(new RegExp(`category: '${category}',`, 'g')) ?? []).length;
}

/** Automation tests = IMPLEMENT_*_AUTOMATION_TEST macros in the test module. */
function countAutomationTests() {
  const dir = resolve(REPO_ROOT, 'Source/RuitkHostTests/Private');
  let count = 0;
  for (const f of readdirSync(dir)) {
    if (!f.endsWith('.cpp')) continue;
    const text = readFileSync(resolve(dir, f), 'utf8');
    count += (text.match(/IMPLEMENT_\w*AUTOMATION_TEST\s*\(/g) ?? []).length;
  }
  return count;
}

/** The registry and the catalog must agree. Returns the agreed count, or -1 (which can never
 *  equal a claimed count) after naming both numbers — a check that silently substitutes one
 *  source for the other is not a check. */
function mustAgree(what, registry, catalog) {
  if (registry === catalog) {
    return registry;
  }
  console.error(`  ! ${what}: registry says ${registry}, hooksCatalog.ts has ${catalog}`);
  return -1;
}

const CHECKS = [
  {
    // README status blockquote: "(23 core hooks, ...".
    file: 'README.md',
    pattern: /\((\d+) core hooks/,
    source: countCoreHooks,
  },
  {
    // Docs intro: "UseState, UseEffect, and 21 more" (= the 23 total, 2 named + N more).
    file: 'RuitkUnrealDocs~/src/pages/Introduction/IntroductionPage.tsx',
    pattern: /and (\d+) more/,
    source: () => countCoreHooks() - 2,
  },
  {
    // README: "65+ wrapped Slate widgets" — a floor claim; the registry must be >= it.
    file: 'README.md',
    pattern: /(\d+)\+ wrapped Slate widgets/,
    source: () => {
      const actual = countWidgetFactories();
      return actual >= 65 ? 65 : actual; // a shrink below the floor surfaces the real number
    },
  },
  {
    // README: "The demo gallery's 16 screens".
    file: 'README.md',
    pattern: /demo gallery's (\d+) screens/,
    source: countGalleryScreens,
  },
  {
    // README: "green under a 165+-test headless automation battery" — floor claim
    // (raised from 100 at the family-parity fold; the battery ran 167 that day).
    file: 'README.md',
    pattern: /(\d+)\+-test headless/,
    source: () => {
      const actual = countAutomationTests();
      return actual >= 165 ? 165 : actual;
    },
  },
  {
    // Components Overview prose: "Markup tags (29 of them)" — must equal the schema exactly.
    file: 'RuitkUnrealDocs~/src/pages/ComponentsOverview/ComponentsOverviewPage.tsx',
    pattern: /\((\d+) of them\)/,
    source: () => Object.keys(readUetkxSchema().elements ?? {}).length,
  },
  {
    // Components Overview TAG_GROUPS chips: hand-listed, so gate MEMBERSHIP against the schema —
    // every schema tag in exactly one group's `tags:` array (the chips drifted silently once:
    // stuck at the 29-tag era while the schema hit 63). Mismatch returns -1 to fail loud.
    file: 'RuitkUnrealDocs~/src/pages/ComponentsOverview/ComponentsOverviewPage.tsx',
    pattern: /TAG_GROUPS chips: (\d+) tags/,
    source: () => {
      const src = readFileSync(
        resolve(REPO_ROOT, 'RuitkUnrealDocs~/src/pages/ComponentsOverview/ComponentsOverviewPage.tsx'),
        'utf8',
      );
      const chips = new Set();
      for (const block of src.matchAll(/tags: \[([^\]]*)\]/g)) {
        for (const t of block[1].matchAll(/'([^']+)'/g)) chips.add(t[1]);
      }
      const schema = new Set(Object.keys(readUetkxSchema().elements ?? {}));
      const same = chips.size === schema.size && [...schema].every((t) => chips.has(t));
      return same ? schema.size : -1;
    },
  },
  {
    // The generated per-hook docs pages: the catalog's CORE entries must cover every core hook.
    // (The claim line lives in the catalog header so the check self-anchors to the data file.)
    file: 'RuitkUnrealDocs~/src/hooksCatalog.ts',
    pattern: /(\d+) core hook entries/,
    source: () => {
      const registry = countCoreHooks();
      const catalog = countHooksCatalog('core');
      // MISMATCH IS A FAILURE, not a number. Returning the CATALOG's own count made the check
      // compare the claim against the thing the claim is written beside — so catalog-vs-registry
      // drift, the one thing this check exists to catch, passed silently.
      return mustAgree('core hooks', registry, catalog);
    },
  },
  {
    // ...and the ROUTER entries must cover every RuitkRouter.h hook.
    file: 'RuitkUnrealDocs~/src/hooksCatalog.ts',
    pattern: /(\d+) router hook entries/,
    source: () => {
      const registry = countRouterHooks();
      const catalog = countHooksCatalog('router');
      return mustAgree('router hooks', registry, catalog);
    },
  },
];

let failures = 0;
for (const check of CHECKS) {
  const filePath = resolve(REPO_ROOT, check.file);
  if (!existsSync(filePath)) {
    console.error(`✗ ${check.file}: file missing`);
    failures++;
    continue;
  }
  const text = readFileSync(filePath, 'utf8');
  const m = text.match(check.pattern);
  if (!m) {
    console.error(`✗ ${check.file}: pattern ${check.pattern} not found (claim removed? update CHECKS)`);
    failures++;
    continue;
  }
  const claimed = parseInt(m[1], 10);
  const actual = check.source();
  if (claimed !== actual) {
    console.error(`✗ ${check.file}: claims ${claimed} but the registry says ${actual} (${check.pattern})`);
    failures++;
  } else {
    console.error(`✓ ${check.file}: ${claimed} matches the registry`);
  }
}

if (failures) {
  console.error(`\ndocs-drift FAILED (${failures}).`);
  process.exit(1);
}
console.error(`✓ docs-drift OK (${CHECKS.length} check(s) configured).`);
