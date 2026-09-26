# Browser MCU image creator

This static browser app creates a personal MPC3.9.1 Gen1 CE v0.2.5 image with
X-Touch control, USB mouse support and built-in Shift+wheel note selection.
The optional Debug build saves a current report to a marked USB drive. Firmware
and keys stay in the browser; the app does not connect to or flash an MPC.

`web/guide.html` is the printable setup and controller reference. Its mappings
are maintained alongside `docs/xtouch-cheatsheet.md`. The build publishes
`web/xtouch-cheatsheet.md`; keep the controller mappings in both copies consistent.

SSH is disabled unless the user selects their own Ed25519 OpenSSH `.pub` file.
Private keys are not needed.

The Debug build keeps SSH off and writes a current report only to a USB drive
with an `MPCLEARN-DIAGNOSTICS` folder at its root.

## Build the static site

Go is required only to build the distributable site, not to use it:

```sh
cd builder
./build-web.sh
```

Serve `builder/dist/` as ordinary static files over HTTPS. The page requires
WebAssembly, Web Worker, File and Blob APIs. Allow at least 2.5 GB of free
memory while it builds the image.

## GitHub Pages

Public site: https://zsoltf.github.io/mpc-3-community-edition/

Choose Settings > Pages > Source > GitHub Actions in
`zsoltf/mpc-3-community-edition`. See [publishing instructions](../docs/PUBLISHING.md).
The supplied workflow builds and publishes only `builder/dist`. All site asset
URLs are relative, so the same output works under a repository Pages subpath.
No firmware, keys, image output, or server credentials are needed by the build.

GitHub Pages supports static sites and custom build workflows:
https://docs.github.com/en/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages

Developers can run `go run . -input OFFICIAL.img -output RESULT-update.img`.
Add `-public-key owner.pub` for SSH or `-diagnostics` for the Debug build.

The generated image includes Akai firmware selected by the user and must not be
published as a generic download. Preserve the `-update.img` suffix. See
`THIRD_PARTY_NOTICES.md` for embedded dependency versions and licenses.
