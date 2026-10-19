# Build container for portable-mcu-hal's EFR32 (Silicon Labs) hardware
# verification firmware. Builds ONLY — this image never flashes a
# board or talks to HF·NODE (see ../../scripts/hfnode.sh for that).
#
# PROVENANCE / CONFIDENCE NOTE: this Dockerfile was authored without
# the ability to actually run it — this development sandbox's network
# egress proxy refuses all silabs.com connections (confirmed via
# direct curl: "403 connect_rejected ... host is not allowed for this
# session"), which is the one place slt-cli, the Simplicity SDK, and
# gcc-arm-none-eabi (via SLT) all come from. GitHub Actions runners
# have normal internet access and reach silabs.com fine, so this image
# is expected to need iteration against real `docker build` output
# there — see docs/verification/efr32.md for the running log of what
# changed and why. Every line below that isn't independently confirmed
# against Silicon Labs' own docs is marked EDIT-ME.

FROM ubuntu:24.04

ARG SDK_VERSION=2026.6.1
# EDIT-ME: real current SLT-CLI Linux release version/URL not
# confirmed from inside this sandbox. Search-engine snippets (not a
# direct fetch of silabs.com, which is blocked here) point at
# https://www.silabs.com/documents/public/software/slt-cli-<version>-linux-x64.zip
# with a Linux x64 installer reported at v1.2.2 "current" as of this
# writing — used as the starting guess below. Fix this ARG from the
# real download page the first time this fails in CI.
ARG SLT_CLI_VERSION=1.2.2

ENV DEBIAN_FRONTEND=noninteractive \
    SLT_HOME=/opt/slt \
    SISDK=/opt/slt/simplicity-sdk \
    PATH=/opt/slt:/opt/slt/bin:${PATH}

RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates curl unzip git make python3 \
    && rm -rf /var/lib/apt/lists/*

# ---------------------------------------------------------------------
# 1. SLT-CLI itself.
# EDIT-ME: exact zip layout (does it extract an `slt` binary at the
# zip root, or a subdirectory?) is unconfirmed — adjust the `mv`/`ln`
# below once the real zip has been inspected.
# ---------------------------------------------------------------------
RUN mkdir -p ${SLT_HOME} && cd ${SLT_HOME} \
    && curl -fsSL -o slt-cli.zip \
       "https://www.silabs.com/documents/public/software/slt-cli-${SLT_CLI_VERSION}-linux-x64.zip" \
    && unzip -q slt-cli.zip && rm slt-cli.zip \
    && chmod +x ${SLT_HOME}/slt* 2>/dev/null || true \
    && slt --version

# ---------------------------------------------------------------------
# 2. Pin + install the Simplicity SDK and build tools via SLT's recipe
# mechanism, NOT git clone (Silicon Labs deprecated GitHub-based SDK
# delivery as of the 2025.12 release — see docs/verification/efr32.md).
#
# EDIT-ME: recipe TOML syntax below follows the one documented example
# this project's research turned up (a `[dependency]` table with
# `pkg = "version-or-range"` entries); the exact version-PIN syntax
# (bare version string vs. a range operator) is NOT independently
# confirmed. EDIT-ME: non-interactive EULA/license acceptance for
# `slt install` is not documented anywhere this research reached —
# the --accept-licenses flag below is a guess by analogy with similar
# vendor tools (Qt, Android sdkmanager), not a confirmed slt flag. If
# CI hangs or fails on a license prompt, this is the line to fix.
# ---------------------------------------------------------------------
RUN cat > /tmp/recipe.toml <<EOF
[dependency]
simplicity-sdk = "${SDK_VERSION}"
slc-cli = "~"
gcc-arm-none-eabi = "~"
cmake = "~"
ninja = "~"
commander = "~"
EOF
RUN slt install -f /tmp/recipe.toml --accept-licenses --yes

# ---------------------------------------------------------------------
# 3. One-time slc-cli configuration: point it at the installed SDK and
# the gcc-arm-none-eabi toolchain SLT just installed.
# EDIT-ME: exact installed paths below are guesses (SLT's own install
# layout under $SLT_HOME) — confirm with `slt list --installed` (or
# equivalent) once this runs for real, and fix the two --sdk/
# --gcc-toolchain paths below.
# ---------------------------------------------------------------------
RUN slc configuration \
      --sdk="${SLT_HOME}/simplicity-sdk" \
      --gcc-toolchain="${SLT_HOME}/gcc-arm-none-eabi"

WORKDIR /work
ENTRYPOINT ["/bin/bash"]
