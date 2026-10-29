# Store guidelines

The store wants useful, honest and light extensions. Reviews follow these rules.

## Identity

- **Id:** `io.github.<your-user>.<name>` (or a domain of yours, reversed: `org.example.<name>`). Do not
  use someone else's name. `mobilog.*` is for the official ones.
- **Name:** short (up to 40 characters), says what it does. Do not use "Mobilog" in it.
- **Summary:** one sentence, up to 80 characters, without "the best", "the only" or other superlatives.
- **Category:** the one that describes its main use.
- **Keywords:** what someone would search for; do not repeat the name.

## Icon

- `icon.svg`, square (`viewBox="0 0 48 48"` suggested), up to 32 KB.
- **Shapes only** (`rect`, `circle`, `path`...): text, embedded images and scripts are not drawn by the
  app.
- A solid color background (rounded corners, radius ~11 of 48) with a clear symbol on top works on both
  the light and dark themes.
- Readable at 24 px: few details, strokes of 3 px or more.
- `templates/icon.svg` is a starting point. `check` draws the icon with the app's renderer (nanosvg) and
  refuses one that does not show.

## Screenshots

Up to 4, PNG or JPG, up to 300 KB each, preferably 1280×720 (16:9). Show the extension working on the
map, not an empty screen. A system screenshot does: shrink it to 1280×720 and save it as JPG at
quality ~80.

## Manual and changes

- `help.md` explains what it does, how to use it and what each permission allows. `help.en.md` in
  English (recommended).
- `CHANGELOG.md` with one section per version (`## 1.2.0`), in user language.

## What is refused

- Asking for permissions the extension does not use.
- Obfuscated code, code generated without its source, or binary dependencies without source.
- Misleading texts (claiming what it does not do), offensive or illegal content.
- Collecting personal data: extensions have no network; do not try to get around it (for instance,
  asking the user to copy data out).
- Extensions that hang or slow the app (long hooks, drawing every frame with no change).
- Copies of another extension with nothing added, or without the credit its license requires.

## License

State the license in `license` (and add a `LICENSE` to the folder if it is not MIT). The code of the
extensions in this repository is public. Without a `LICENSE` of its own, the repository's MIT license
applies.
