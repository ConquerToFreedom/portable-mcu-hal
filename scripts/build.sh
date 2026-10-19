#!/usr/bin/env bash
# build.sh <vendor> <test> <short_sha>
#
# Builds one vendor's one test firmware inside its pinned Docker image
# (see verify/targets.yml / docker/<vendor>/Dockerfile) and copies the
# resulting .hex/.elf/.map into out/<vendor>/<test>.*.
#
# Does NOT flash anything and does NOT talk to HF·NODE — build only.
set -euo pipefail

if [ $# -ne 3 ]; then
    echo "usage: $0 <vendor> <test> <short_sha>" >&2
    exit 1
fi

VENDOR="$1"
TEST="$2"
SHORT_SHA="$3"

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

read_target() {
    python3 -c "
import sys, yaml
with open('verify/targets.yml') as f:
    cfg = yaml.safe_load(f)
t = cfg['targets']['$VENDOR']
print(t['$1'])
"
}

read_test_field() {
    python3 -c "
import sys, yaml
with open('verify/targets.yml') as f:
    cfg = yaml.safe_load(f)
t = cfg['targets']['$VENDOR']
tests = [x for x in t['tests'] if x['name'] == '$TEST']
if not tests:
    sys.exit('test \"$TEST\" not found for vendor \"$VENDOR\" in verify/targets.yml')
print(tests[0]['$1'])
"
}

if ! python3 -c "
import yaml
with open('verify/targets.yml') as f:
    cfg = yaml.safe_load(f)
import sys
sys.exit(0 if '$VENDOR' in cfg['targets'] else 1)
"; then
    echo "error: vendor '$VENDOR' not found in verify/targets.yml" >&2
    exit 1
fi

DOCKERFILE="$(read_target dockerfile)"
IMAGE="$(read_target docker_image)"
TARGET_MACRO="$(read_target target_macro)"
BOARD="$(read_target board)"
TEST_DIR="$(read_test_field dir)"

echo "== building docker image: $IMAGE (from $DOCKERFILE) =="
# DOCKER_BUILD_EXTRA_ARGS lets CI opt into registry/GHA layer caching
# (e.g. "--cache-from=type=gha --cache-to=type=gha,mode=max") without
# this script needing to know it's running in CI. Empty/unset locally.
# shellcheck disable=SC2086
docker buildx build --load ${DOCKER_BUILD_EXTRA_ARGS:-} \
    -t "$IMAGE" -f "$DOCKERFILE" "$(dirname "$DOCKERFILE")"

OUT_DIR="out/$VENDOR"
mkdir -p "$OUT_DIR"

echo "== building $VENDOR/$TEST (sha=$SHORT_SHA, board=$BOARD, macro=$TARGET_MACRO) =="

# The actual slc-generate + make sequence lives in build_in_container.sh
# so it can be iterated on without touching this orchestration script.
# EDIT-ME: this inner script does not exist and has not been run for
# real yet (see docs/verification/efr32.md) — it's the next file to
# write/fix once a real `docker build` of docker/efr32/Dockerfile has
# been attempted in CI.
docker run --rm \
    -v "$ROOT:/repo:ro" \
    -v "$ROOT/$OUT_DIR:/out" \
    -e "SHORT_SHA=$SHORT_SHA" \
    -e "TEST_DIR=/repo/$TEST_DIR" \
    -e "TARGET_MACRO=$TARGET_MACRO" \
    -e "BOARD=$BOARD" \
    -e "TEST_NAME=$TEST" \
    "$IMAGE" \
    bash /repo/docker/efr32/build_in_container.sh

echo "== artifacts =="
ls -la "$OUT_DIR"
