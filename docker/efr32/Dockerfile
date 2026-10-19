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
        ca-certificates curl unzip git make python3 bzip2 xz-utils \
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
# confirmed.
#
# CONFIRMED (first real CI run, 2026-10-19): `slt install --help`'s
# real flags are --check-updates/-e/-f/--force/--no-lock/-s plus the
# GLOBAL flags --json/--non-interactive/--token-file/--version — there
# is no --accept-licenses or --yes. Using --non-interactive instead.
# CONFIRMED (second CI run, same day): --non-interactive alone is
# enough — no token/account/EULA prompt at all. It installed
# simplicity-sdk 2026.6.1 (the exact pin), slc-cli 6.0.25,
# gcc-arm-none-eabi 14.2.rel1, cmake 3.30.2, ninja 1.12.1 successfully;
# the only failure was `commander` needing `bzip2` on the base image
# (fixed above, in the apt-get install line).
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
# Redirect-then-cat (not `| tee`) deliberately: piping through tee
# would make this RUN step's exit code tee's (always 0), silently
# hiding a real `slt install` failure from `docker build`.
RUN slt install -f /tmp/recipe.toml --non-interactive > /tmp/install.log 2>&1; \
    ec=$?; cat /tmp/install.log; exit $ec

# ---------------------------------------------------------------------
# 3. Discover where SLT actually put things and configure slc-cli.
#
# CONFIRMED (second CI run): SLT installs packages under
# /root/.silabs/slt/installs/{conan,archive}/... with a hashed,
# non-predictable path per package (e.g.
# /root/.silabs/slt/installs/conan/p/simpleca33d691c539/p for
# simplicity-sdk) — NOT the flat ${SLT_HOME}/<pkg-name> layout this
# file originally guessed.
#
# CONFIRMED-WRONG (third CI run): `slt install --json`'s output is
# NOT the same Id/Version/Path block as the plain-text output — it's
# just `{"lock": "/tmp/recipe.lock"}`, a pointer to a lock file, not
# inline package details. Parsing it found nothing, left paths.env
# empty, and `slc configuration` failed with "slc: not found" since
# its own install dir never made it onto PATH.
#
# FIX: `tee` the ORIGINAL plain-text install log (step 2 above, now
# confirmed twice to contain real "  Id: <x>" / "  Path: <y>" blocks)
# instead of relying on --json at all, and parse that line-by-line:
# whenever an "Id:" line is seen, remember it as the current package;
# the next "Path:" line is paired with it. `docker/efr32/
# build_in_container.sh` sources the resulting /opt/slt/paths.env.
# ---------------------------------------------------------------------
RUN python3 - <<'PYEOF'
import re

with open('/tmp/install.log') as f:
    lines = f.readlines()

paths = {}
current_id = None
for line in lines:
    m = re.match(r'\s*Id:\s*(\S+)', line)
    if m:
        current_id = m.group(1)
        continue
    m = re.match(r'\s*Path:\s*(\S+)', line)
    if m and current_id:
        paths[current_id] = m.group(1)
        current_id = None

with open('/opt/slt/paths.env', 'w') as f:
    for ident, path in paths.items():
        var = ident.upper().replace('-', '_')
        f.write(f'export {var}_PATH="{path}"\n')
    f.write('export PATH="${SLC_CLI_PATH:-}/bin:${GCC_ARM_NONE_EABI_PATH:-}/bin:$PATH"\n')

print('--- parsed package paths ---')
print(paths)
print('--- /opt/slt/paths.env ---')
with open('/opt/slt/paths.env') as f:
    print(f.read())
PYEOF

RUN . /opt/slt/paths.env && slc configuration \
      --sdk="${SIMPLICITY_SDK_PATH}" \
      --gcc-toolchain="${GCC_ARM_NONE_EABI_PATH}"

WORKDIR /work
ENTRYPOINT ["/bin/bash"]
