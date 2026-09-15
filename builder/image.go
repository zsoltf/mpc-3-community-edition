package main

import (
	"bytes"
	"crypto/sha1"
	"crypto/sha256"
	"encoding/binary"
	"encoding/hex"
	"errors"
	"fmt"
	"hash"
	"io"
	"os"

	"github.com/ulikunitz/xz"
)

const (
	officialImageSHA256 = "4c0f7797a006313fad206f9513eca8b3823e834c9e0941a6b5f088386cb835d6"
	rootSize            = int64(441279488)
	payloadOffset       = int64(340)
)

var partitionDescriptor = []byte{
	0x06, 0x00, 0x00, 0x00, 0x72, 0x6f, 0x6f, 0x74,
	0x66, 0x73, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
	0x78, 0x7a, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
	0x04, 0x00, 0x00, 0x00, 0x73, 0x68, 0x61, 0x31,
	0x00, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00,
}

var imageFooter = append([]byte{'E', 'O', 'F', 0, 0x10, 0, 0, 0}, make([]byte, 8)...)

type imageLayout struct {
	Header         []byte
	CompressedSize int64
	CompressedSHA1 [sha1.Size]byte
	Footer         []byte
}

func validateOfficial(path string) (imageLayout, error) {
	digest, err := hashFile(path, sha256.New())
	if err != nil {
		return imageLayout{}, fmt.Errorf("read selected image: %w", err)
	}
	if hex.EncodeToString(digest) != officialImageSHA256 {
		return imageLayout{}, errors.New("the selected file is not the exact official MPC 3.9.1 Gen1 USB image")
	}
	return inspectImage(path)
}

func inspectImage(path string) (imageLayout, error) {
	f, err := os.Open(path)
	if err != nil {
		return imageLayout{}, err
	}
	defer f.Close()
	stat, err := f.Stat()
	if err != nil {
		return imageLayout{}, err
	}
	header := make([]byte, payloadOffset)
	if _, err := io.ReadFull(f, header); err != nil {
		return imageLayout{}, errors.New("image header is truncated")
	}
	if !bytes.Equal(header[:4], []byte("AZ01")) || binary.LittleEndian.Uint32(header[4:8]) != 1 || binary.LittleEndian.Uint32(header[8:12]) != 264 || binary.LittleEndian.Uint32(header[12:16]) != 23 || header[39] != 0 {
		return imageLayout{}, errors.New("image has an unsupported AZ01 header")
	}
	pos := 40
	if binary.LittleEndian.Uint32(header[pos:pos+4]) != 8 {
		return imageLayout{}, errors.New("image compatibility table is not MPC 3.9.1 Gen1")
	}
	pos += 4
	foundACVB := false
	for i := 0; i < 8; i++ {
		length := int(binary.LittleEndian.Uint32(header[pos : pos+4]))
		pos += 4
		if length < 1 || length > 16 || !allZero(header[pos+length:pos+16]) {
			return imageLayout{}, errors.New("image compatibility table is malformed")
		}
		if string(header[pos:pos+length]) == "inmusic,acvb" {
			foundACVB = true
		}
		pos += 16
	}
	if !foundACVB || binary.LittleEndian.Uint32(header[pos:pos+4]) != 8 {
		return imageLayout{}, errors.New("image is not compatible with the qualified MPC Live II target")
	}
	pos += 4 + 8*4
	descriptionLength := int(binary.LittleEndian.Uint32(header[pos : pos+4]))
	pos += 4
	if descriptionLength < 1 || descriptionLength > 256 || pos+descriptionLength > 264 || !allZero(header[pos+descriptionLength:264]) {
		return imageLayout{}, errors.New("image description is malformed")
	}
	if !bytes.Equal(header[264:272], []byte{'P', 'A', 'R', 'T', 'L', 0, 0, 0}) || !bytes.Equal(header[280:320], partitionDescriptor) {
		return imageLayout{}, errors.New("image partition format is unsupported")
	}
	compressedSize := int64(binary.LittleEndian.Uint64(header[272:280]))
	if compressedSize <= 0 {
		return imageLayout{}, errors.New("image contains an empty root filesystem")
	}
	end := payloadOffset + compressedSize
	aligned := (end + 7) / 8 * 8
	if stat.Size() != aligned+int64(len(imageFooter)) {
		return imageLayout{}, errors.New("image has unexpected data after its root filesystem")
	}
	tail := make([]byte, stat.Size()-end)
	if _, err := f.ReadAt(tail, end); err != nil {
		return imageLayout{}, errors.New("image trailer is truncated")
	}
	pad := aligned - end
	if !allZero(tail[:pad]) || !bytes.Equal(tail[pad:], imageFooter) {
		return imageLayout{}, errors.New("image trailer is malformed")
	}
	var compressedSHA1 [sha1.Size]byte
	copy(compressedSHA1[:], header[320:340])
	return imageLayout{Header: header, CompressedSize: compressedSize, CompressedSHA1: compressedSHA1, Footer: append([]byte(nil), tail[pad:]...)}, nil
}

