#!/usr/bin/env python3
"""Prepare byte replacements for an exactly pinned rootfs, without original data."""
import base64, hashlib, json, mmap, pathlib, sys
no_ssh = '--no-ssh' in sys.argv
args = [arg for arg in sys.argv[1:] if arg != '--no-ssh']
original, patched, public, output = map(pathlib.Path, args)
output.mkdir(parents=True, exist_ok=False)
key=b'' if no_ssh else public.read_bytes()
if not no_ssh:
 assert len(key)==81 and key.startswith(b'ssh-ed25519 ') and key.endswith(b'\n')
with original.open('rb') as f, patched.open('rb') as g:
 with mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ) as a, mmap.mmap(g.fileno(),0,access=mmap.ACCESS_READ) as b:
  assert len(a)==len(b)==441279488
  offset=-1 if no_ssh else b.find(key)
  if not no_ssh: assert offset>=0 and b.find(key,offset+1)<0
  spans=[];data=bytearray()
  for start in range(0,len(a),4096):
   before,after=a[start:start+4096],b[start:start+4096]
   if before==after: continue
   at=0
   while at<len(before):
    if before[at]==after[at]: at+=1;continue
    end=at+1
    while end<len(before) and before[end]!=after[end]: end+=1
    spans.append({'offset':start+at,'length':end-at,'patch_offset':len(data)})
    data.extend(after[at:end]);at=end
  (output/'patch.bin').write_bytes(data)
  result={'version':1,'ssh_enabled':not no_ssh,'original_size':len(a),'original_sha256':hashlib.sha256(a).hexdigest(),'patched_sha256':hashlib.sha256(b).hexdigest(),'key_offset':offset,'key_placeholder':base64.b64encode(key).decode(),'patch_sha256':hashlib.sha256(data).hexdigest(),'spans':spans}
  (output/'manifest.json').write_text(json.dumps(result,separators=(',',':'))+'\n')
  # Reconstruct independently through the emitted recipe before handing it on.
  reconstructed=bytearray(a)
  for s in spans: reconstructed[s['offset']:s['offset']+s['length']]=data[s['patch_offset']:s['patch_offset']+s['length']]
  assert hashlib.sha256(reconstructed).hexdigest()==result['patched_sha256']
  print(f'PASS: {len(spans)} spans, {len(data)} replacement bytes; exact441279488-byte rootfs reconstruction; key offset{offset}')
