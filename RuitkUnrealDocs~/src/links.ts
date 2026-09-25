/** Canonical community Discord invite — the single source every page/component should import
 * this from, rather than hard-coding the invite URL. */
export const DISCORD_INVITE_URL = 'https://discord.gg/Knedqu4Wyv'

/** The ruitk-unreal GitHub repository. */
export const GITHUB_URL = 'https://github.com/reactive-ui-toolkit/ruitk-unreal'

/** The ref every repo-file link below is pinned to.
 *
 * `master` rather than `HEAD`, for two reasons. It is what a READER of a published docs site
 * should see — `master` is release-only (CLAUDE.md: fast-forwarded from `dev`), so it holds the
 * state that matches the plugin version this site documents, where `HEAD`/`dev` can be weeks
 * ahead of it. And it was already the convention: the two licence links used `master` while the
 * roadmap link used `HEAD`, which is one site speaking with two voices about the same repo. */
const REPO_REF = 'master'

/** A file in the repository, at {@link REPO_REF}.
 *
 * Every repo-file URL on the site goes through here so the ref is stated once. Hand-built
 * `${GITHUB_URL}/blob/…` strings are what let the two conventions diverge in the first place. */
export const repoFile = (path: string) => `${GITHUB_URL}/blob/${REPO_REF}/${path}`

/** The Community Licence, and the commercial terms beside it. */
export const LICENSE_URL = repoFile('LICENSE')
export const LICENSE_COMMERCIAL_URL = repoFile('LICENSE-COMMERCIAL.md')

/** The 0.15 rename migration guide (the `Rui*` → `Ruitk*` sweep). */
export const MIGRATION_0_15_URL = repoFile('MIGRATION-0.15.md')

/** The living status document in the repo.
 *
 * NOTE for page authors: the site has its OWN Roadmap page (`/roadmap`), and that is what a
 * reader should normally be sent to — it is this document, rendered, without leaving the docs.
 * This URL is for the places that genuinely mean "the file in the repository". */
export const ROADMAP_URL = repoFile('plans/ROADMAP.md')
