# FAQ

### Why did my pull request fail in the CI?

Open the log of the failed step. The usual reasons (the messages from the package checks are in
Portuguese):

- `version: ... já está na loja` (already in the store): raise the version in `manifest.json` and add
  `## <version>` to `CHANGELOG.md`.
- `icon: almost nothing shows at 48 px` or `has no shapes`: the icon uses text or images, or has no
  `viewBox`/`width`/`height`. See [guidelines.md](guidelines.md#icon).
- `... without the permission "x" in the manifest`: the extension calls a function whose permission it
  did not ask for.
- `build not reproducible`: something in the build depends on the machine (date, path). Use
  `mobilog_ext build`.
- `the folder must be named after the id`: `extensions/<id>/` with the manifest's id.

Run `mobilog_ext check extensions/<id> --store mobilog_store --require-bump` and
`mobilog_ext test extensions/<id>` locally: they are the same steps.

### Can I write it in Rust, Zig, AssemblyScript...?

Yes, if it produces a WebAssembly 1.0 module (with bulk memory, sign-ext, non-trapping conversions and
mutable globals; no SIMD, threads or exceptions), exporting its memory with a maximum and the `mlx_*`
functions, and importing only the `mobilog` module (see `mlx/include/mlx/Abi.h`). A minimal Rust
example:

```rust
#[link(wasm_import_module = "mobilog")]
extern "C" { fn log(level: i32, text: *const u8, len: i32); }

#[no_mangle] pub extern "C" fn mlx_api_version() -> i32 { 1 }
#[no_mangle] pub extern "C" fn mlx_init() -> i32 {
    let s = b"hello from Rust";
    unsafe { log(1, s.as_ptr(), s.len() as i32) };
    0
}
// mlx_alloc / mlx_free: export an allocator (over Vec<u8>, for instance).
```

Build with `--target wasm32-unknown-unknown` and `-C link-arg=--max-memory=67108864`, and pack it with
`mobilog_ext pack`. For the store, `mobilog_ext build` builds C and C++ today; for another language,
open an issue: `build.json` can learn it.

### How do I debug?

`mobilog_log` (or `printf` with the C library) goes to **Sistema → Extensões → (the extension) →
Registro** (log). Use `mobilog_ext test` with a script to replay a sequence of events, and the
development folder to see each build in the app. See [testing.md](testing.md).

### The app does not show my update

GitHub caches the store's index for up to 5 minutes, and the app reads the store when it opens and once
a day. Tap **Atualizar a lista** (refresh) in the store.

### Does it work offline?

The store keeps its last index: it opens offline, but downloading needs the network. A
`mobilog_server` on the local network mirrors the store (on by default) and serves devices without
internet.

### Can my extension reach the internet or files?

No. See [permissions.md](permissions.md#what-no-permission-allows).

### How do I keep data between sessions?

`storage_set`/`storage_get` (up to 20 MB, on the device), or the extension's section of the scenario
(`scenario.data_set`), which goes with the scenario. See [concepts.md](concepts.md).

### How do I know whether a function exists in this app version?

`mobilog_has("function.name", n)`. New functions get new numbers; old ones never change.

### Which license?

Any you like, stated in `license`. Without a `LICENSE` of its own in the folder, this repository's MIT
applies.

### Can I sell an extension or charge for it?

The store only has free, open-source extensions. Outside it, distributing your `.mlx` is up to you.
