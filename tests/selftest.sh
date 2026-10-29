#!/usr/bin/env bash
# selftest.sh <mobilog_ext> <repository>: the tool end to end in a scratch copy of the repository.
set -euo pipefail
tool=$(realpath "$1")
repo=$(realpath "$2")
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
export MOBILOG_EXT_LANG=en
# A copy with what the tool needs (the SDK, the format headers, the templates; wasi-sdk by link).
mkdir -p "$work/repo/extensions" "$work/repo/external"
cp -r "$repo/sdk" "$repo/mlx" "$repo/templates" "$work/repo/"
mkdir -p "$work/repo/extensions"
if [ -n "${WASI_SDK_PREFIX:-}" ]; then ln -s "$WASI_SDK_PREFIX" "$work/repo/external/wasi-sdk"
else ln -s "$(realpath "$repo/external/wasi-sdk")" "$work/repo/external/wasi-sdk"; fi
cd "$work/repo"
fail() { echo "FAIL: $*" >&2; exit 1; }
expect_fail() { if "$@" >/dev/null 2>&1; then fail "should have failed: $*"; fi; }

for lang in c cpp; do
  id="io.github.selftest.$lang"
  "$tool" new "$id" --$lang --author "Selftest" >/dev/null
  "$tool" build "extensions/$id" >/dev/null
  [ -f "extensions/$id/dist/$id-0.1.0.mlx" ] || fail "no package for $lang"
  "$tool" check "extensions/$id" >/dev/null || fail "check $lang"
  "$tool" test "extensions/$id" >/dev/null || fail "test $lang"
  "$tool" verify "extensions/$id/dist/$id-0.1.0.mlx" >/dev/null || fail "verify $lang"
  # Two builds, the same bytes.
  cp "extensions/$id/dist/$id-0.1.0.mlx" first.mlx
  "$tool" build "extensions/$id" >/dev/null
  cmp -s first.mlx "extensions/$id/dist/$id-0.1.0.mlx" || fail "build not reproducible ($lang)"
done

e=extensions/io.github.selftest.c
# A store and the version bump.
mkdir store
cp "$e/dist/"*.mlx store/
"$tool" catalog store >/dev/null
expect_fail "$tool" check "$e" --store store --require-bump
"$tool" check "$e" --store store >/dev/null || fail "check without --require-bump should pass"
# Signing: catalog.json.sig, then a package signed by a key.
"$tool" keygen chave >/dev/null
"$tool" catalog store --key chave.secret >/dev/null
[ -f store/catalog.json.sig ] || fail "no catalog signature"
"$tool" sign "$e/dist/io.github.selftest.c-0.1.0.mlx" --key chave.secret -o signed.mlx >/dev/null
"$tool" verify signed.mlx --trust chave.pub >/dev/null || fail "signed package"
# Refusals.
expect_fail "$tool" new mobilog.reservado
expect_fail "$tool" new "Id Inválido"
cp "$e/manifest.json" manifest.bak
sed -i.tmp 's/"mapa.ler", //' "$e/manifest.json"
expect_fail "$tool" test "$e"   # map.info without its permission
cp manifest.bak "$e/manifest.json"
cp "$e/icon.svg" icon.bak
echo '<svg xmlns="http://www.w3.org/2000/svg" width="48" height="48"></svg>' > "$e/icon.svg"
expect_fail "$tool" check "$e"
cp icon.bak "$e/icon.svg"
sed -i.tmp 's/"category": "ferramentas"/"category": "jogos"/' "$e/manifest.json"
expect_fail "$tool" build "$e"
cp manifest.bak "$e/manifest.json"
sed -i.tmp 's/"id": "io.github.selftest.c"/"id": "mobilog.selftest"/' "$e/manifest.json"
expect_fail "$tool" build "$e"
cp manifest.bak "$e/manifest.json"
# The API reference is the one the tool writes (docs/*/api.md: generated, never edited by hand).
for lang in pt en; do
  "$tool" doc api --lang $lang -o "api-$lang.md"
  cmp -s "api-$lang.md" "$repo/docs/$lang/api.md" || fail "docs/$lang/api.md is stale: mobilog_ext doc api --lang $lang -o docs/$lang/api.md"
done
echo "selftest: OK"
