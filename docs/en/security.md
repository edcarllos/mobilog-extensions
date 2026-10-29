# Security

## Isolation

Each extension is a WebAssembly module with **its own memory** (up to the manifest's limit). It cannot
read the app's memory or another extension's; it only exchanges bytes through the API functions, which
check every request (sizes, indices, permissions). There is no network, file system, processes or
system clock beyond `time_ms`. A module that hangs is stopped (by epoch in wasmtime, by a loop count in
the browser) and turned off.

The app checks the module before installing it: WebAssembly 1.0 with the accepted features only, memory
with a maximum, API imports only, no start function.

## Signatures and badges

| Badge | Meaning |
|---|---|
| **Mobilog** | an official extension (`mobilog.*` id), built and signed by the store |
| **Verificada · Loja** | built from this repository's reviewed code and signed with the store's key |
| **Verificada** | signed by a key the user trusts (Sistema → Extensões → Avançado → Chaves) |
| **Não verificada** | unsigned, or signed by an unknown key: the app warns before installing |
| **Em desenvolvimento** | from the user's own development folder |

The signature (Ed25519) covers **every** file in the package: a file changed, added or removed breaks
it. The store's index is signed too: a mirror (a `mobilog_server`) cannot change the list or the
packages.

## The store's key

The private key lives only in this repository's GitHub secrets (`MOBILOG_STORE_KEY`) and only
`publish.yml`, on `main`, uses it. Pull requests (forks included) have no access to it. The public key is
in `keys/loja.pub` and is built into the app. To change the key, the new one goes into `keys/` next to
the old one, an app version ships with both, and only then does the store sign with the new one.

## Reporting a problem

Found a security issue in the format, the tool or a store extension? Do not open a public issue: write
to the maintainer through the GitHub profile ([@edcarllos](https://github.com/edcarllos)).
