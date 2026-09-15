//go:build !js

package main

import (
	"crypto/sha256"
	"encoding/hex"
	"errors"
	"fmt"
	"os"
	"path/filepath"
)

type buildResult struct {
	ImagePath   string
	ImageSHA256 string
	SSHEnabled  bool
}

func buildImage(inputPath, outputPath string, ownerPublicKey []byte, progress func(int, string)) (buildResult, error) {
	progress(2, "Checking embedded MCU patch...")
	sshEnabled := len(ownerPublicKey) != 0
	var authorized []byte
	var err error
	if sshEnabled {
		authorized, err = normalizeOwnerPublicKey(ownerPublicKey)
		if err != nil {
			return buildResult{}, err
		}
	}
	manifest, patch, placeholder, err := loadPatch(sshEnabled)
	if err != nil {
		return buildResult{}, err
	}
	progress(5, "Verifying official MPC image...")
	layout, err := validateOfficial(inputPath)
	if err != nil {
		return buildResult{}, err
	}
	outputPath, err = filepath.Abs(outputPath)
	if err != nil {
		return buildResult{}, err
	}
	if _, err := os.Stat(outputPath); err == nil {
		return buildResult{}, errors.New("the selected output image already exists; choose a new filename")
	} else if !os.IsNotExist(err) {
		return buildResult{}, fmt.Errorf("check output image: %w", err)
	}
	outputDir := filepath.Dir(outputPath)
	workDir, err := os.MkdirTemp(outputDir, ".mpclearn-image-*")
	if err != nil {
		return buildResult{}, fmt.Errorf("create temporary build folder: %w", err)
	}
	defer os.RemoveAll(workDir)
	rootPath := filepath.Join(workDir, "rootfs.ext")
	progress(10, "Extracting and checking the official root filesystem...")
	originalRootSHA, err := decodeRoot(inputPath, rootPath, layout, func(done int64) {
		progress(10+int(done*30/rootSize), "Extracting and checking the official root filesystem...")
	})
	if err != nil {
		return buildResult{}, err
	}
	if originalRootSHA != manifest.OriginalSHA256 {
		return buildResult{}, errors.New("official root filesystem does not match this MPC 3.9.1 patch")
	}
	progress(42, "Applying the qualified MCU payload...")
	if err := applyPatch(rootPath, manifest, patch, placeholder); err != nil {
		return buildResult{}, err
	}
	finalRootSHA := manifest.PatchedSHA256
	if sshEnabled {
		progress(48, "Installing your owner public key...")
		finalRootSHA, err = replaceOwnerKey(rootPath, manifest.KeyOffset, authorized)
		if err != nil {
			return buildResult{}, fmt.Errorf("insert owner SSH key: %w", err)
		}
	} else {
		progress(48, "Confirming SSH is disabled...")
	}
	temporaryImage := filepath.Join(workDir, filepath.Base(outputPath))
	progress(50, "Compressing the personal MCU image. This takes several minutes...")
	imageSHA, err := encodeImage(rootPath, temporaryImage, layout, func(done int64) {
		progress(50+int(done*38/rootSize), "Compressing the personal MCU image. This takes several minutes...")
	})
	if err != nil {
		return buildResult{}, err
	}
	progress(90, "Decoding the finished image for independent verification...")
	finishedLayout, err := inspectImage(temporaryImage)
	if err != nil {
		return buildResult{}, fmt.Errorf("verify finished image structure: %w", err)
	}
	verifiedRoot := filepath.Join(workDir, "verified-rootfs.ext")
	verifiedSHA, err := decodeRoot(temporaryImage, verifiedRoot, finishedLayout, func(done int64) {
		progress(90+int(done*8/rootSize), "Decoding the finished image for independent verification...")
	})
	if err != nil {
		return buildResult{}, fmt.Errorf("verify finished image root filesystem: %w", err)
	}
	if verifiedSHA != finalRootSHA {
		return buildResult{}, errors.New("finished image root filesystem differs from the patched source")
	}
	progress(99, "Saving the verified image...")
	if err := os.Rename(temporaryImage, outputPath); err != nil {
		return buildResult{}, fmt.Errorf("save verified image: %w", err)
	}
	// Confirm the destination bytes after the final move.
	destinationDigest, err := hashFile(outputPath, sha256.New())
	if err != nil {
		return buildResult{}, fmt.Errorf("check saved image: %w", err)
	}
	if hex.EncodeToString(destinationDigest) != imageSHA {
		return buildResult{}, errors.New("saved image checksum changed during the final move")
	}
	return buildResult{ImagePath: outputPath, ImageSHA256: imageSHA, SSHEnabled: sshEnabled}, nil
}
