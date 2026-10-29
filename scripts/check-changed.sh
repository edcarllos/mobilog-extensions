#!/usr/bin/env bash
# What the pull requests' CI runs (pr.yml), the same by hand: for each extension changed since <base>
# (default origin/main): check against the store (a new version), build twice (the same bytes), test.
# The store, the keys and the official list are not changed by pull requests.
#   scripts/check-changed.sh [base] [mobilog_ext]
set -euo pipefail
cd "$(dirname "$0")/.."
base=${1:-origin/main}
tool=$(realpath "${2:-build/tools/mobilog_ext/mobilog_ext}")
changed=$(git diff --name-only "$base"...HEAD)
fail=0
if echo "$changed" | grep -q '^mobilog_store/'; then
  echo "erro / error: mobilog_store/ é gerada pelo CI de publicação; não a mude num pull request / is made by the publishing CI" >&2
  fail=1
fi
ids=$(echo "$changed" | sed -n 's|^extensions/\([^/]*\)/.*|\1|p' | sort -u)
[ -z "$ids" ] && echo "nenhuma extensão alterada / no extension changed"
for id in $ids; do
  dir=extensions/$id
  [ -d "$dir" ] || { echo "== $id: removida / removed"; continue; }
  echo "== $id"
  "$tool" check "$dir" --store mobilog_store --require-bump || { fail=1; continue; }
  v=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["version"])' "$dir/manifest.json")
  cp "$dir/dist/$id-$v.mlx" "/tmp/first-$$.mlx"
  "$tool" build "$dir" -q
  if ! cmp -s "/tmp/first-$$.mlx" "$dir/dist/$id-$v.mlx"; then
    echo "erro / error: build não reprodutível / not reproducible: $id" >&2
    fail=1
  fi
  rm -f "/tmp/first-$$.mlx"
  "$tool" test "$dir" || fail=1
done
exit $fail
