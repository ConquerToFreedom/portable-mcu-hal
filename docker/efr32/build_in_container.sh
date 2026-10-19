#!/usr/bin/env bash
# Runs INSIDE the efr32 build container (see ../../scripts/build.sh).
# Generates a real board project via slc-cli from one of Simplicity
# SDK's own LED/GPIO example apps, overlays this test's main.c and the
# wrapper's own source on top of it (never the reverse — the SDK
# example's generated board-init/clock/pin-config scaffolding is what
# we keep; its own application logic is replaced), builds it, and
# copies the resulting .hex/.elf/.map to /out.
#
# PROVENANCE: written without being able to run it (see
# docker/efr32/Dockerfile's own header comment for why) — every
# EDIT-ME below is a concrete, confirmed gap to close against the
# first real CI log, not a design uncertainty.
set -euo pipefail

: "${SHORT_SHA:?}"
: "${TEST_DIR:?}"
: "${TARGET_MACRO:?}"
: "${BOARD:?}"
: "${TEST_NAME:?}"

WORK=/work/project
rm -rf "$WORK"
mkdir -p "$WORK"

# EDIT-ME: real example project path not independently confirmed from
# inside this sandbox (silabs.com blocked — see Dockerfile). Silicon
# Labs' own documented Makefile-workflow example uses
# "$SISDK/app/common/example/blink_baremetal" for a different board;
# this is the starting guess for BRD2601B's current-SDK (sl_gpio)
# equivalent. Run `find $SISDK/app -iname '*.slcp' | grep -i led` (or
# similar) in the container and fix this path if it's wrong — that is
# expected on the first real run, not a sign anything upstream broke.
EXAMPLE_SLCP="${SISDK:-/opt/slt/simplicity-sdk}/app/common/example/blink_baremetal/blink_baremetal.slcp"

echo "== slc generate: board=$BOARD example=$EXAMPLE_SLCP =="
slc generate "$EXAMPLE_SLCP" \
    -np \
    -d "$WORK" \
    -name="$TEST_NAME" \
    -o makefile \
    --with "${BOARD,,}"

# Overlay: our main.c replaces the generated project's own.
cp "$TEST_DIR/main.c" "$WORK/"

# Overlay: the wrapper itself — public header + config + the ONE
# adapter file this test actually exercises (GPIO only for `smoke`).
# EDIT-ME: as more tests are added (I2C etc.), this needs the matching
# adapter file(s) added here too — not hardcoded to GPIO forever.
mkdir -p "$WORK/wrapper/include"
cp /repo/include/hal_wrapper.h /repo/include/hal_wrapper_config.h "$WORK/wrapper/include/"
mkdir -p "$WORK/wrapper/gpio"
cp /repo/silicon_labs/gpio/hal_port_silabs_slgpio.c "$WORK/wrapper/gpio/"

GENERATED_MAKEFILE=$(find "$WORK" -maxdepth 1 -name "*.Makefile" | head -1)
if [ -z "$GENERATED_MAKEFILE" ]; then
    echo "error: slc generate did not produce a *.Makefile in $WORK" >&2
    exit 1
fi

echo "== make: $GENERATED_MAKEFILE =="
make -C "$WORK" -f "$(basename "$GENERATED_MAKEFILE")" \
    -j"$(nproc)" \
    CFLAGS_EXTRA="-D$TARGET_MACRO -DFW_GIT_SHA=\\\"$SHORT_SHA\\\" -I$WORK/wrapper/include" \
    SRC_EXTRA="$WORK/wrapper/gpio/hal_port_silabs_slgpio.c"
# EDIT-ME: whether the generated Makefile actually honors
# CFLAGS_EXTRA/SRC_EXTRA override variables (vs. needing the extra
# include dir and source file woven directly into the generated
# Makefile/project) is NOT confirmed — this is the single most likely
# next thing to fix once a real generated Makefile exists to inspect.

mkdir -p /out
find "$WORK" -maxdepth 2 \( -name "*.hex" -o -name "*.elf" -o -name "*.map" \) -exec cp {} /out/ \;

for ext in hex elf map; do
    f=$(find /out -maxdepth 1 -name "*.$ext" | head -1)
    if [ -n "$f" ]; then
        mv "$f" "/out/${TEST_NAME}.${ext}"
    else
        echo "warning: no .$ext produced" >&2
    fi
done

echo "== done, /out contents =="
ls -la /out
