#!/usr/bin/env python3
"""Generate exact native Preferences admission after stock/current comparison."""
import hashlib,json,struct,sys
from pathlib import Path

if len(sys.argv)!=4:raise SystemExit('native-preferences-prepare.py /current/CE/MPC /stock/MPC /output/header')
current_path,stock_path,out=map(Path,sys.argv[1:])
current=current_path.read_bytes();stock=stock_path.read_bytes()
identity=json.loads((Path(__file__).resolve().parents[2]/'firmware/mpc-ce.json').read_text())
if len(current)!=identity['size'] or hashlib.sha256(current).hexdigest()!=identity['output_sha256']:raise SystemExit('wrong current CE MPC')
if hashlib.sha256(stock).hexdigest()!=identity['input_sha256']:raise SystemExit('wrong stock MPC')

def loads(raw):
 if raw[:7]!=b'\x7fELF\x01\x01\x01' or struct.unpack_from('<HH',raw,16)!=(3,40):raise SystemExit('wrong ARM ELF')
 phoff=struct.unpack_from('<I',raw,28)[0];ents,count=struct.unpack_from('<HH',raw,42)
 return [struct.unpack_from('<8I',raw,phoff+i*ents) for i in range(count)]
def at(raw,segments,address,length):
 for typ,offset,va,pa,size,mem,flags,align in segments:
  if typ==1 and va<=address and address+length<=va+size:return raw[offset+address-va:offset+address-va+length]
 raise SystemExit(f'unmapped range {address:#x}+{length:#x}')

current_loads,stock_loads=loads(current),loads(stock)
# The overlay setup ranges retain the actual integer, stack and hard-float
# provenance used by the injected call. The remaining ranges cover every host
# entry and vtable consumed by the experiment.
ranges=[
 (0x03667964,0x108,'stock NotImplementedTab call and append'),
 (0x03667a90,0x0d8,'final Splice construction and append'),
 (0x03668df8,0x15c,'overlay teardown'),
 (0x03665c60,0x0b0,'inline std string constructor'),
 (0x0369d174,0x1ac,'NotImplementedTab constructor'),
 (0x0369cdec,0x060,'NotImplementedTab complete destructor'),
 (0x036a047c,0x058,'tab child removal'),
 (0x036a04d4,0x05c,'tab child addition'),
 (0x037fa0e0,0x00c,'host operator new PLT'),
 (0x037fa32c,0x00c,'host sized delete PLT'),
 (0x00961120,0x098,'JUCE String char constructor'),
 (0x00911f58,0x068,'JUCE String destructor'),
 (0x00bd2cec,0x03c,'named TextButton constructor'),
 (0x00be19e0,0x03c,'TextButton complete destructor'),
 (0x00ba8680,0x23c,'Component setBounds'),
 (0x00bbf2fc,0x290,'Component setVisible'),
 (0x00bbba14,0x008,'Button setToggleState no-notification entry'),
 (0x036b0258,0x0bc,'NotImplementedTab active callback'),
 (0x00b24f60,0x084,'Button clicked and modifier dispatch'),
 (0x069cd0b4,0x040,'NotImplementedTab vtables'),
 (0x068905a0,0x0d4,'TextButton primary vtable'),
]
expected_digests={
 (0x00bbba14,0x008):'48ba116993b3b39a006802d6bd0cd32fa6bf2b57528828c92b39116fbdaca6d3',
 (0x036b0258,0x0bc):'6a2574bd16d2b78b97a8ea4605cd2d2f5f186e3b3f0fa8955f3523882b612f93',
}
for address,length,name in ranges:
 a=at(stock,stock_loads,address,length);b=at(current,current_loads,address,length)
 if a!=b:raise SystemExit(f'{name} changed between stock and current CE')
 if (address,length) in expected_digests and hashlib.sha256(b).hexdigest()!=expected_digests[address,length]:raise SystemExit(f'{name} digest changed')

site=0x03667a94;site_bytes=at(current,current_loads,site,8)
if site_bytes.hex()!='50709de5a72100e3':raise SystemExit('Preferences hook site changed')
pointers=[
 (0x069cd0b4+8,0x0369cdec,'tab complete destructor'),
 (0x069cd0b4+12,0x0369cf60,'tab deleting destructor'),
 (0x068905a8+46*4,0x00b24f60,'TextButton clicked'),
 (0x068905a8+47*4,0x00b24fd8,'TextButton modifier dispatch'),
 (0x069cd0b4+16,0x036b0258,'NotImplementedTab active callback'),
]
for slot,target,name in pointers:
 if struct.unpack_from('<I',at(current,current_loads,slot,4))[0]!=target:raise SystemExit(f'{name} slot changed')

# Replacing two words is valid only when no direct ARM B/BL enters word two.
for typ,offset,va,pa,size,mem,flags,align in current_loads:
 if typ!=1 or not flags&1:continue
 for delta in range(0,size-3,4):
  word=struct.unpack_from('<I',current,offset+delta)[0]
  if word&0x0e000000!=0x0a000000:continue
  displacement=word&0xffffff
  if displacement&0x800000:displacement-=1<<24
  if va+delta+8+4*displacement==site+4:raise SystemExit('direct branch enters Preferences hook interior')

def bytes_literal(data):return ','.join(f'0x{x:02x}' for x in data)
lines=['#ifndef NATIVE_PREFERENCES_PINNED_H','#define NATIVE_PREFERENCES_PINNED_H','#include <stdint.h>',
 f'#define NATIVE_PREFERENCES_SITE 0x{site:08x}u',
 '#define NATIVE_PREFERENCES_TAB_VTABLE_BYTES 32u',
 '#define NATIVE_PREFERENCES_BUTTON_VTABLE_BYTES 212u',
 f'static const unsigned char native_preferences_site_bytes[8]={{{bytes_literal(site_bytes)}}};',
 'typedef struct {uint32_t address,length;const unsigned char *bytes;} NativePreferencesRange;',
 'typedef struct {uint32_t slot,target;} NativePreferencesPointer;']
# Runtime code and PLT bytes are unchanged. The two vtable ranges are still
# compared between stock and current above, but their pointer words have been
# relocated by the loader before the injected observer runs.
runtime_ranges=ranges[:-2]
for index,(address,length,name) in enumerate(runtime_ranges):
 data=at(current,current_loads,address,length)
 lines.append(f'static const unsigned char native_preferences_range_{index}[{length}]={{{bytes_literal(data)}}};')
lines.append(f'#define NATIVE_PREFERENCES_RANGE_COUNT {len(runtime_ranges)}u')
lines.append('static const NativePreferencesRange native_preferences_ranges[NATIVE_PREFERENCES_RANGE_COUNT]={')
for index,(address,length,name) in enumerate(runtime_ranges):lines.append(f' {{0x{address:08x}u,{length}u,native_preferences_range_{index}}},')
lines.append('};')
lines.append(f'#define NATIVE_PREFERENCES_POINTER_COUNT {len(pointers)}u')
lines.append('static const NativePreferencesPointer native_preferences_pointers[NATIVE_PREFERENCES_POINTER_COUNT]={')
for slot,target,name in pointers:lines.append(f' {{0x{slot:08x}u,0x{target:08x}u}},')
lines.extend(['};','#endif',''])
out.write_text('\n'.join(lines))
print(f'PASS exact stock/current native Preferences ranges={len(ranges)}; runtime code guards={len(runtime_ranges)}; relocated pointer guards={len(pointers)}; hook={site:#x}; no direct branch enters displaced second word')
