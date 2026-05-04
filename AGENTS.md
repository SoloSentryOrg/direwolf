# Codex Project Instructions

## Communication

- Use BLUF format.
- Use concise bullets.
- Prioritize the top five most important items.

## Project Rules

- Treat radio frames, APRS packets, KISS/AGW/TCP client data, APRS-IS data,
  audio decoder output, files, command-line arguments, configuration files, and
  telemetry toolkit inputs as untrusted.
- Preserve protocol bytes until the relevant parser or decoder boundary has
  validated framing, length, escaping, and character-set expectations.
- Fail closed on malformed packets, malformed frames, unsafe configuration, and
  invalid network input. Do not silently normalize attacker-controlled input in
  security-sensitive paths.
- Keep input bounds explicit for packet, frame, line, file, audio-buffer, and
  network-read handling. Avoid unbounded copies, unchecked indexes, integer
  overflows, and unsafe string formatting.
- Follow leading practice secure coding standards for C and CMake, including
  OWASP-aligned input validation, safe output handling, least privilege, secure
  defaults, and defense in depth.

## Workflow

- Use feature branches with the `codex/` prefix for Codex changes.
- Keep changes narrowly scoped and aligned with upstream Dire Wolf conventions.
- Prefer small, reviewable patches over broad rewrites.
- Do not commit generated build trees, local configuration containing station
  details, audio captures, packet logs, credentials, API tokens, registry
  caches, or temporary tool state.
- Prefer `scripts/verify.sh` for local verification before commit, push, or
  release work.
- Before changing build, packaging, or release behavior, inspect the existing
  CMake, package, and GitHub Actions flow and preserve supported platforms
  unless the user explicitly approves a support change.

## Sub-Agent Use

- Sub-agents are allowed when they provide clear benefit, such as focused
  review, independent investigation, parallel checks, or bounded
  implementation.
- Use sub-agents only with explicit ownership, narrow scope, and clear expected
  outputs.
- Do not use sub-agents for tightly coupled edits, urgent blocking work,
  ambiguous tasks, or changes likely to create merge conflicts.
- For code changes, assign disjoint write scopes and require changed file paths
  in the sub-agent summary.
- Verify sub-agent findings and changes before relying on them, committing, or
  merging.

## Secure Review And Main Branch Gate

- Before pushing or merging to `main` or `master`, perform a secure code review.
- Do not commit directly to `main` or `master` when secure review findings
  remain open.
- If secure review finds issues, document the findings, propose fixes, and ask
  for a decision on next steps before proceeding.
- Security-sensitive changes should include focused regression tests or fixtures
  where practical, especially for packet parsing, frame decoding, configuration
  parsing, file input, and network input.
- Treat CodeQL, compiler warnings, sanitizer findings, secret scans, and
  dependency advisories as release-blocking until reviewed and either fixed or
  explicitly accepted by the maintainer.
