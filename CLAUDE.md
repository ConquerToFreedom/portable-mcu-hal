# Hardware verification pipeline — working notes

This file is for whoever (human or Claude Code session) picks up the
hardware-verification pipeline next. It does NOT describe the wrapper
itself — see `README.md`/`docs/architecture.md` for that. Nothing here
authorizes editing `stm/`, `nrf/`, `silicon_labs/`, `ti/`, or
`include/` — a test finding a wrapper bug gets reported, never fixed,
without the maintainer's explicit go-ahead.

## Status (as of this writing)

**Phase 1/2 (build-only, no hardware) are written but not yet
validated by a real build anywhere.** This development sandbox's
network egress proxy refuses all `silabs.com` connections (confirmed:
`curl` gets `403 connect_rejected ... host is not allowed for this
session`; same result through Docker's build network and through an
independent fetch tool). That blocks `slt-cli`/the Simplicity SDK
download, which is the one thing `docker/efr32/Dockerfile` needs most.
GitHub Actions runners have ordinary internet access and are expected
to reach `silabs.com` fine — so the actual first real signal on
whether this Dockerfile/build script work comes from a pushed commit's
Actions run, not from this sandbox. See `docs/verification/efr32.md`
for the running log of what broke and what was fixed, each time.

**Phase 3 (flash + capture through HF·NODE) has not started.** It
needs, from the maintainer, none of which exist yet in this repo or
session:
- `HFNODE_API_URL` / `HFNODE_API_KEY` environment variables.
- A description of the HF·NODE API itself (flash/logs/status
  endpoints, auth header shape, request/response bodies) — the hard
  rule from the original brief is "never guess HF·NODE endpoints,"
  so `scripts/hfnode.sh`/`fetch_build.sh`/`verify.sh` are not written
  until this is supplied.
- The HF·NODE target id for the BRD2601B board being used.

**Phase 4 (I2C baseline vs. wrapper)** additionally needs the exact
I2C sensor + 7-bit address on the board — not yet supplied.

## Hard rules (apply to every future session working on this pipeline)

- Never modify `stm/`, `nrf/`, `silicon_labs/`, `ti/`, `include/`
  (the wrapper). A test exposing a wrapper bug is reported with
  evidence, not patched — the maintainer decides whether/how.
- Never edit files inside a vendor SDK or example repo. Copy what's
  needed into `tests/<vendor>/<test>/`, and keep the original vendor
  license header on anything genuinely copied from a vendor example.
  Do NOT add a vendor license header to hand-written automation code
  that wasn't actually copied from the vendor (a real mistake made and
  fixed once already in this pipeline's own `tests/efr32/smoke/
  main.c` — see its header comment).
- Never commit a vendor SDK, a binary, or a secret. `HFNODE_API_KEY`
  only ever comes from the environment variable — never printed,
  logged, or echoed by any script here.
- The GitHub Actions workflow (`.github/workflows/build.yml`) builds
  firmware only. No secrets referenced anywhere in it, no flashing
  step, ever — flashing/verification is a local script run by a human
  or Claude Code session with HF·NODE credentials, never CI.
- Never change a test's expectation or a golden log to make a test
  pass. A mismatch between captured output and golden is a finding to
  report, not something to edit away.
- Test firmware logs over the board's own native serial/iostream path
  — never through `portable-mcu-hal`'s own UART adapter — so a
  peripheral bug in the thing being tested can't masquerade as (or
  hide behind) a logging bug in that same wrapper.
- An SDK version pin (`verify/targets.yml`'s `sdk_version`) changes
  only when a human deliberately decides to bump it — never silently.

## Environment / commands

- `HFNODE_API_URL`, `HFNODE_API_KEY` — not yet set in any session that
  has worked on this; Phase 3 needs them.
- `verify/targets.yml` — the one file that describes every vendor:
  board, SDK version, Docker image, target macro, test list. Adding a
  vendor should only ever mean: a new `docker/<vendor>/Dockerfile`, new
  `tests/<vendor>/<test>/` folders, and a new entry here — no script
  changes.
- `scripts/build.sh <vendor> <test> <short_sha>` — builds one test's
  firmware inside its pinned Docker image, writes
  `out/<vendor>/<test>.{hex,elf,map}`. Runs identically locally and in
  CI (CI sets `DOCKER_BUILD_EXTRA_ARGS` for layer caching; everything
  else is the same command).
- `.github/workflows/build.yml` — matrix over `verify/targets.yml`,
  runs `scripts/build.sh` per vendor × test, uploads the artifact. No
  secrets, no flashing, runs on `push` and `pull_request`.
- Phase 3+ commands (`scripts/fetch_build.sh`, `scripts/hfnode.sh`,
  `scripts/compare.sh`, `scripts/verify.sh <vendor> <test>`) don't
  exist yet — see Status above.

## Adding a new peripheral test (once Phase 3/4 exist)

1. `tests/<vendor>/<test_name>/` — hand-written `main.c` (or adapted
   from a real vendor example, with its license header kept), the
   `[BOOT]`/structured-op/`[DONE]` log-line conventions from the
   original brief, calling the wrapper API being verified.
2. Add the test to its vendor's `tests:` list in `verify/targets.yml`.
3. `scripts/build.sh <vendor> <test_name> <sha>` to confirm it builds.
4. Once it builds in CI, run `scripts/verify.sh <vendor> <test_name>`
   on real hardware, show the maintainer the captured log, and only
   after their explicit approval save it as
   `tests/<vendor>/golden/<test_name>.log`.

## Adding a new vendor (STM32 / nRF52 / TI MSPM0)

1. `docker/<vendor>/Dockerfile` — pinned vendor SDK + toolchain,
   mirroring `docker/efr32/Dockerfile`'s structure.
2. A new entry under `targets:` in `verify/targets.yml`.
3. `tests/<vendor>/smoke/` following the same conventions as
   `tests/efr32/smoke/`.
4. No changes to `scripts/build.sh`/the workflow should be needed —
   if one turns out to be, that's a sign something was hardcoded to
   `efr32` that shouldn't have been; fix the script, not the pattern.
