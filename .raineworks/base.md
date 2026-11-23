<!-- Copy of base.md from raine-works/agents, kept current by its sync. Change base.md there, not this file. -->

# Raineworks base rules for agents

These rules apply to every `raine-works` repository, in any language, for any agent (Claude Code, Codex, Cursor,
Copilot, CI review bots) and for people. They are the floor: a repository's own `AGENTS.md` adds rules for its stack
and may be stricter, and where it's more specific than this file, it wins. Where nothing covers a case, follow the
code that's already there.

Before you change anything, work out what kind of project you're in (see [Adapt to the project](#adapt-to-the-project))
and apply the rules for every language and tool it uses.

---

## Git: always branch

- The default branch is **`master`**, never `main`. A new repository starts on `master` (`git init -b master`).
- Never commit to `master`. Start every change on a new branch off an up-to-date `master`, named for what it does
  (`fix/…`, `feat/…`, `chore/…`).
- Keep commits focused, with an imperative subject line ("Add …", "Fix …"). Commit only what the change needs.
- Agents commit only when asked: by a person, or by an automated run's instructions. Being asked for a change, a
  pull request or to handle a review's findings counts as being asked.
- Never push to `master`, force-push a branch someone else works on, or rewrite history that's been pushed.
- Never commit secrets, `.env*` files (except committed examples such as `.env.example`), build output or editor files.

### Protect `master`

- `master` is protected on GitHub by rulesets kept in `raine-works/agents` (`rulesets/`, applied by
  `scripts/apply-rulesets.sh`): changes go in only through a pull request with an approval of its last push, every
  review thread resolved, and the review and the repository's own checks passing.
- The owner can bypass the rulesets to merge a pull request, never to push.

## Pull requests and code review

Every change reaches `master` through a pull request. A pull request isn't done when it's opened.

1. Open it with a description of what changed, why, and how it was tested (the real results).
2. **Wait for every check and the code review to finish** (`gh pr checks <number> --watch`): the repository's CI and
   Claude's code review. Don't report the pull request ready, and don't stop, while any is running. If none start,
   find out why (a merge conflict stops them).
3. **Handle everything they raise.** Fix each failing check. For each review finding, fix it in a new commit, or, when
   it's wrong, reply in its thread saying why. Never leave a finding unanswered or resolve it without a reply: the
   review resolves its own threads once it sees them fixed or answered.
4. Push the fixes and wait again. Repeat until every check passes and the review has nothing open.
5. Only then say it's ready, with what the checks and the review found and what you did about each.

- When Claude's review of the latest commit has nothing open, it approves the pull request: that's the approval
  `master` requires. A push dismisses it, and the next review gives it again.
- Agents never merge a pull request: the owner does. Never skip, disable or weaken a check, a review or a ruleset to
  get one through.
- If `master` moves, bring it into the branch and resolve the conflicts; if it already contains the change,
  say so instead of merging a duplicate.
- A pull request that changes `.github/workflows/code-review.yml` can't be reviewed by Claude (only `master`'s copy
  runs): its review fails on purpose, and a person reviews it. Change the review workflow in a pull request of its own.

### Required workflows

Every `raine-works` repository runs these, and none keeps its own copy of them: each is a reusable workflow in
`raine-works/agents` (`.github/workflows/`), and the repository's `.github/workflows/` holds only a short file that
calls it, pinned to a commit, from the `raineworks-base` skill's `templates/workflows/`. A repository sets only the
caller's inputs (`with:`); anything else changes in `raine-works/agents`, for every repository at once. When you work
in a repository, check the callers are there. If one is missing, or the repository has its own copy of one of these
workflows, tell the person and offer to add the caller in its own pull request; don't fold it into an unrelated change.

- **Base rules check** (`check-base.yml`): fails when `AGENTS.md` doesn't import `.raineworks/base.md`, `CLAUDE.md`
  isn't a symlink to it, a caller is missing, or `.raineworks/base.md` isn't the current `base.md` from
  `raine-works/agents`. On `master` it opens a pull request with the current copy (and stays failed until it's
  merged), and another moving the callers' pins when the shared workflows change. Merge them; never edit the copy or
  the pins by hand.
