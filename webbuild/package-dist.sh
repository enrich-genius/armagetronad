#!/usr/bin/env bash
#
# Package the built client for consumption by the platform.
#
# The platform deploys the client as an opaque artifact rather than by cloning
# this repository, so this produces a tarball plus a checksum and a manifest
# recording exactly which source it came from. The manifest is what makes the
# GPL source offer answerable: given a deployed build, it names the commit.
#
# Usage:
#   webbuild/package-dist.sh [outdir]
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/.." && pwd)"
DIST="$HERE/dist"
OUT="${1:-$REPO/build-artifacts}"

[[ -f "$DIST/armagetronad.html" ]] || {
  echo "No build at $DIST. Run webbuild/build.sh first." >&2
  exit 1
}

VERSION="$(grep -o 'assetVersion="[0-9]*"' "$DIST/armagetronad.html" | head -1 | grep -o '[0-9]*')"
[[ -n "$VERSION" ]] || { echo "Could not read assetVersion from the build." >&2; exit 1; }

COMMIT="$(git -C "$REPO" rev-parse HEAD 2>/dev/null || echo unknown)"
DIRTY=""
git -C "$REPO" diff --quiet 2>/dev/null || DIRTY=" (dirty)"

mkdir -p "$OUT"
TARBALL="$OUT/armagetronad-client-$VERSION.tar.gz"

# Manifest travels inside the tarball, so a deployed artifact always carries its
# own provenance even if it is copied around.
cat > "$DIST/BUILD-INFO.json" <<EOF
{
  "product": "armagetronad-wasm-client",
  "assetVersion": "$VERSION",
  "sourceRepository": "https://github.com/enrich-genius/armagetronad",
  "sourceCommit": "$COMMIT",
  "license": "GPL-2.0-or-later",
  "sourceOffer": "https://github.com/enrich-genius/armagetronad/blob/$COMMIT/LICENSES.md",
  "builtAt": "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
}
EOF

# Only the runtime assets. build.sh leaves its .o intermediates in dist, and
# shipping those would put stale objects in front of players.
tar -czf "$TARBALL" -C "$DIST" \
  armagetronad.html armagetronad.js armagetronad.wasm armagetronad.data \
  BUILD-INFO.json \
  $( [[ -f "$DIST/_headers" ]] && echo _headers ) \
  $( [[ -f "$DIST/_redirects" ]] && echo _redirects )
( cd "$OUT" && sha256sum "$(basename "$TARBALL")" > "$(basename "$TARBALL").sha256" )

echo "Packaged: $TARBALL$DIRTY"
echo "  assetVersion $VERSION"
echo "  source commit $COMMIT"
echo "  $(cd "$OUT" && cat "$(basename "$TARBALL").sha256")"
echo
echo "Publish as a release asset with:"
echo "  gh release create client-$VERSION \"$TARBALL\" \"$TARBALL.sha256\" \\"
echo "    --repo enrich-genius/armagetronad --title \"Client $VERSION\" --notes \"Built from $COMMIT\""
