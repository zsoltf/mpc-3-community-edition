package main

import (
	"crypto/sha256"
	"encoding/hex"
	"errors"
	"fmt"
)

type memoryBuildResult struct {
	Image       []byte
	ImageSHA256 string
	SSHEnabled  bool
}

func buildImageMemory(input, ownerPublicKey []byte, progress func(int, string)) (memoryBuildResult, error) {
	progress(2, "Checking embedded MCU patch...")
	sshEnabled := len(ownerPublicKey) != 0
	var authorized []byte
	var err error
	if sshEnabled {
		authorized, err = normalizeOwnerPublicKey(ownerPublicKey)
		if err != nil {
			return memoryBuildResult{}, err
		}
	}
	manifest, patch, placeholder, err := loadPatch(sshEnabled)
	if err != nil {
		return memoryBuildResult{}, err
	}
	progress(5, "Verifying official MPC image...")
	layout, err := validateOfficialBytes(input)
	if err != nil {
		return memoryBuildResult{}, err
	}
	progress(10, "Extracting and checking the official root filesystem...")
	root, originalRootSHA, err := decodeRootBytes(input, layout, func(done int64) {
		progress(10+int(done*30/rootSize), "Extracting and checking the official root filesystem...")
	})
	if err != nil {
		return memoryBuildResult{}, err
	}
	if originalRootSHA != manifest.OriginalSHA256 {
		return memoryBuildResult{}, errors.New("official root filesystem does not match this MPC 3.9.1 patch")
	}
	progress(42, "Applying the qualified MCU payload...")
	if err := applyPatchBytes(root, manifest, patch, placeholder); err != nil {
		return memoryBuildResult{}, err
	}
	if sshEnabled {
		progress(48, "Installing your owner public key...")
		copy(root[manifest.KeyOffset:manifest.KeyOffset+int64(len(authorized))], authorized)
	} else {
		progress(48, "Confirming SSH is disabled...")
	}
	rootDigest := sha256.Sum256(root)
	finalRootSHA := hex.EncodeToString(rootDigest[:])
	progress(50, "Compressing the personal MCU image. This takes several minutes...")
	image, imageSHA, err := encodeImageBytes(root, layout, func(done int64) {
		progress(50+int(done*38/rootSize), "Compressing the personal MCU image. This takes several minutes...")
	})
	if err != nil {
		return memoryBuildResult{}, err
	}
	root = nil
	progress(90, "Decoding the finished image for independent verification...")
	if err := verifyImageBytes(image, finalRootSHA, func(done int64) {
		progress(90+int(done*9/rootSize), "Decoding the finished image for independent verification...")
	}); err != nil {
		return memoryBuildResult{}, fmt.Errorf("verify finished image: %w", err)
	}
	return memoryBuildResult{
		Image: image, ImageSHA256: imageSHA, SSHEnabled: sshEnabled,
	}, nil
}