func inspectImageBytes(image []byte) (imageLayout, error) {
	if len(image) < int(payloadOffset)+len(imageFooter) {
		return imageLayout{}, errors.New("image is truncated")
	}
	header := append([]byte(nil), image[:payloadOffset]...)
	if !bytes.Equal(header[:4], []byte("AZ01")) || binary.LittleEndian.Uint32(header[4:8]) != 1 || binary.LittleEndian.Uint32(header[8:12]) != 264 || binary.LittleEndian.Uint32(header[12:16]) != 23 || header[39] != 0 {
		return imageLayout{}, errors.New("image has an unsupported AZ01 header")
	}
	pos := 40
	if binary.LittleEndian.Uint32(header[pos:pos+4]) != 8 {
		return imageLayout{}, errors.New("image compatibility table is not MPC 3.9.1 Gen1")
	}
	pos += 4
	foundACVB := false
	for i := 0; i < 8; i++ {
		length := int(binary.LittleEndian.Uint32(header[pos : pos+4]))
		pos += 4
		if length < 1 || length > 16 || !allZero(header[pos+length:pos+16]) {
			return imageLayout{}, errors.New("image compatibility table is malformed")
		}
		if string(header[pos:pos+length]) == "inmusic,acvb" {
			foundACVB = true
		}
		pos += 16
	}
	if !foundACVB || binary.LittleEndian.Uint32(header[pos:pos+4]) != 8 {
		return imageLayout{}, errors.New("image is not compatible with the qualified MPC Live II target")
	}
	pos += 4 + 8*4
	descriptionLength := int(binary.LittleEndian.Uint32(header[pos : pos+4]))
	pos += 4
	if descriptionLength < 1 || descriptionLength > 256 || pos+descriptionLength > 264 || !allZero(header[pos+descriptionLength:264]) {
		return imageLayout{}, errors.New("image description is malformed")
	}
	if !bytes.Equal(header[264:272], []byte{'P', 'A', 'R', 'T', 'L', 0, 0, 0}) || !bytes.Equal(header[280:320], partitionDescriptor) {
		return imageLayout{}, errors.New("image partition format is unsupported")
	}
	compressedSize := int64(binary.LittleEndian.Uint64(header[272:280]))
	if compressedSize <= 0 {
		return imageLayout{}, errors.New("image contains an empty root filesystem")
	}
	end := payloadOffset + compressedSize
	aligned := (end + 7) / 8 * 8
	if int64(len(image)) != aligned+int64(len(imageFooter)) {
		return imageLayout{}, errors.New("image has unexpected data after its root filesystem")
	}
	if !allZero(image[end:aligned]) || !bytes.Equal(image[aligned:], imageFooter) {
		return imageLayout{}, errors.New("image trailer is malformed")
	}
	var compressedSHA1 [sha1.Size]byte
	copy(compressedSHA1[:], header[320:340])
	return imageLayout{Header: header, CompressedSize: compressedSize, CompressedSHA1: compressedSHA1, Footer: append([]byte(nil), imageFooter...)}, nil
}

func validateOfficialBytes(image []byte) (imageLayout, error) {
	digest := sha256.Sum256(image)
	if hex.EncodeToString(digest[:]) != officialImageSHA256 {
		return imageLayout{}, errors.New("the selected file is not the exact official MPC 3.9.1 Gen1 USB image")
	}
	return inspectImageBytes(image)
}

