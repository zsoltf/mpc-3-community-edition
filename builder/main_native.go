//go:build !js

package main

import (
	"flag"
	"fmt"
	"os"
)

func main() {
	input := flag.String("input", "", "official MPC 3.9.1 Gen1 update image")
	output := flag.String("output", "", "new personal -update.img path")
	publicKeyPath := flag.String("public-key", "", "optional owner Ed25519 public key; SSH stays disabled when omitted")
	flag.Parse()
	if *input == "" || *output == "" {
		fmt.Fprintln(os.Stderr, "Developer verification harness: -input and -output are required")
		os.Exit(2)
	}
	var publicKey []byte
	if *publicKeyPath != "" {
		var err error
		publicKey, err = os.ReadFile(*publicKeyPath)
		if err != nil {
			fmt.Fprintln(os.Stderr, "ERROR: read public key:", err)
			os.Exit(1)
		}
	}
	last := -1
	result, err := buildImage(*input, normalizedOutputPath(*output), publicKey, func(percent int, message string) {
		if percent != last {
			last = percent
			fmt.Printf("%3d%% %s\n", percent, message)
		}
	})
	if err != nil {
		fmt.Fprintln(os.Stderr, "ERROR:", err)
		os.Exit(1)
	}
	fmt.Printf("PASS\nImage: %s\nSSH enabled: %t\nSHA-256: %s\n", result.ImagePath, result.SSHEnabled, result.ImageSHA256)
}

func normalizedOutputPath(path string) string {
	if len(path) >= len("-update.img") && path[len(path)-len("-update.img"):] == "-update.img" {
		return path
	}
	if len(path) >= 4 && path[len(path)-4:] == ".img" {
		path = path[:len(path)-4]
	}
	return path + "-update.img"
}
