package main

import (
	"bytes"
	"crypto/sha256"
	"encoding/base64"
	"encoding/hex"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"os"

	"golang.org/x/crypto/ssh"
)

type patchManifest struct {
	Version        int         `json:"version"`
	OriginalSize   int64       `json:"original_size"`
	OriginalSHA256 string      `json:"original_sha256"`
	PatchedSHA256  string      `json:"patched_sha256"`
	KeyOffset      int64       `json:"key_offset"`
	KeyPlaceholder string      `json:"key_placeholder"`
	PatchSHA256    string      `json:"patch_sha256"`
	SSHEnabled     bool        `json:"ssh_enabled"`
	Spans          []patchSpan `json:"spans"`
}

type patchSpan struct {
	Offset      int64 `json:"offset"`
	Length      int64 `json:"length"`
	PatchOffset int64 `json:"patch_offset"`
}

func loadPatch(sshEnabled bool) (patchManifest, []byte, []byte, error) {
	variant := "no-ssh"
	if sshEnabled {
		variant = "ssh"
	}
	manifestBytes, err := assets.ReadFile("resources/" + variant + "/manifest.json")
	if err != nil {
		return patchManifest{}, nil, nil, err
	}
	patch, err := assets.ReadFile("resources/" + variant + "/patch.bin")
	if err != nil {
		return patchManifest{}, nil, nil, err
	}
	var manifest patchManifest
	if err := json.Unmarshal(manifestBytes, &manifest); err != nil {
		return patchManifest{}, nil, nil, fmt.Errorf("invalid embedded patch manifest: %w", err)
	}
	var placeholder []byte
	if manifest.KeyPlaceholder != "" {
		placeholder, err = base64.StdEncoding.Strict().DecodeString(manifest.KeyPlaceholder)
		if err != nil {
			return patchManifest{}, nil, nil, errors.New("embedded owner-key placeholder is invalid")
		}
	}
	validKeySlot := !manifest.SSHEnabled && manifest.KeyOffset == -1 && len(placeholder) == 0
	if manifest.SSHEnabled {
		validKeySlot = manifest.KeyOffset >= 0 && manifest.KeyOffset+81 <= rootSize && len(placeholder) == 81
	}
	if manifest.Version != 1 || manifest.OriginalSize != rootSize || manifest.SSHEnabled != sshEnabled || !validKeySlot {
		return patchManifest{}, nil, nil, errors.New("embedded patch manifest has unsupported values")
	}
	patchDigest := sha256.Sum256(patch)
	if hex.EncodeToString(patchDigest[:]) != manifest.PatchSHA256 {
		return patchManifest{}, nil, nil, errors.New("embedded patch data checksum failed")
	}
	var rootEnd, patchEnd int64
	for i, span := range manifest.Spans {
		if span.Length <= 0 || span.Offset < rootEnd || span.PatchOffset != patchEnd || span.Offset+span.Length > manifest.OriginalSize || span.PatchOffset+span.Length > int64(len(patch)) {
			return patchManifest{}, nil, nil, fmt.Errorf("embedded patch span %d is invalid", i)
		}
		rootEnd = span.Offset + span.Length
		patchEnd = span.PatchOffset + span.Length
	}
	if patchEnd != int64(len(patch)) {
		return patchManifest{}, nil, nil, errors.New("embedded patch data has unused bytes")
	}
	return manifest, patch, placeholder, nil
}

