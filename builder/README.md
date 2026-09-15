# Browser MCU image creator

This static browser app creates a personal MPC3.9.1 Gen1 USB image containing
the MPC Learn MCU runtime 9000390. It is limited to the tested MPC
Live II and full X-Touch in MC/USB mode. It creates and verifies an image; it
does not connect to or flash an MPC.

The browser verifies the exact official input SHA-256, strictly decodes its
AZ01/XZ container, checks the complete 441,279,488-byte root filesystem, applies
an embedded sparse patch, recompresses the filesystem, and decodes the finished
image before offering it for download. Firmware and keys stay inside the
browser; the site has no upload or server API.

The pure-Go engine is shared with the native command-line verification harness.
It can later be wrapped in a signed macOS or Windows app without changing the
patch format; this release implements only the static browser interface.

`web/guide.html` is the printable setup and controller reference. Its mappings
are maintained alongside `docs/xtouch-cheatsheet.md`. The build publishes
`web/xtouch-cheatsheet.md`; keep the controller mappings in both copies consistent.

SSH is disabled unless the user selects their own Ed25519 OpenSSH public key.
For the SSH-enabled variant the app removes comments and writes the canonical
81-byte public key into the preverified filesystem slot. Private keys are not
needed or generated; selected private-key files are rejected.

## Build the static site

Go is required only to build the distributable site, not to use it:

```sh
cd builder
./build-web.sh
```

Serve `builder/dist/` as ordinary static files over HTTPS. The page requires
WebAssembly, Web Worker, File and Blob APIs. The full no-SSH path was observed
in Chrome152 on macOS: it took4m40s and the renderer reached about2.1GB RSS once
the downloadable image Blob existed. Allow at least2.5GB of free memory.
Windows and other browsers have not yet exercised the real firmware path.

## GitHub Pages

Public site: https://zsoltf.github.io/mpc-3-community-edition/

Choose Settings > Pages > Source > GitHub Actions in
`zsoltf/mpc-3-community-edition`. See [publishing instructions](../docs/PUBLISHING.md).
The supplied workflow builds and publishes only `builder/dist`. All site asset
URLs are relative, so the same output works under a repository Pages subpath.
No firmware, keys, image output, or server credentials are needed by the build.
The workflow uses the same Go1.26.4 compiler as the observed browser candidate.

GitHub Pages supports static sites and custom build workflows:
https://docs.github.com/en/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages

`go run . -input OFFICIAL.img -output RESULT-update.img` is a developer
verification harness. Add `-public-key owner.pub` to exercise the opt-in SSH
variant. The website is the end-user deliverable.

The generated image includes Akai firmware selected by the user and must not be
published as a generic download. Preserve the `-update.img` suffix. See
`THIRD_PARTY_NOTICES.md` for embedded dependency versions and licenses.
