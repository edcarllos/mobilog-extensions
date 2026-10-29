# Publishing in the store

The **Mobilog store** is this repository's `mobilog_store/` folder. The app reads its index
(`catalog.json`, signed in `catalog.json.sig`) and downloads the packages. **Nobody sends `.mlx`
files:** you send the code; the store builds, checks, signs and publishes it. What is in the store is
always the build of reviewed code.

## Step by step

1. **Fork** this repository and make a branch (`my-extension`).
2. **Create the folder** with `mobilog_ext new io.github.<your-user>.<name>` (or copy a template).
   - The id is forever: users update by it.
   - Use your GitHub user in the id. `mobilog.*` ids are only for the official ones.
3. **Write, test and check:**
   ```bash
   mobilog_ext build extensions/<id>
   mobilog_ext test extensions/<id>
   mobilog_ext check extensions/<id> --store mobilog_store
   ```
   Do not commit `dist/` (the `.gitignore` ignores it).
4. **Open the pull request** to `main`, with the template's checklist filled in. Only
   `extensions/<id>/` should change.
5. **The PR's CI** (`pr.yml`) runs, for each changed extension:
   - `check --require-bump` (a new version compared to the store);
   - `build` twice, checking the bytes are the same (a reproducible build);
   - `test`.

   It has no access to keys: nothing is signed in a PR.
6. **Review.** A maintainer reads the code, the permissions asked for and the store texts. See what is
   refused in [guidelines.md](guidelines.md).
7. **Merge.** The publishing CI (`publish.yml`) builds it again, signs it with the store's key, writes
   `mobilog_store/<id>-<version>.mlx`, removes the previous version and rewrites the index. Within a few
   minutes (GitHub caches the index for up to 5 min) the extension shows in **Sistema → Extensões →
   Loja** in every app, with the **Verificada · Loja** (verified · store) badge.

## Updating

Change the code, **raise the version** in `manifest.json` and add a `## <new version>` section to
`CHANGELOG.md`. The CI refuses a PR that changes the extension without a new version. Users who have it
installed are told; an update that asks for no new permission installs with **Atualizar todas**
(update all).

## Who may change the folder

`.github/CODEOWNERS` makes the author the owner of the extension's folder: other people's PRs in it need
the author's approval (and a maintainer's).

## Leaving the store

Open a PR that removes the folder. On publishing, the package leaves the store; users who installed it
keep the installed version.

## Outside the store

You may also distribute the `.mlx` directly (website, e-mail) or from your organization's
`mobilog_server` (`mobilog_server ext add`). Sign it with your own key (`mobilog_ext keygen`,
`mobilog_ext sign`) and publish your `.pub`: users who trust it see the extension as **Verificada**.
