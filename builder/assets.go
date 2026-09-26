package main

import "embed"

//go:embed resources/ssh/manifest.json resources/ssh/patch.bin resources/no-ssh/manifest.json resources/no-ssh/patch.bin resources/diagnostics/manifest.json resources/diagnostics/patch.bin
var assets embed.FS
