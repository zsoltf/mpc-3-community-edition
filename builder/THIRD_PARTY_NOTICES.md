# Third-party notices

The browser image creator includes these source dependencies in its WebAssembly
binary:

- Go standard library and WebAssembly support code, under the Go BSD-style
  license in `GO-LICENSE.txt`.
- `github.com/ulikunitz/xz` v0.5.15, source revision
  `7eee8a8a405163554a9accec7b9402ee21400769`, under the BSD 3-Clause
  license in `XZ-LICENSE.txt`.
- `golang.org/x/crypto` v0.31.0, source revision
  `b4f1988a35dee11ec3e05d6bf3e90b695fbd8909`, under the Go BSD-style
  license in `X-CRYPTO-LICENSE.txt`.

The embedded sparse patches contain the qualified MPC Learn payload and ext4
filesystem edits, with separate SSH-disabled and owner-key variants. They do not
contain Akai firmware, an MPC image, a private SSH key, or a shared owner public
key. The official firmware and optional owner public key selected by the user
are processed locally in the browser and are not uploaded.