- **Claude code review** (`code-review.yml`): on every pull request, Claude reviews the diff against this file and the
  repository's `AGENTS.md`, posts its findings, and approves once none is left open. Handle them as in the steps above.
- **Docs sync** (`docs-sync.yml`): after `master`'s CI passes (or its Base rules check, where there's no CI), Claude checks
  whether what's new on `master` made any of the repository's docs wrong (see [Documentation](#documentation)) and,
  if so, opens or updates one pull request fixing them. It never pushes to `master`. It backs up the rule to update
  docs in the same pull request; it doesn't replace it.
- **Rules audit** (`rules-audit.yml`): weekly, and when the rules change, Claude audits `master` against this file
  and the repository's own rules, and keeps one issue labelled `rules-audit` open while it finds something they're
  broken by. Fix each finding in a pull request; the next audit closes the issue once there's nothing left.

## Verify, and report what really happened

- Before you say you're done, run the project's lint, type check and tests (whatever it has) and report the real
  result. A failure is reported with its output, even when it isn't yours; say whether it also fails without your
  change.
- Say what you didn't do or couldn't check. Never present a guess as a fact or a skipped step as done.

## Tests

- **Every change comes with tests** where the project has them: new code with tests of its own, a bug fix with a test
  that fails without the fix. Tests sit next to the code they cover, in the project's test framework.
- Test behaviour through the public interface (an endpoint over HTTP, a function's exports, an integration's
  entities), including the failure paths and the edge cases, not the implementation's internals. Security-sensitive
  behaviour (access checks, validation) always gets an explicit test.
- Where a project measures coverage, never lower its threshold, or exclude code from it, to get a change through.

## Documentation

Document as you go, in the same change as the code:

- **Inline.** Every module, and every exported function, type, class and constant, gets a doc comment in the
  language's standard form (TSDoc / JSDoc, Python docstrings, Go doc comments, rustdoc): what it's for, its inputs and
  results, what it throws or returns on failure, and anything a caller couldn't guess. Comments say *why*, not what
  the next line does.
- **In `docs/`.** Every repository has a `docs/` folder for what doesn't belong next to a single file: installing and
  using it, how the parts fit, design decisions and their reasons, development (setting up, running, testing,
  releasing). `docs/README.md` is its index, a table of its pages (raineworks.com shows each project's docs from it).
  The root `README.md` orients (what it is, how to start, a link to the docs).
- **Keep them true.** A change that alters behaviour described in a doc updates that doc in the same pull request. If
  a doc disagrees with the code, trust the code and fix the doc.
- `AGENTS.md` holds rules, briefly; the reasoning and how-tos live in `docs/`, and `AGENTS.md` links to them.
- These base rules are copied into each repository as `.raineworks/base.md`, which `AGENTS.md` links to and imports
  (`@.raineworks/base.md` on a line of its own), so `AGENTS.md` holds only the repository's own rules. `CLAUDE.md` is
  a symlink to `AGENTS.md`, so every agent reads the same rules. Never edit `.raineworks/base.md`: change `base.md` in
  `raine-works/agents`.

## Dependencies: always pinned

- **Pin every dependency to an exact version** where the ecosystem allows: no ranges (`^`, `~`, `>=`, `*`, `latest`).
  Commit the lockfile.
- Pin what runs the code too: the language and package manager version, container base images (by digest), and
  GitHub Actions (by commit SHA, with the version in a comment).
- Before adding a dependency, prefer the language's standard library or what the project already uses. Say what a new
  one pulls in, and use a maintained, stable release.
- Upgrade on purpose, in a change of its own, and read the changelog for breaking changes.

## Security

- Secrets come from the environment or a secret manager, never source or test fixtures.
- **Never expose a secret** in a log, an error, CI output, a command line, a commit, a pull request or a comment. Log
  that something happened, never the value. Agents don't print, echo or `cat` secrets (`.env*` files, tokens, keys)
  to read or check them; check they're set without showing them.
- If a secret is exposed anyway (logged, committed, pasted), treat it as compromised: tell a person, rotate it, and
  then remove what's left of it.
- **Jobs that run a pull request's code get no secrets.** A `pull_request` job that installs, builds or tests the
  branch gets only a read-only `GITHUB_TOKEN`. Jobs that need secrets (publishing, deploying) run after merge, or use
  cloud identity (OIDC) whose conditions exclude pull request branches.
- Give each workflow and job the least permissions it needs. Claude, in a workflow, never gets a token or a tool beyond
  its job.
- Validate everything that comes from outside (requests, files, webhooks, devices, environment) at the boundary.
- Anything hard to undo, or visible outside the repository (publishing, releasing, sending messages, deleting data,
  changing permissions), needs a person's go-ahead first, unless the repository's rules say otherwise. Releases, tags
  and deploys are made by CI, never by hand.

## Working with AI

- **What may be shared.** Never paste personal data, credentials or secrets into an AI tool, prompt or external
  service. An issue handed to an agent counts as shared with it.
- **The owner owns the work.** Whoever starts an agent owns what it produces. Claude's review approves pull requests;
  the owner merges them, and reads what matters before doing so.
- **Say when AI did the work.** Commits and pull requests made by an agent say so (e.g. a `Co-Authored-By` line).
- **Provenance.** Don't bring in code an AI reproduced from a source whose licence is unclear.

## Adapt to the project

Work out the stack from the files in the repository, and apply each section that matches. A repository can match
several. Existing projects keep their toolchain unless changing it is the task; when the repository's own `AGENTS.md`
or tooling disagrees with a default below, the repository wins, except on pinning, which is never relaxed.

| Found | Stack |
| --- | --- |
| `package.json`, `bun.lock` | JavaScript / TypeScript |
| `pyproject.toml`, `requirements*.txt`, `uv.lock`, `custom_components/` | Python (and Home Assistant integrations) |
| ESPHome YAML, `components/` with `__init__.py` and C++ | ESPHome firmware |
| `go.mod` | Go |
| `Cargo.toml` | Rust |
| `Dockerfile`, `compose*.yml`, devcontainer features | Infrastructure |
| shell scripts | Shell |
| anything else | the ecosystem's standard tools, by the same rules |

### JavaScript / TypeScript

- **New projects** use the latest stable release of **Bun** (runtime, package manager, test runner, bundler) and of
  **TypeScript**, in strict mode, checked by `bun check`. Pin Bun's version (`packageManager` in `package.json`, and
  in CI and images).
- Pin exact versions (`"1.2.3"`) and set installs to exact (`bunfig.toml`: `[install] exact = true`). Commit
  `bun.lock`. Only one package manager and one lockfile per repository. Monorepos keep every version once, in the root
  `package.json`'s catalog.
- **Dev tools are Bun's own**: `bun check` type checks, `bun fmt` formats, `bun test` tests, `Bun.build` bundles. Don't
  add `tsc`, ESLint, Prettier, Biome, Jest, Vitest or another bundler.
- Prefer Bun's built-ins (`Bun.serve`, `bun:sql`, `bun:test`, `Bun.build`, …) over packages that do the same job.

### Python and Home Assistant

- Pin the Python version and every requirement (`==`, or a committed `uv.lock`). Type hints throughout; format and
  lint with the project's tool (`ruff` by default); test with `pytest`.
- A Home Assistant integration follows Home Assistant's own rules: it passes `hassfest` and HACS validation, its
  `manifest.json` version is bumped for a release, and it does no blocking I/O in the event loop.

### ESPHome firmware

- Pin ESPHome and every external component to a version or commit. Keep real-time callbacks free of blocking work,
  allocation and unthrottled logging; keep large buffers out of internal RAM where the board has PSRAM.

### Go

- Set `go` and `toolchain` in `go.mod`, commit `go.sum`, run `gofmt` / `go vet`, and `go test ./...`.

### Rust

- Pin the toolchain in `rust-toolchain.toml`, commit `Cargo.lock`, and keep `cargo fmt --check`, `cargo clippy` and
  `cargo test` clean.

### Shell

- `#!/usr/bin/env bash` with `set -euo pipefail`, quoted variables, and `shellcheck` clean. Scripts that change a
  machine are safe to run again.

### Infrastructure

- **Runtime images are distroless** wherever possible, built multi-stage: compile in a full build image, copy only the
  output into the distroless one. Where distroless can't work, use the smallest image that does and say why in the
  Dockerfile.
- Every base image pinned by digest; containers run as non-root, with no shell or package manager in the runtime
  image where it can be avoided.
