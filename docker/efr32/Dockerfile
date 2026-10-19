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
RUN slt install -f /tmp/recipe.toml --non-interactive

# ---------------------------------------------------------------------
# 3. Discover where SLT actually put things and configure slc-cli.
#
# CONFIRMED (second CI run): SLT installs packages under
# /root/.silabs/slt/installs/{conan,archive}/... with a hashed,
# non-predictable path per package (e.g.
# /root/.silabs/slt/installs/conan/p/simpleca33d691c539/p for
# simplicity-sdk) — NOT the flat ${SLT_HOME}/<pkg-name> layout this
# file originally guessed. `slt install`'s plain-text output lists
# each package's real Id/Version/Path, so re-running it with the
# documented global `--json` flag and parsing that is the reliable way
# to find these paths rather than hardcoding them. `docker/efr32/
# build_in_container.sh` sources the resulting /opt/slt/paths.env.
#
# EDIT-ME: the exact top-level shape of `slt install --json`'s output
# is not independently confirmed (only the plain-text Id/Version/Path
# block has been seen for real) — the parser below searches
# recursively for any {Id|id, Path|path} pair so it's tolerant of
# reasonable shape differences, but confirm against the real
# /tmp/install.json (printed below) if this step fails.
# ---------------------------------------------------------------------
RUN slt install -f /tmp/recipe.toml --non-interactive --json > /tmp/install.json; \
    cat /tmp/install.json

# NOTE: this uses BuildKit's heredoc RUN form (plain `<<'PYEOF' ... PYEOF`,
# same as the recipe.toml heredoc above) specifically because a
# python3 -c "<multi-line>" argument does NOT work the same way — the
# Dockerfile instruction parser ends a shell-form RUN at the first
# newline not preceded by a backslash, UNLESS it's inside a `<<TOKEN`
# heredoc, which it does track across lines. Confirmed locally with a
# throwaway Dockerfile before relying on it here (python3 -c with an
# embedded newline genuinely fails: "unknown instruction: import").
RUN python3 - <<'PYEOF'
import json

with open('/tmp/install.json') as f:
    data = json.load(f)

paths = {}

def walk(obj):
    if isinstance(obj, dict):
        ident = obj.get('Id') or obj.get('id')
        path = obj.get('Path') or obj.get('path')
        if ident and path:
            paths[ident] = path
        for v in obj.values():
            walk(v)
    elif isinstance(obj, list):
        for item in obj:
            walk(item)

walk(data)

with open('/opt/slt/paths.env', 'w') as f:
    for ident, path in paths.items():
        var = ident.upper().replace('-', '_')
        f.write(f'export {var}_PATH="{path}"\n')
    f.write('export PATH="${SLC_CLI_PATH:-}/bin:${GCC_ARM_NONE_EABI_PATH:-}/bin:$PATH"\n')

print('--- /opt/slt/paths.env ---')
with open('/opt/slt/paths.env') as f:
    print(f.read())
PYEOF

RUN . /opt/slt/paths.env && slc configuration \
      --sdk="${SIMPLICITY_SDK_PATH}" \
      --gcc-toolchain="${GCC_ARM_NONE_EABI_PATH}"

WORKDIR /work
ENTRYPOINT ["/bin/bash"]