func decodeRootBytes(image []byte, layout imageLayout, progress func(done int64)) ([]byte, string, error) {
	end := payloadOffset + layout.CompressedSize
	payload := image[payloadOffset:end]
	compressedHash := sha1.Sum(payload)
	if compressedHash != layout.CompressedSHA1 {
		return nil, "", errors.New("compressed root filesystem checksum does not match the image")
	}
	reader, err := xz.NewReader(bytes.NewReader(payload))
	if err != nil {
		return nil, "", fmt.Errorf("open compressed root filesystem: %w", err)
	}
	var root bytes.Buffer
	root.Grow(int(rootSize))
	rootHash := sha256.New()
	written, err := copyWithProgress(io.MultiWriter(&root, rootHash), reader, rootSize, progress)
	if err != nil {
		return nil, "", fmt.Errorf("decompress root filesystem: %w", err)
	}
	if written != rootSize {
		return nil, "", fmt.Errorf("root filesystem has unexpected size %d", written)
	}
	return root.Bytes(), hex.EncodeToString(rootHash.Sum(nil)), nil
}

func encodeImageBytes(root []byte, layout imageLayout, progress func(done int64)) ([]byte, string, error) {
	var compressed bytes.Buffer
	compressed.Grow(len(root) / 3)
	compressedHash := sha1.New()
	encoder, err := (xz.WriterConfig{CheckSum: xz.CRC32}).NewWriter(io.MultiWriter(&compressed, compressedHash))
	if err != nil {
		return nil, "", fmt.Errorf("start root filesystem compression: %w", err)
	}
	if _, err := copyWithProgress(encoder, bytes.NewReader(root), rootSize, progress); err != nil {
		return nil, "", fmt.Errorf("compress root filesystem: %w", err)
	}
	if err := encoder.Close(); err != nil {
		return nil, "", fmt.Errorf("finish root filesystem compression: %w", err)
	}
	header := append([]byte(nil), layout.Header...)
	binary.LittleEndian.PutUint64(header[272:280], uint64(compressed.Len()))
	copy(header[320:340], compressedHash.Sum(nil))
	end := int(payloadOffset) + compressed.Len()
	padding := (8 - end%8) % 8
	result := make([]byte, end+padding+len(layout.Footer))
	copy(result, header)
	copy(result[payloadOffset:], compressed.Bytes())
	copy(result[end+padding:], layout.Footer)
	digest := sha256.Sum256(result)
	return result, hex.EncodeToString(digest[:]), nil
}

func verifyImageBytes(image []byte, expectedRootSHA string, progress func(done int64)) error {
	layout, err := inspectImageBytes(image)
	if err != nil {
		return err
	}
	end := payloadOffset + layout.CompressedSize
	payload := image[payloadOffset:end]
	compressedHash := sha1.Sum(payload)
	if compressedHash != layout.CompressedSHA1 {
		return errors.New("finished compressed root filesystem checksum failed")
	}
	reader, err := xz.NewReader(bytes.NewReader(payload))
	if err != nil {
		return err
	}
	h := sha256.New()
	written, err := copyWithProgress(h, reader, rootSize, progress)
	if err != nil {
		return err
	}
	if written != rootSize || hex.EncodeToString(h.Sum(nil)) != expectedRootSHA {
		return errors.New("finished image root filesystem differs from the patched source")
	}
	return nil
}

func decodeRoot(imagePath, rootPath string, layout imageLayout, progress func(done int64)) (string, error) {
	in, err := os.Open(imagePath)
	if err != nil {
		return "", err
	}
	defer in.Close()
	if _, err := in.Seek(payloadOffset, io.SeekStart); err != nil {
		return "", err
	}
	limited := &io.LimitedReader{R: in, N: layout.CompressedSize}
	compressedHash := sha1.New()
	reader, err := xz.NewReader(io.TeeReader(limited, compressedHash))
	if err != nil {
		return "", fmt.Errorf("open compressed root filesystem: %w", err)
	}
	out, err := os.OpenFile(rootPath, os.O_CREATE|os.O_EXCL|os.O_WRONLY, 0600)
	if err != nil {
		return "", err
	}
	rootHash := sha256.New()
	written, copyErr := copyWithProgress(io.MultiWriter(out, rootHash), reader, rootSize, progress)
	closeErr := out.Close()
	if copyErr != nil {
		return "", fmt.Errorf("decompress root filesystem: %w", copyErr)
	}
	if closeErr != nil {
		return "", closeErr
	}
	if written != rootSize {
		return "", fmt.Errorf("root filesystem has unexpected size %d", written)
	}
	if limited.N != 0 || !bytes.Equal(compressedHash.Sum(nil), layout.CompressedSHA1[:]) {
		return "", errors.New("compressed root filesystem checksum does not match the image")
	}
	return hex.EncodeToString(rootHash.Sum(nil)), nil
}

