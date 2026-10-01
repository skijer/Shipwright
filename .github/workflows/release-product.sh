#!/usr/bin/env bash
# SOH [fork] The product a release tag names, as key=value lines for $GITHUB_OUTPUT: the product
# (which picks .github/release-notes/<product>.md), the branch that builds it, the release title,
# and whether the release is a draft. The one place generate-builds.yml maps a tag to its product.
# A tag ending in "-test" publishes a draft, visible only to maintainers, for trying the pipeline.
set -euo pipefail

TAG="$1"
case "$TAG" in
  *modapi*)   echo "product=unbound-modapi"; echo "branch=unbound-modapi";          echo "title=SoH: Unbound ModApi ${TAG}" ;;
  *unbound*)  echo "product=unbound";  echo "branch=unbound";                      echo "title=SoH: Unbound ${TAG}" ;;
  *celshade*) echo "product=celshade"; echo "branch=wind-waker-style-cel-shading"; echo "title=SoH (cel-shading fork) ${TAG}" ;;
  *) echo "tag ${TAG} names no product" >&2; exit 1 ;;
esac
case "$TAG" in
  *-test) echo "draft=true" ;;
  *)      echo "draft=false" ;;
esac