func applyPatch(rootPath string, manifest patchManifest, patch, placeholder []byte) error {
	f, err := os.OpenFile(rootPath, os.O_RDWR, 0)
	if err != nil {
		return err
	}
	defer f.Close()
	for i, span := range manifest.Spans {
		data := patch[span.PatchOffset : span.PatchOffset+span.Length]
		if _, err := f.WriteAt(data, span.Offset); err != nil {
			return fmt.Errorf("apply embedded patch span %d: %w", i, err)
		}
	}
	if err := f.Sync(); err != nil {
		return err
	}
	digest, err := hashOpenFile(f)
	if err != nil {
		return err
	}
	if digest != manifest.PatchedSHA256 {
		return errors.New("patched root filesystem checksum failed before owner key insertion")
	}
	if manifest.SSHEnabled {
		actual := make([]byte, len(placeholder))
		if _, err := f.ReadAt(actual, manifest.KeyOffset); err != nil {
			return fmt.Errorf("read owner-key slot: %w", err)
		}
		if !bytes.Equal(actual, placeholder) {
			return errors.New("owner-key slot does not contain the expected placeholder")
		}
	}
	return nil
}

func applyPatchBytes(root []byte, manifest patchManifest, patch, placeholder []byte) error {
	if int64(len(root)) != manifest.OriginalSize {
		return errors.New("official root filesystem has an unexpected size")
	}
	for _, span := range manifest.Spans {
		copy(root[span.Offset:span.Offset+span.Length], patch[span.PatchOffset:span.PatchOffset+span.Length])
	}
	digest := sha256.Sum256(root)
	if hex.EncodeToString(digest[:]) != manifest.PatchedSHA256 {
		return errors.New("patched root filesystem checksum failed before owner key insertion")
	}
	if manifest.SSHEnabled && !bytes.Equal(root[manifest.KeyOffset:manifest.KeyOffset+int64(len(placeholder))], placeholder) {
		return errors.New("owner-key slot does not contain the expected placeholder")
	}
	return nil
}

func normalizeOwnerPublicKey(input []byte) ([]byte, error) {
	trimmed := bytes.TrimSpace(input)
	if len(trimmed) == 0 || len(trimmed) > 16*1024 || bytes.Contains(trimmed, []byte("PRIVATE KEY")) {
		return nil, errors.New("owner key must be one Ed25519 OpenSSH public key")
	}
	public, _, options, rest, err := ssh.ParseAuthorizedKey(trimmed)
	if err != nil {
		return nil, errors.New("owner key is not a valid OpenSSH public key")
	}
	if len(options) != 0 || len(bytes.TrimSpace(rest)) != 0 || public.Type() != ssh.KeyAlgoED25519 {
		return nil, errors.New("owner key must contain exactly one Ed25519 public key without authorization options")
	}
	authorized := ssh.MarshalAuthorizedKey(public)
	if len(authorized) != 81 {
		return nil, errors.New("owner Ed25519 public key has an unsupported encoding")
	}
	canonical := bytes.TrimSpace(authorized)
	if !bytes.HasPrefix(trimmed, canonical) {
		return nil, errors.New("owner key file contains content before the Ed25519 public key")
	}
	suffix := trimmed[len(canonical):]
	if len(suffix) > 0 && (suffix[0] != ' ' && suffix[0] != '\t') {
		return nil, errors.New("owner key file has invalid trailing content")
	}
	if bytes.ContainsAny(suffix, "\r\n") {
		return nil, errors.New("owner key file must contain exactly one public key line")
	}
	return authorized, nil
}

func replaceOwnerKey(rootPath string, offset int64, authorized []byte) (string, error) {
	if len(authorized) != 81 {
		return "", errors.New("owner public key must occupy exactly 81 bytes")
	}
	f, err := os.OpenFile(rootPath, os.O_RDWR, 0)
	if err != nil {
		return "", err
	}
	defer f.Close()
	if _, err := f.WriteAt(authorized, offset); err != nil {
		return "", err
	}
	if err := f.Sync(); err != nil {
		return "", err
	}
	return hashOpenFile(f)
}

func hashOpenFile(f *os.File) (string, error) {
	if _, err := f.Seek(0, io.SeekStart); err != nil {
		return "", err
	}
	h := sha256.New()
	if _, err := io.Copy(h, f); err != nil {
		return "", err
	}
	return hex.EncodeToString(h.Sum(nil)), nil
}
