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
# No output redirection here on purpose: `slt install` only prints its
# readable "Installed Packages" summary when it detects a TTY (see
# step 3's own note below, found the hard way) — redirecting would
# both lose that summary from the build log AND gain nothing, since
# step 3 no longer parses it anyway.
RUN slt install -f /tmp/recipe.toml --non-interactive

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
# just `{"lock": "/tmp/recipe.lock"}`, not inline package details.
#
# CONFIRMED-WRONG (fourth CI run): redirecting `slt install`'s plain-
# text output to a file (`> /tmp/install.log`) also doesn't work — the
# "Installed Packages:" Id/Version/Path summary block seen directly in
# BuildKit's own captured RUN output (runs 2 and 3) never reaches the
# file; `slt` evidently only prints that pretty summary when it
# detects a TTY, and suppresses it when stdout is redirected. Parsing
# the file found nothing again.
#
# FIX: stop depending on slt's own summary output entirely. Instead,
# search the filesystem SLT actually populated for real, known marker
# files/dirs: `bin/arm-none-eabi-gcc` for the gcc-arm-none-eabi root,
# an `slc` executable for slc-cli's bin dir, and `platform/driver/gpio`
# for the Simplicity SDK root — the last one isn't a guess, it's a
# path this project's own silicon_labs/gpio/hal_port_silabs_slgpio.c
# already confirmed real against the SDK source during the wrapper's
# own development (see that file's header comment).
# ---------------------------------------------------------------------
RUN python3 - <<'PYEOF'
import subprocess, os

INSTALL_ROOT = '/root/.silabs/slt/installs'

def find_first(args):
    out = subprocess.run(args, capture_output=True, text=True).stdout
    lines = [l for l in out.splitlines() if l.strip()]
    return lines[0] if lines else None

gcc_bin = find_first(['find', INSTALL_ROOT, '-maxdepth', '8', '-type', 'f', '-name', 'arm-none-eabi-gcc'])
slc_bin = find_first(['find', INSTALL_ROOT, '-maxdepth', '8', '-type', 'f', '-name', 'slc'])
gpio_dir = find_first(['find', INSTALL_ROOT, '-maxdepth', '10', '-type', 'd', '-path', '*/platform/driver/gpio'])

paths = {}
if gcc_bin:
    gcc_bin_dir = os.path.dirname(gcc_bin)
    paths['GCC_BIN_DIR'] = gcc_bin_dir
    paths['GCC_ARM_NONE_EABI_PATH'] = os.path.dirname(gcc_bin_dir)
if slc_bin:
    paths['SLC_BIN_DIR'] = os.path.dirname(slc_bin)
if gpio_dir:
    sdk_root = gpio_dir
    for _ in range(3):
        sdk_root = os.path.dirname(sdk_root)
    paths['SIMPLICITY_SDK_PATH'] = sdk_root

with open('/opt/slt/paths.env', 'w') as f:
    for k, v in paths.items():
        f.write(f'export {k}="{v}"\n')
    f.write('export PATH="${SLC_BIN_DIR:-}:${GCC_BIN_DIR:-}:$PATH"\n')

print('--- discovered paths ---')
print(paths)
with open('/opt/slt/paths.env') as f:
    print(f.read())
if not (gcc_bin and slc_bin and gpio_dir):
    raise SystemExit(
        f'one or more markers not found: gcc_bin={gcc_bin!r} slc_bin={slc_bin!r} gpio_dir={gpio_dir!r}'
    )
PYEOF

RUN . /opt/slt/paths.env && slc configuration \
      --sdk="${SIMPLICITY_SDK_PATH}" \
      --gcc-toolchain="${GCC_ARM_NONE_EABI_PATH}"

WORKDIR /work
ENTRYPOINT ["/bin/bash"]
