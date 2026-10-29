#!/usr/bin/env bash
# Publishes into mobilog_store/ every extension whose version is not there yet: built, checked as
# the store checks, signed with the store's key, the old version removed; then the index
# (catalog.json) is written again and signed. The workflow publish.yml runs this on main; the same
# works by hand with a test key.
#   scripts/publish-store.sh <store.secret> [mobilog_ext]
set -euo pipefail
cd "$(dirname "$0")/.."
key=$(realpath "$1")
tool=${2:-build/tools/mobilog_ext/mobilog_ext}
tool=$(realpath "$tool")
store=mobilog_store
mkdir -p "$store"
version_of() { python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["version"])' "$1"; }
published=0
for dir in extensions/*/; do
  dir=${dir%/}
  [ -f "$dir/manifest.json" ] || continue
  id=$(basename "$dir")
  v=$(version_of "$dir/manifest.json")
  if [ -f "$store/$id-$v.mlx" ]; then continue; fi
  echo "== $id $v"
  args=()
  [ -f "$store/catalog.json" ] && args=(--store "$store" --require-bump)
  "$tool" check "$dir" "${args[@]}"
  "$tool" test "$dir" >/dev/null
  rm -f "$store/$id"-*.mlx
  official=()
  grep -qx "$id" extensions/OFFICIAL && official=(--mobilog)  # the reserved ids "mobilog.*"
  "$tool" sign "$dir/dist/$id-$v.mlx" --key "$key" -o "$store/$id-$v.mlx" "${official[@]}"
  published=$((published + 1))
done
# Packages whose folder is gone leave the store.
for f in "$store"/*.mlx; do
  [ -e "$f" ] || continue
  name=$(basename "$f" .mlx)
  found=0
  for dir in extensions/*/; do
    id=$(basename "$dir")
    case "$name" in "$id"-[0-9]*) found=1 ;; esac
  done
  [ $found = 1 ] || { echo "removida / removed: $name"; rm -f "$f"; published=$((published + 1)); }
done
"$tool" catalog "$store" --key "$key" --name "Loja Mobilog" --updated-from-git .
echo "$published pacote(s) publicado(s) / package(s) published"
