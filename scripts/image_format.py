#!/usr/bin/env python3
"""Strict independent decoder for the one-partition AZ01 firmware used here."""
import hashlib, lzma, pathlib, struct, sys
OFFICIAL_SHA = '4c0f7797a006313fad206f9513eca8b3823e834c9e0941a6b5f088386cb835d6'
ROOT_SIZE = 441279488

def decode(path, output=None, reference=None):
    b = pathlib.Path(path).read_bytes()
    assert b[:4] == b'AZ01' and struct.unpack_from('<III', b, 4) == (1,264,23)
    assert b[16:40].endswith(b'\0')
    pos = 40
    count, = struct.unpack_from('<I', b, pos); pos += 4
    assert count == 8
    names = []
    for _ in range(count):
        length, = struct.unpack_from('<I', b, pos); pos += 4
        assert 1 <= length <= 16
        value = b[pos:pos+16]; pos += 16
        assert value[length:] == bytes(16-length)
        names.append(value[:length].decode('ascii'))
    assert 'inmusic,acvb' in names
    ids, = struct.unpack_from('<I', b, pos); pos += 4
    assert ids == count
    pos += ids * 4
    length, = struct.unpack_from('<I', b, pos); pos += 4
    assert 0 < length <= 256
    pos += length
    assert not any(b[pos:264])
    assert b[264:272] == b'PARTL\0\0\0'
    size, = struct.unpack_from('<Q', b, 272)
    assert b[280:320] == bytes.fromhex('06000000726f6f746673000002000000787a00000100000004000000736861310000000014000000')
    payload = b[340:340+size]
    assert len(payload) == size and hashlib.sha1(payload).digest() == b[320:340]
    end = 340+size; aligned = (end+7)//8*8
    assert not any(b[end:aligned])
    assert b[aligned:] == b'EOF\0\x10\0\0\0'+bytes(8)
    dec = lzma.LZMADecompressor(format=lzma.FORMAT_XZ)
    root = dec.decompress(payload)
    assert dec.eof and not dec.unused_data and len(root) == ROOT_SIZE
    if reference:
        original = pathlib.Path(reference).read_bytes()
        assert b[:272] == original[:272] and b[280:320] == original[280:320]
        assert b[-16:] == original[-16:]
    if output:
        pathlib.Path(output).write_bytes(root)
    return {'image_sha256': hashlib.sha256(b).hexdigest(), 'image_bytes': len(b),
            'rootfs_sha256': hashlib.sha256(root).hexdigest(), 'rootfs_bytes': len(root),
            'compressed_bytes': size, 'compatible': names}

if __name__ == '__main__':
    import json
    print(json.dumps(decode(*sys.argv[1:]), indent=2))