func encodeImage(rootPath, outputPath string, layout imageLayout, progress func(done int64)) (string, error) {
	in, err := os.Open(rootPath)
	if err != nil {
		return "", err
	}
	defer in.Close()
	out, err := os.OpenFile(outputPath, os.O_CREATE|os.O_EXCL|os.O_RDWR, 0600)
	if err != nil {
		return "", err
	}
	ok := false
	defer func() {
		_ = out.Close()
		if !ok {
			_ = os.Remove(outputPath)
		}
	}()
	header := append([]byte(nil), layout.Header...)
	for i := 272; i < 280; i++ {
		header[i] = 0
	}
	for i := 320; i < 340; i++ {
		header[i] = 0
	}
	if _, err := out.Write(header); err != nil {
		return "", err
	}
	compressedHash := sha1.New()
	counter := &countingWriter{Writer: io.MultiWriter(out, compressedHash)}
	encoder, err := (xz.WriterConfig{CheckSum: xz.CRC32}).NewWriter(counter)
	if err != nil {
		return "", fmt.Errorf("start root filesystem compression: %w", err)
	}
	if _, err := copyWithProgress(encoder, in, rootSize, progress); err != nil {
		return "", fmt.Errorf("compress root filesystem: %w", err)
	}
	if err := encoder.Close(); err != nil {
		return "", fmt.Errorf("finish root filesystem compression: %w", err)
	}
	compressedSize := counter.N
	end := payloadOffset + compressedSize
	padding := (8 - end%8) % 8
	if _, err := out.Write(make([]byte, padding)); err != nil {
		return "", err
	}
	if _, err := out.Write(layout.Footer); err != nil {
		return "", err
	}
	binary.LittleEndian.PutUint64(header[272:280], uint64(compressedSize))
	copy(header[320:340], compressedHash.Sum(nil))
	if _, err := out.WriteAt(header, 0); err != nil {
		return "", err
	}
	if err := out.Sync(); err != nil {
		return "", err
	}
	if err := out.Close(); err != nil {
		return "", err
	}
	digest, err := hashFile(outputPath, sha256.New())
	if err != nil {
		return "", err
	}
	ok = true
	return hex.EncodeToString(digest), nil
}

func allZero(data []byte) bool {
	for _, b := range data {
		if b != 0 {
			return false
		}
	}
	return true
}

type countingWriter struct {
	io.Writer
	N int64
}

func (w *countingWriter) Write(p []byte) (int, error) {
	n, err := w.Writer.Write(p)
	w.N += int64(n)
	return n, err
}

func hashFile(path string, h hash.Hash) ([]byte, error) {
	f, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	defer f.Close()
	if _, err := io.Copy(h, f); err != nil {
		return nil, err
	}
	return h.Sum(nil), nil
}

func copyWithProgress(dst io.Writer, src io.Reader, total int64, progress func(int64)) (int64, error) {
	buffer := make([]byte, 1024*1024)
	var written int64
	last := int64(-1)
	for {
		n, readErr := src.Read(buffer)
		if n > 0 {
			wn, writeErr := dst.Write(buffer[:n])
			written += int64(wn)
			if writeErr != nil {
				return written, writeErr
			}
			if wn != n {
				return written, io.ErrShortWrite
			}
			percent := written * 100 / total
			if percent != last {
				last = percent
				progress(written)
			}
		}
		if readErr == io.EOF {
			return written, nil
		}
		if readErr != nil {
			return written, readErr
		}
	}
}
