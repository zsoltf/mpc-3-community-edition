#!/bin/sh
set -eu
cd "$(dirname "$0")"
rm -rf dist
mkdir dist
GOOS=js GOARCH=wasm go build -trimpath -ldflags '-s -w' -o dist/mpclearn-installer.wasm .
cp "$(go env GOROOT)/lib/wasm/wasm_exec.js" dist/wasm_exec.js
cp web/index.html web/guide.html web/worker.js web/favicon.svg web/xtouch-cheatsheet.md LICENSE THIRD_PARTY_NOTICES.md GO-LICENSE.txt XZ-LICENSE.txt X-CRYPTO-LICENSE.txt dist/
