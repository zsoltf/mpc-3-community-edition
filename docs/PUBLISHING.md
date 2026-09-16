# Publishing the public release

Development stays in the private mpclearn project. The clean release checkout
is a publication snapshot for **zsoltf/mpc-3-community-edition**, not a replacement
for the development repository. Its local location is
`artifacts/mpc-3-community-edition` under the main mpclearn checkout. Release
attachments are in `artifacts/community-release-v0.1.0-rc.3` alongside it.
Never push the private repository's history.

## 1. Create the GitHub repository

While signed in as **zsoltf**, create `mpc-3-community-edition`:

- Visibility: **Public** for the community release.
- Leave README, .gitignore and license initialization **unchecked**; the
  prepared local checkout supplies its own files.
- Description: `Hands-on X-Touch control and a browser firmware image builder for MPC standalone.`
- After creation, set the About website to
  `https://zsoltf.github.io/mpc-3-community-edition/`.
- Suggested topics: `mpc`, `akai`, `midi`, `mackie-control`, `xtouch`, `music`.

The prepared release checkout already has a local `main` commit and no remote.
From that checkout, with credentials for your **zsoltf** account:

```sh
git remote add origin https://github.com/zsoltf/mpc-3-community-edition.git
git push -u origin main
```

Use GitHub Desktop if preferred: add the prepared local repository and publish
it to the intended account. Do not run these commands in the private project.

## 2. Enable GitHub Pages

In the public repository, open **Settings > Pages > Build and deployment** and
select **GitHub Actions** as the source. No gh-pages branch is needed.

Open **Actions > Publish MPC 3 CE Image Builder > Run workflow**, choose `main`
and run it. If the first push ran before Pages was enabled and failed, rerun
that workflow. Future changes to the builder on main publish automatically.

The workflow tests the Go builder, builds WebAssembly and deploys only the
static `builder/dist` files. Firmware, keys and local generated images are not
uploaded. The site will be:

`https://zsoltf.github.io/mpc-3-community-edition/`

Check the builder, guide, repository links and official Akai download link on
the deployed page. Deployment has not been observed until this workflow runs.

Reference: [GitHub Pages custom workflows](https://docs.github.com/en/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages).

## 3. Create the release

Open **Releases > Draft a new release**:

- New tag: `v0.1.0-rc.3`, target: **main**.
- Title: `MPC 3 Community Edition v0.1.0-rc.3`.
- Copy the body from [release notes](releases/v0.1.0-rc.3.md).
- Check **Set as a pre-release**.
- Attach only the prepared website ZIP, runtime payload archive and SHA256SUMS.
  GitHub supplies source archives automatically.
- Do not attach an Akai firmware image, personal recovery image, public/private
  SSH keys, MPC.settings, projects or private research exports.

Save a draft while preparing. The release notes record the r4 flash, boot and
upgrade on the owner's Live II, and state that the r5 image itself has not been
flashed yet. Keep the experimental pre-release label while community testing
expands hardware and long-session coverage.

## Later updates

Prepare another clean snapshot from private development, review the changes in
this release checkout and push only the intended public files. Keep runtime,
patch recipes, guide version and release notes consistent. Never merge the
private Git history into the public repository.

Exclude development-only `AGENTS.md`, `LEARNINGS.md` and `PLAN.md` from the
public snapshot. Keep useful performance measurements and contributor-facing
technical guidance in normal public documentation instead.

The private development checkout owns the reviewed export list at
`release/public-files.json`. Its `scripts/export_public.py` refreshes this
public checkout from that list; it does not create repositories or push.
Review and commit the exported changes before publication. Do not re-export
whole source directories or private Git history.
