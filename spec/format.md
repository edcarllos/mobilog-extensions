# The `.mlx` format, version 1

Reference implementation: [`mlx/`](../mlx) (C++, MIT). The Mobilog app and `mobilog_ext` use it; a
package it accepts installs in the app. Português: [formato.md](formato.md).

## Package

A `.mlx` is a **zip** file (stored or deflate members only; no encryption, zip64, multi-disk archives
or symlinks) holding at most 256 members, 64 MB per member, 128 MB uncompressed, a compression ratio of
at most 200, and 48 MB as a whole. Member names are relative UTF-8 paths with `/`, without `.`, `..`,
`\` or `:`, unique even ignoring case, up to 160 bytes.

| Member | |
|---|---|
| `manifest.json` | required (see the [manifest](../docs/en/manifest.md)) |
| the `entry` module | required: a WebAssembly module |
| the files the manifest names | `icon`, `help`, `help@*`, `changelog`, `screenshots` |
| `signature.json` | optional |

Writers produce deterministic zips: `manifest.json` first, the rest sorted by name, timestamps
1980-01-01 00:00, no extra attributes.

## Module

WebAssembly 1.0 plus bulk memory, sign extension, non-trapping float-to-int and mutable globals; nothing
else (no SIMD, threads, exceptions, reference types, multi-value, memory64, multiple memories or
tables). It must define and export its `memory` with a maximum of at most `limits.memoryMB`, have no
start function, import only the functions in [`mlx/include/mlx/Abi.h`](../mlx/include/mlx/Abi.h)
(module `mobilog`, and a few `wasi_snapshot_preview1` ones) and export `mlx_api_version`, `mlx_alloc`,
`mlx_free` and `mlx_init` (optionally `mlx_event`, `mlx_task`, `mlx_shutdown`, `_initialize`).

## Signature (`signature.json`)

```json
{"version": 1, "key": "<Ed25519 public key, base64>", "signature": "<base64>",
 "files": [{"path": "...", "size": 123, "sha512": "<hex>"}]}
```

The signed text is `"mobilog-ext-signature-v1\n"` followed by one `path\tsize\tsha512\n` line per
member except `signature.json`, sorted by path (bytes). Every member must be listed with its size and
SHA-512, and nothing else.

Key files (text): `mobilog-ext-key-v1 <base64 public key> [name]` and
`mobilog-ext-secret-v1 <base64 64-byte secret key>` (Monocypher's Ed25519 secret).

## Catalog (`catalog.json`)

```json
{"version": 1, "server": "<name>", "serverKey": "<key line or empty>",
 "extensions": [{"id", "name", "version", "description", "author", "permissions": ["..."],
                 "file": "<id>-<version>.mlx", "size", "sha512", "signerKey": "<base64 or empty>",
                 "example": false,
                 "summary", "category", "keywords", "license", "homepage", "repository",
                 "minAppVersion", "apiVersion", "updated", "icon": "<SVG text>", "screenshots": 2,
                 "name@<lang>", "summary@<lang>", "description@<lang>"}]}
```

`file` is a plain name: packages sit next to the index (`<index url's folder>/<file>`). A client
downloads a package and installs it only when its size and SHA-512 match the entry. Readers skip
entries they cannot use and ignore fields they do not know.

## Store index signature (`catalog.json.sig`)

```
mobilog-catalog-signature-v1 <public key, base64> <signature, base64>
```

An Ed25519 signature of `"mobilog-catalog-v1\n"` followed by the exact bytes of `catalog.json`. The
Mobilog store's index is accepted only when signed by one of the store's keys ([`keys/`](../keys)).

## Trust

A package signed by a store key may use the reserved ids (`mobilog.*`); with such an id it is one of
the Mobilog's own. Other store-signed packages are "verified by the store". Packages signed by other
keys are verified when the user trusts the key; unsigned ones are unverified.
