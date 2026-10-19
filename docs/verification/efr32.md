# EFR32 hardware verification — results and iteration log

Running record of what was tried, what broke, and what was fixed, for
the Silicon Labs EFR32 (BRD2601B) hardware-verification pipeline. See
`CLAUDE.md` for commands/conventions and current phase status.

## Board / SDK

- Board: EFR32xG24 Dev Kit, BRD2601B, part EFR32MG24B310F1536IM48.
- Target macro: `TARGET_SILABS_SLGPIO` (current `sl_gpio` driver, not
  classic emlib) — confirmed with the maintainer.
- Simplicity SDK pin: `2026.6.1` (latest patch of the June-2026 LTS
  baseline at the time of writing) — confirmed with the maintainer.

## Entry 1 — Phase 1/2 written, not yet validated anywhere

**What was built:** `verify/targets.yml`, `docker/efr32/Dockerfile`,
`docker/efr32/build_in_container.sh`, `tests/efr32/smoke/main.c`,
`scripts/build.sh`, `.github/workflows/build.yml`.

**Why it's unvalidated:** this development sandbox's network egress
proxy refuses all `silabs.com` connections outright —

```
$ curl -sS -o /dev/null -w "%{http_code}\n" https://www.silabs.com/
curl: (56) CONNECT tunnel failed, response 403
[agent-proxy] ... docs.silabs.com:443 — connect_rejected (... host is
not allowed for this session)
```

— confirmed identically via plain `curl`, via the Docker daemon's own
build network (same proxy config), and via an independent fetch tool
(which got a DNS failure on its own separate network path). This
blocks:

1. Downloading `slt-cli` itself (`docker/efr32/Dockerfile`'s first
   `RUN curl ...` step).
2. `slt install`'ing the pinned Simplicity SDK (same Dockerfile).
3. Running `slc generate` locally against a real SDK example to build
   `tests/efr32/smoke/`'s project scaffolding — meaning that file's
   exact include paths/macro names for the board's LED pin
   (`SL_SIMPLE_LED_LED0_PORT`/`_PIN`) are a flagged placeholder guess,
   not a confirmed real symbol (see that file's own header comment).

Checked and ruled out as workarounds, rather than left unconsidered:
- `github.com/SiliconLabs/gecko_sdk` (the pre-2025.12 GitHub SDK
  mirror) is reachable, but Silicon Labs' own docs call it "reference
  only" since the 2025.12 Conan-based delivery change, and it's frozen
  at an older Gecko-SDK-branded release — using it would silently
  substitute a different SDK than the `2026.6.1` pin the maintainer
  confirmed. Not used, for that reason.
- No official or community Docker image bundling Simplicity SDK +
  slc-cli + gcc-arm-none-eabi was found.
- No non-`silabs.com`-hosted Conan/package remote was found.

**Decision (confirmed with the maintainer):** write everything now,
validate via the first real GitHub Actions run — Actions runners have
ordinary internet access this sandbox doesn't. Every genuinely
unconfirmed specific in the files above is marked `EDIT-ME` with the
exact command to run or check once real SDK/slc-cli access exists, so
the next iteration is a short, targeted fix, not a redesign.

**Next entry will be:** the first real `docker build`/CI log, and
whatever `EDIT-ME` it resolves or new issue it surfaces.
