package main

import (
	"bytes"
	"crypto/ed25519"
	"crypto/rand"
	"crypto/rsa"
	"encoding/pem"
	"testing"

	"golang.org/x/crypto/ssh"
)

func TestNormalizeOwnerPublicKey(t *testing.T) {
	public, private, err := ed25519.GenerateKey(rand.Reader)
	if err != nil {
		t.Fatal(err)
	}
	sshPublic, err := ssh.NewPublicKey(public)
	if err != nil {
		t.Fatal(err)
	}
	canonical := ssh.MarshalAuthorizedKey(sshPublic)
	withComment := append(append([]byte(nil), bytes.TrimSpace(canonical)...), []byte(" owner@studio\n")...)
	got, err := normalizeOwnerPublicKey(withComment)
	if err != nil {
		t.Fatalf("valid Ed25519 key with comment rejected: %v", err)
	}
	if !bytes.Equal(got, canonical) {
		t.Fatal("public key was not normalized to its canonical comment-free form")
	}

	rsaPrivate, err := rsa.GenerateKey(rand.Reader, 1024)
	if err != nil {
		t.Fatal(err)
	}
	rsaPublic, err := ssh.NewPublicKey(&rsaPrivate.PublicKey)
	if err != nil {
		t.Fatal(err)
	}
	privateBlock, err := ssh.MarshalPrivateKey(private, "must reject")
	if err != nil {
		t.Fatal(err)
	}
	cases := map[string][]byte{
		"empty":           nil,
		"malformed":       []byte("this is not a key\n"),
		"rsa":             ssh.MarshalAuthorizedKey(rsaPublic),
		"private":         pem.EncodeToMemory(privateBlock),
		"multiple":        append(append([]byte(nil), canonical...), canonical...),
		"options":         append([]byte(`from="127.0.0.1" `), canonical...),
		"leading comment": append([]byte("# owner key\n"), canonical...),
	}
	for name, input := range cases {
		t.Run(name, func(t *testing.T) {
			if _, err := normalizeOwnerPublicKey(input); err == nil {
				t.Fatal("invalid owner key accepted")
			}
		})
	}
}

func TestPatchVariantSelection(t *testing.T) {
	disabled, _, placeholder, err := loadPatch(false)
	if err != nil {
		t.Fatal(err)
	}
	if disabled.SSHEnabled || disabled.KeyOffset != -1 || len(placeholder) != 0 {
		t.Fatal("default patch does not explicitly disable SSH")
	}
	enabled, _, placeholder, err := loadPatch(true)
	if err != nil {
		t.Fatal(err)
	}
	if !enabled.SSHEnabled || enabled.KeyOffset < 0 || len(placeholder) != 81 {
		t.Fatal("owner-key patch does not expose its verified 81-byte key slot")
	}
}
