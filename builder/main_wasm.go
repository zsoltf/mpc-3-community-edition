//go:build js && wasm

package main

import (
	"fmt"
	"runtime/debug"
	"syscall/js"
)

func main() {
	js.Global().Set("mpclearnBuild", js.FuncOf(startWebBuild))
	js.Global().Call("postMessage", map[string]any{"type": "ready"})
	select {}
}

func startWebBuild(this js.Value, args []js.Value) any {
	if len(args) != 2 || !args[0].InstanceOf(js.Global().Get("Uint8Array")) || !args[1].InstanceOf(js.Global().Get("Uint8Array")) {
		return "expected image and optional owner-key Uint8Arrays"
	}
	inputValue := args[0]
	keyValue := args[1]
	go func() {
		defer func() {
			if recovered := recover(); recovered != nil {
				js.Global().Call("postMessage", map[string]any{"type": "error", "message": fmt.Sprintf("image creation failed: %v", recovered)})
				debug.FreeOSMemory()
			}
		}()
		input := make([]byte, inputValue.Get("byteLength").Int())
		if copied := js.CopyBytesToGo(input, inputValue); copied != len(input) {
			js.Global().Call("postMessage", map[string]any{"type": "error", "message": "could not read the complete selected image"})
			return
		}
		ownerKey := make([]byte, keyValue.Get("byteLength").Int())
		if copied := js.CopyBytesToGo(ownerKey, keyValue); copied != len(ownerKey) {
			js.Global().Call("postMessage", map[string]any{"type": "error", "message": "could not read the complete owner public key"})
			return
		}
		result, err := buildImageMemory(input, ownerKey, func(percent int, message string) {
			js.Global().Call("postMessage", map[string]any{"type": "progress", "progress": percent, "message": message})
		})
		input = nil
		debug.FreeOSMemory()
		if err != nil {
			js.Global().Call("postMessage", map[string]any{"type": "error", "message": err.Error()})
			return
		}
		image := js.Global().Get("Uint8Array").New(len(result.Image))
		js.CopyBytesToJS(image, result.Image)
		js.Global().Call("postMessage", map[string]any{
			"type": "complete", "image": image, "imageSHA256": result.ImageSHA256, "sshEnabled": result.SSHEnabled,
		}, []any{image.Get("buffer")})
		result = memoryBuildResult{}
		debug.FreeOSMemory()
	}()
	return nil
}
