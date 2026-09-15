#!/usr/bin/env python3
"""Exact ELF/raw ARM coordinate checks and model-mode detour recipes."""
import hashlib,struct,sys
from pathlib import Path
raw=Path(sys.argv[1]).read_bytes()
if hashlib.sha256(raw).hexdigest()!='bc054a3f3ba02c2d33ac9a515a4a8638964da779223502286d6a64b517bf1426':raise SystemExit('wrong MPC ELF')
if raw[:7]!=b'\x7fELF\x01\x01\x01' or struct.unpack_from('<HH',raw,16)!=(3,40):raise SystemExit('wrong ABI')
phoff=struct.unpack_from('<I',raw,28)[0];ents,count=struct.unpack_from('<HH',raw,42)
loads=[struct.unpack_from('<8I',raw,phoff+i*ents) for i in range(count)]
def at(a,n):
 for typ,off,va,pa,size,mem,flags,align in loads:
  if typ==1 and va<=a and a+n<=va+size:return raw[off+a-va:off+a-va+n]
 raise ValueError('not file backed')
# Entry/commit/normal-exit triplets. Early no-op branches join these exits.
# Project clear's PC-relative literal loads are relocated to immediate values.
sites=[
 (0xe36604,'f0416de10040a0e1'),(0x25f0c9c,'f4426de1f860cde1'),
 (0x17b7fb8,'28319de50040a0e1'),(0x17b8100,'683095e5dc2593e5'),
 (0x255f900,'00269fe500369fe5'),(0x2564788,'940a95e5000050e3'),
 (0x25831fc,'fc416de1005052e2'),(0x258329c,'0830a0e318208de2'),(0x25832bc,'1cd08de2d040cde1'),
 (0x25823d4,'f8416de1004051e2'),(0x2582440,'0830a0e318208de2'),(0x2582460,'18d08de2d040cde1'),
 (0x25816c4,'f4416de1343090e5'),(0x2581798,'1020a0e30130a0e3'),(0x25817b8,'1cd08de2d040cde1'),
 (0x25832f4,'f0416de108608de5'),(0x258342c,'1820a0e30130a0e3'),(0x2583480,'20d08de2d040cde1'),
 (0x25836b4,'f0416de1004051e2'),(0x2583774,'2420a0e30130a0e3'),(0x258379c,'18d08de2d040cde1'),
 (0x2581890,'343090e5382090e5'),
 (0xe5afc8,'012083e2040056e1'),(0xe29938,'012083e2040056e1'),
 (0xe4af4c,'012083e2060054e1'),(0xe524b8,'012083e2040056e1'),(0xe5fc70,'fc30d5e1084095e5'),
 (0xe46700,'e41f9fe5e42f9fe5'),(0x25641c8,'34199fe534299fe5'),(0x264eb8c,'80ce9fe5f4426de1'),
 (0x264fc58,'f4209fe5f4309fe5'),(0x264fccc,'0231d2e35bf07ff5'),
 (0x1bda804,'003095e55bf07ff5'),(0x1bda9f4,'020152e35bf07ff5'),
 (0x2651cec,'08708de50c508de5'),(0x2651f3c,'14908de518508de5'),
 (0x2651d04,'485684e54c6684e5'),(0x2652160,'485684e54c3684e5'),(0x2651b7c,'1c20a0e30130a0e3'),
 (0x25178b8,'08402de50040a0e1'),(0x25175cc,'f4416de10040a0e1'),
 (0x25178d4,'003094e55bf07ff5'),(0x2517900,'253400e3f0426de1')]
mode=sys.argv[3] if len(sys.argv)>3 else ''
ui_witness=mode=='--ui-witness'
command=mode=='--command'
mirror=mode in ('--mirror','--ui-witness','--command')
if mirror:
 selected=list(range(22))+[22,27,28,29,34,35,36,37,38]
 selected += [43,44,45]
 if command:selected += [39,40,41,46]
 if ui_witness:selected=list(range(22))+[27,28,29,34,35,36,37,38,46]
 sites += [(0x236f628,'c0309de50a8a85ed'),(0x2370018,'a03599e5a03584e5'),(0x1efc91c,'34119fe534219fe5'),(0x28c66b8,'f0416de10050a0e1')]
 if not ui_witness:
  sites += [(0x2514ab4,'f4426de1f8a1cde1'),(0x251426c,'f4426de10250a0e1'),(0x250fb0c,'d8159fe5d8259fe5'),(0x2652224,'f4426de1f860cde1'),
   (0x236f3d8,'8f0742f4c47384e5'),(0x236fe14,'bc7384e5c42384e5'),(0x2652440,'463e84e20c3083e2'),(0x26527dc,'143684e5183684e5'),
   (0x257b858,'1730c4e5243084e5'),(0x254c9c0,'8f0a41f4b03084e5'),(0xe5add4,'012083e2040056e1'),(0x2562ab0,'f4426de1f860cde1'),
   (0x2374f24,'383084e53c3084e5'),(0x2375594,'682195e54f60d5e5'),(0x2374f3c,'503084e20300a0e1'),(0x23755ac,'4f60c4e50300a0e1'),
   (0x2374f88,'7830c4e5029098e7'),(0x23755ec,'7860c4e5801084e5'),(0x2375288,'64a1c4e55c308de5'),(0x23758e4,'683195e5045084e2'),(0xe4f9b0,'012083e2040056e1')]
  selected += [23,24,25,26]+list(range(47,68))
 if command:
  sites += [(0x236f444,'287088e52c3088e5'),(0x236fe98,'385085e2103005e5')]
  sites += [(0x135a48c,'f4426de1f860cde1'),(0x135a760,'d05084e5d4b084e5'),(0x1356840,'b0339fe5f8416de1'),(0x1c37144,'f0426de10150a0e1'),(0x1c371fc,'103184e51431c4e5'),(0x1c37c4c,'f4129fe5f4229fe5')]
  sites += [(0x1bfcea4,'1000c0f2003491e5'),(0x1bfcf0c,'0400a0e10cd08de2')]
  sites += [(0x238f8d4,'0020a0e368129fe5'),(0x238f964,'303084e5342084e5'),(0x1462b08,'012083e2040056e1')]
  sites += [(0x1c3b630,'fc416de10030a0e3'),(0x1c3b740,'2c1bc4ed9030c4e5')]
  sites += [(0x28cb31c,'019089e35bf07ff5'),(0x1bb86a0,'08402de50040a0e1'),(0x1bb8750,'28d08de200409de5'),(0x1bb93bc,'04309de50020a0e3'),(0x1bb8588,'08402de50040a0e1'),(0x1bb8638,'28d08de200409de5'),(0x1bb9534,'04309de50020a0e3'),(0x1bb8470,'08402de50040a0e1'),(0x1bb8520,'28d08de200409de5'),(0x1bb96ac,'04309de50020a0e3'),(0x1bba6ac,'f0416de10040a0e1'),(0x1bba828,'f0416de10040a0e1')]
  sites += [(0x1c2e76c,'f4426de10020a0e3'),(0x1c2f644,'283584e550308de5'),(0x1c2ce9c,'012083e2040056e1'),(0x1c2bf7c,'14e08de5f860cde1')]
  sites += [(0x13d643c,'400094e5000050e3'),(0x13c028c,'0c309de50020a0e3'),(0x13bfd1c,'08402de50040a0e1'),(0x13bfd58,'00409de504d08de2'),(0x13f5770,'f0416de108608de5'),(0x13f57e8,'f0416de108608de5'),(0x13598ac,'0040a0e1a80080e2'),(0x1359898,'08d08de2028bbdec')]
  sites += [(0x23639d8,'10d08de2088bbdec'),(0x2363778,'10d08de2068bbdec'),(0x2363550,'08d08de200409de5'),(0x2363320,'08402de504e08de5'),(0x23633dc,'08402de504e08de5'),(0x264aa70,'486694e5000056e3')]
  sites += [(0x1bea8fc,'fc416de10150a0e1'),(0x1bea98c,'303084e53430c4e5'),(0x1beadc4,'f0416de10040a0e1'),(0x1beaf08,'f0416de10040a0e1'),(0x238f5e8,'b43094e50020a0e3'),(0x238f58c,'9c0097e5000050e3'),(0x238f610,'00409de504d08de2'),(0x23901d8,'f8416de1a85090e5'),(0x239001c,'f8416de1a85090e5'),(0x238fb74,'f4426de1f860cde1'),(0x238f2cc,'f0416de10050a0e1'),(0x238f36c,'f8416de10110a0e3')]
  sites += [(0x238f5d8,'08402de50040a0e1'),(0x238f010,'000051e3a00080e2'),(0x103baa8,'040994e5045984e5'),(0x2c6e7e8,'f4426de10050a0e1')]
  sites += [(0x13cf830,'fc406de10c1090e5'),(0x13cf89c,'0cd08de2d040cde1'),(0x2513fc0,'f4426de10240a0e1'),(0x1bd74dc,'f4426de1f860cde1'),(0x1bd76f0,'003090e5083093e5'),(0x1bd77b4,'0040a0e11c319de5'),(0x1bbce3c,'08402de50040a0e1'),(0x1bbce94,'003094e55bf07ff5'),(0x1bbceb4,'28d08de200409de5'),(0x1bbe6a0,'f0426de10050a0e1')]
  selected += list(range(68,139))
  sites += [(0x2437188,'f4426de1f860cde1'),(0x2435bcc,'f4426de1f8a1cde1'),(0x243820c,'0400a0e127dd8de2'),(0x2436a3c,'0400a0e1dcd08de2'),(0x2433af4,'7c3a9fe5f4426de1'),(0x24345d0,'08402de50040a0e1'),(0x24aa5ac,'f4426de1f081cde1'),(0x24aa84c,'34d08de2d040cde1'),(0x24aa91c,'00c19fe5f0416de1'),(0x24aaa0c,'10d08de2d040cde1'),(0x2434724,'9cc59fe5f4426de1')]
  sites += [(0x242e8fc,'6c259fe56c359fe5')]
  selected += list(range(139,151))
  sites += [(0x23669d0,'9c3184e5a02184e5'),(0x2366b44,'a03195e5a03184e5'),(0x265415c,'08402de5082080e2'),(0x265419c,'00409de504d08de2'),(0x13cfa14,'08402de50040a0e1'),(0x13cfa74,'10d08de200409de5'),(0xe5af48,'d8219fe5d8319fe5')]
  selected += list(range(151,158))
  sites += [(0x15951e4,'9c219fe59c319fe5')]
  selected += [158]
  sites += [(0x1c3752c,'14c1d0e500005ce3'),(0x1c37708,'2a0bc4ed2c1bc4ed'),(0x1c0c540,'0b00a0e1083090e4'),(0x1c0c560,'0030a0e30400a0e1'),(0x1c0c89c,'4fdf8de2048bbdec'),(0x1c0cac0,'d040cde108609de5')]
  selected += list(range(159,165))
  sites += [(0x2350278,'dc219fe5dc319fe5'),(0x2350340,'18d08de2d040cde1'),(0x23569e0,'08219fe508319fe5'),(0x23565f4,'f8416de10040a0e1'),(0x235a920,'f4426de108c29fe5'),(0x2358740,'58159fe558259fe5')]
  selected += list(range(165,171))
  sites += [(0x13ed940,'f4416de1004051e2'),(0x13d4408,'f0426de1f081cde1'),(0x2356afc,'40219fe540319fe5')]
  selected += list(range(171,174))
  sites += [(0x235efe0,'f4426de1f860cde1'),(0x2362dfc,'f4426de1143090e5'),(0x235c5e4,'340093e5003090e5'),(0x1a24810,'f0426de10040a0e1'),(0x1a263c0,'b8319fe59010d0e5'),(0x8f0094,'f4426de120e08de5'),(0x8b3e2c,'e4209fe5e4309fe5')]
  selected += list(range(174,181))
  # 181 is the synthetic existing UI drain diagnostic tag, never a patch.
  sites += [None,(0x235d660,'e4c19fe5f0426de1')]
  selected += [182]
  # AsyncQLinks begin/end and absolute-value queue receipts; allocator
  # publication reuses the existing after-pair28cb31c hook.
  sites += [(0x14dda28,'04309de50020a0e3'),(0x14de37c,'04309de50020a0e3'),(0x14dfc04,'d420c0e108402de5'),(0x14dfc44,'00409de504d08de2'),(0x14e3a50,'043090e5f0426de1'),(0x14e3b1c,'10229fe504329fe5')]
  selected += list(range(183,189))
  sites += [(0x14dda14,'00308de5e0309fe5'),(0x14de368,'00308de5e0309fe5')]
  selected += [189,190]
  # Capture the completed vector swap after its final stores, before the
  # untouched DMB/exclusive loop. Native retry branches enter the LDREX word;
  # patching DMB+LDREX would replace that retry destination with a literal.
  sites += [(0x14e071c,'300094e5343094e5'),(0x14e1bbc,'fc416de101c042e2'),(0x128f750,'8f0741f40cc084e5'),(0x12c5aec,'8f0741f40ce080e5'),(0x11b6d28,'5c219fe55c319fe5'),(0x11b86d8,'40229fe540329fe5'),(0x11b77f0,'500090e504a30cea'),(0x14e040c,'283090e500229fe5'),(0x14e045c,'c0219fe5b4319fe5'),(0x14e3c30,'9f3f96e1013083e2')]
  selected += list(range(191,201))
  sites += [(0x14e68bc,'0830a0e308308de5')]
  selected += [201]
  # Mode listener queue preparation/results and post-pending callback exits.
  sites += [(0x11c1140,'18708de51c408de5'),(0x11c1160,'18908de214309de5'),(0x11c11c4,'18708de51c408de5'),(0x11c11e4,'14309de50910a0e1'),(0x11c2104,'041090e55c219fe5'),(0x11c2218,'54209fe54c309fe5'),(0x11bcc70,'f0416de1044090e5'),(0x11bcd44,'d040cde108609de5'),(0x11bbd84,'70159fe570259fe5')]
  selected += list(range(202,211))
  # Final stores preserve source operands for post-pair capture. Do not place
  # the channel commit hook on STR/LDRSH, which replaces the delivered r3.
  sites += [(0x26522e4,'0a31e0e3743384e5'),(0x2652328,'003093e5a03384e5'),(0x265236c,'f23f84e28f0743f4'),(0x26523b0,'003093e5f83384e5'),(0xeeee58,'101080e5012083e2'),(0xe4fb9c,'287085e5012083e2'),(0x1462cec,'084095e5283085e5')]
  selected += list(range(211,218))
  sites += [(0xeeedec,'103090e5bc219fe5'),(0xe4fb24,'d4219fe5d4319fe5'),(0x1462c7c,'d8219fe5d8319fe5'),(0x1e93c8c,'00409de504d08de2'),(0x1e93d6c,'00409de504d08de2'),(0x1e93cfc,'00409de504d08de2')]
  selected += list(range(218,224))
  sites += [(0x1e8c020,'f4426de120e08de5'),(0x1dc97d8,'f8416de10040a0e1'),(0x1dc9fe4,'08402de50040a0e1'),(0x1e8d978,'003051e2d81e9fe5'),(0x1e8e210,'1cd08de2d040cde1'),(0x1e93750,'fc30d5e1084095e5'),(0x1e8cc2c,'8f0741f46a0bcded'),(0x1e8cca0,'203a84e58f074ef4'),(0x1e8cfa0,'0400a0e175df8de2')]
  selected += list(range(224,233))
  sites += [(0x245e8bc,'0800a0e38f0743f4'),(0x1351008,'107080e5012083e2'),(0x245e63c,'043083e2181082e5'),(0x245e6e8,'00308de5dc309fe5'),(0x245e6fc,'04309de50020a0e3'),(0x245e634,'003090e5060090e9'),(0x245e650,'912f83e1000052e3'),(0x245ee98,'0800a0e38f0743f4'),(0x1462ed0,'107080e5012083e2'),(0x245ec18,'043083e2181082e5'),(0x245ecc4,'00308de5dc309fe5'),(0x245ecd8,'04309de50020a0e3'),(0x245ec10,'003090e5060090e9'),(0x245ec2c,'912f83e1000052e3')]
  selected += list(range(233,247))
  sites += [(0x2576e98,'f0309fe5f8416de1'),(0x2577284,'4c319fe5f0416de1'),(0x25773e4,'58319fe5f0416de1'),(0x2576a50,'f4426de1404090e5'),(0x2576c84,'0c0085e20cd08de2')]
  selected += list(range(247,252))
  sites += [(0x2471e84,'e82685e58f0743f4'),(0x24712cc,'f03697e5f03684e5'),(0x2471f60,'0030a0e3943785e5'),(0x2471fa4,'bc3785e5c03785e5'),(0x2471420,'bc3784e5c03784e5'),(0xe8ece8,'012083e2040056e1'),(0x11fabd0,'287085e5012083e2'),(0xf57e0c,'287085e5012083e2')]
  selected += list(range(252,260))
  sites += [(0x24713cc,'0030a0e3943784e5'),(0x236f204,'1c2384e5203384e5'),(0x236f210,'242384e5283384e5'),(0x236f21c,'2c2384e5303384e5'),(0x236fc74,'1c2384e5203384e5'),(0x236fc80,'242384e5283384e5'),(0x236fc8c,'2c2384e5303384e5'),(0x236f274,'5c6384e5607384e5'),(0x236f280,'646384e5687384e5'),(0x236f294,'6c6384e5707384e5'),(0x236fce0,'5c6384e5607384e5'),(0x236fcf0,'646384e5687384e5'),(0x236fd0c,'6c6384e5707384e5'),(0x16051a4,'047085e2f021c5e1'),(0x16051b0,'070054e1f821c5e1'),(0x16051c0,'012083e2f002c5e1'),(0x2373a8c,'047085e2f802c5e1'),(0x2373a9c,'040057e1f003c5e1'),(0x2373aa8,'bc20c5e1f803c5e1')]
  selected += list(range(260,279))
  sites += [(0x1e758f0,'f4426de120e08de5'),(0xe7d5a4,'f0426de1982590e5'),(0xe7debc,'08402de50040a0e1'),(0x1e73100,'fc416de1f860cde1'),(0x1e733b0,'24d08de2d040cde1'),(0x1a2a6ac,'fc30d5e1084095e5'),(0x1e75f48,'50308de58f8703f4'),(0x1e5dc58,'633e84e20c3083e2'),(0x1e5e5b4,'ccc59fe5cc359fe5'),(0x1e5e6a4,'840594e5e4349fe5'),(0x1dc3764,'082790e5f8416de1'),(0x1dc3ec4,'082790e5f8416de1')]
  selected += list(range(279,291))
  sites += [(0x1e5da00,'f4426de1ccc49fe5')]
  selected += [291]
  sites += [(0x145d800,'fc239fe5fc339fe5'),(0x145da10,'4cd08de2d040cde1')]
  selected += [292,293]
  # Native blank ProjectCreator callback: invalidate before reset, publish only
  # at the ordinary return after Project initialization and page transition.
  sites += [(0xe45844,'08d04de2f4426de1'),(0xe45b2c,'fcd08de2d040cde1')]
  selected += [294,295]
 sites=[sites[i] for i in selected]
for a,h in sites:
 if at(a,8).hex()!=h:raise SystemExit(f'raw recipe changed at {a:x}')
if command:
 # The new create pairs must not replace any native retry/branch destination
 # with their inline jump literal. Scan direct B/BL in the complete RX image.
 interiors={0xe45848,0xe45b30}
 for typ,off,va,pa,size,mem,flags,align in loads:
  if typ!=1 or not flags&1:continue
  for offset in range(0,size-3,4):
   w=struct.unpack_from('<I',raw,off+offset)[0]
   if w&0x0e000000!=0x0a000000:continue
   d=w&0xffffff
   if d&0x800000:d-=1<<24
   if va+offset+8+4*d in interiors:raise SystemExit('branch enters create patch interior')
# Calls and store provenance; include successful Browser branch and source stores.
guards=[(0x25178b8,0x48),(0x25175cc,0xa8),(0x2517900,0x50),(0x2517a7c,0x58),(0x250bb6c,0x70),(0x25176a4,0x40),(0x236d848,0x10),(0x236d944,0x28),(0x264fc58,0x80),(0x1bda7e8,0x24),(0x1bda9d8,0x28),
 (0x2651ce0,0x34),(0x2651f30,0x20),(0x2652158,0x20),(0x2651b7c,0x38),(0xe46700,0x24),(0x25641c8,0x24),(0x264eb8c,0x24),(0xe36604,0x64),(0x17b7fb4,0x2c),(0x17b80f8,16),(0x25f1274,0x2c),
 (0x25831fc,0xf8),(0x25823d4,0xa4),(0x25816c4,0x108),
 (0x25832f4,0x19c),(0x25836b4,0xfc),(0x2581890,0x44),
 (0x255f900,0x10),(0x255fad4,12),(0x2564788,24),
 (0xe5af48,0x88),(0xe298bc,0x84),(0xe5afa8,0x30),(0xe29928,0x20),(0xe4af44,0x18),(0xe524b0,0x18),(0xe5fc4c,0x2c)]
if command:
 guards += [(0x14dd984,0x18c),(0x14de2d8,0x188),(0x14dfc04,0x64),(0x14e3a50,0x2dc)]
if mirror:
 guards += [(0x236efec,0x48),(0x236f5c8,0x70),(0x236fa78,0x48),(0x236ffd0,0x58),(0x23702e8,0x68),(0x23705b4,0x48),(0x23706cc,0x48),(0x2370a60,0x24),(0x2514ab4,0x38),(0x2514bdc,0x30),(0x251426c,0x38),(0x2514300,0x38),(0x1efc91c,0x48)]
if mirror and not ui_witness:
 guards += [(0x236f080,0x18),(0x236fae0,0x20),(0x236f380,0x64),(0x236fdc0,0x60),(0x2652224,0x68),(0x26523f0,0x70),(0x265279c,0x54),(0x257b7e4,0x7c),(0x254c860,0x30),(0x254c964,0x7c),(0x2562ab0,0xa0),(0xe5ad58,0x88),(0xe4f934,0x84),(0x2374e80,0x80),(0x23754ec,0x74),(0x250fb0c,0x30)]
 for address in (0x2374f24,0x2375594,0x2374f3c,0x23755ac,0x2374f88,0x23755ec,0x2375288,0x23758e4):guards.append((address-4,16))
if ui_witness or command:
 guards += [(0x10e9dec,0xa0),(0x10ea5c8,0x10),(0x10ec76c,8),(0x28c66b8,0xf4),(0x28c6c88,0x18)]
if command:guards += [(0x1beaad0,0x248),(0x1c27e90,0x34),(0x1c22a30,0x1c),(0x13ed940,0x80),(0x13d3bd8,0x230),(0x13cf830,0x80),(0x1bd74dc,0x320),(0x1bbce38,0x94),(0x2513fc0,0xa0),(0x1bbe6a0,0x40),(0x13d63a8,0x1b0),(0x13c01a8,0x168),(0x13bfd1c,0x48),(0x13f5770,0xf0),(0x1359840,0x104),(0x13fa704,0x100),(0x1c2e768,0x30),(0x1c2f620,0x40),(0x1c2ce20,0x120),(0x1c2bf70,0x38),(0x1c2caac,0x20),(0x1374b48,0x24),(0x1bba540,0xa8),(0x1bba6a8,0x160),(0x1bba824,0x140),(0x1bb9324,0x468),(0x28cb18c,0x1e0),(0x1bb8470,0x348),(0x1c3b630,0x120),(0x238f8d4,0xa0),(0x1462a8c,0x88),(0x1bfcea4,0x7c),(0x135a48c,0x98),(0x135a6f8,0x70),(0x1356840,0x64),(0x1c37144,0xd4),(0x1c37c4c,0x90),(0x236f410,0x44),(0x236fe54,0x58),(0x2549c90,8),(0x26500f8,0x1c0),(0x2650488,0x140),(0x2510754,0xc0),(0x236d65c,0x14c),(0x236d848,0x290),(0x250ba74,0x220),(0x2555f7c,0xc8),(0x2557750,0x80),(0x28c66b8,0xf4)]
if command:guards += [(0x13ed940,0x84),(0x13d4408,0x1f0),(0x2356afc,0x154),(0x2350278,0x180),(0x23569e0,0x138),(0x23565f4,0x90),(0x235a920,0x90),(0x2358740,0x90),(0x1c374f0,0x290),(0x1c0c254,0x8d0),(0x1c0cddc,0x178),(0x1c0cf44,0x130),(0x2c53c40,0x2400),(0x2c70f20,0x2300),(0x1575a98,0x200)]
if command:guards += [(0x235d660,0x1f4),(0x235efe0,0xa40),(0x2362dfc,0x36c),(0x235c5c8,0x58),(0x1a24810,0x240),(0x1a263c0,0x1d0),(0x8f0094,0x930),(0x8b3be8,0x3f8)]
if command:guards += [(0x25701f0,0x23c),(0x15951e4,0x1b0),(0x158bbb0,0x28),(0xa12bd8,0x84),(0x240274c,0x60),(0x13f64cc,0x90),(0x13f5e58,0x80),(0x2366954,0x180),(0x2366abc,0x150),(0x2652224,0xa0),(0x13d5fd0,0x300),(0x13fa308,0x160),(0x13f7714,0x100),(0x257eec4,0x44),(0x257ea18,0xa0),(0x264a378,0x220),(0x265415c,0x58),(0x13cfa14,0x78),(0x264d450,0x40)]
if command:guards += [(0x2437188,0x100),(0x2435bcc,0xa0),(0x243820c,0x28),(0x2436a3c,0x24),(0x2433af4,0x30),(0x24345d0,0x20),(0x2434724,0x70),(0x24aa5ac,0x2d4),(0x24aa91c,0x110),(0x242e8fc,0x80),(0x2430d80,0x140),(0x24bb9f0,0x84),(0x249ea40,0x2b0),(0x1dd220c,0xa0)]
if command:guards += [(0x11bcd54,0x58),(0x11c0fe0,0x568),(0x11c2104,0x178),(0x11bcc70,0xe4),(0x11bbd84,0x598),(0x1174ed0,0x198),(0x1f5ae64,0x20)]
if command:guards += [(0x14e6768,0x250),(0x14e0634,0x3f0),(0x14e1bbc,0xe48),(0x128f708,0x74),(0x12c5abc,0x54),(0x11b6d28,0x17c),(0x11b86d8,0x258),(0x11b77f0,8),(0x14e040c,0x224),(0x14e3a50,0x2f4)]
if command:guards += [(0xe4fb24,0x1f0),(0xeeedec,0x1e0),(0x1462c7c,0x1f4),(0x1dc97d8,0x7b4),(0x1dc9f8c,0x244),(0x1e88d30,0x37c),(0x1e890b4,0x380),(0x1e8943c,0x37c),(0x1e897c0,0x380),(0x1e8c020,0x1958),(0x1e8d978,0xf40),(0x1e935f0,0x4e0),(0x1e93c34,0x150),(0x264eb8c,0xec0),(0x2652224,0x10a4)]
for address,target in [(0x17b7fb4,0x25f0c9c),(0x17b80fc,0x255caa4),(0x25f1274,0x26320d4),(0x25f1284,0x255f370),(0x255fad8,0x2581890),(0xe5fc6c,0x925d58),(0x2651cf8,0x264e1d8),(0x2651f48,0x264e1d8),(0x2651ba8,0x264e1d8),(0x25178d0,0x25175cc),(0x25176d0,0x2510754)]:
 w=struct.unpack('<I',at(address,4))[0];d=w&0xffffff
 if d&0x800000:d-=1<<24
 if w>>24!=0xeb or address+8+4*d!=target:raise SystemExit('call provenance changed')
# Resolve each .ARM.exidx function boundary without an analysis import base.
shoff=struct.unpack_from('<I',raw,32)[0];shent,shnum,shstr=struct.unpack_from('<HHH',raw,46)
sections=[struct.unpack_from('<10I',raw,shoff+i*shent) for i in range(shnum)]
ex=[s for s in sections if s[1]==0x70000001]
def prel(x,place):
 d=x&0x7fffffff
 if d&0x40000000:d-=1<<31
 return place+d
starts=[]
for s in ex:
 for o in range(0,s[5],8):starts.append(prel(struct.unpack_from('<I',raw,s[4]+o)[0],s[3]+o))
if command:
 # I/O source/command paths are guarded as complete native function ranges.
 io_entries=[a for a,_ in sites if a in [sites[j][0] for j in range(len(sites)) if selected[j]>=211]]
 io_entries += [0x15762ec,0xe44ef4,0x25612e0,0x2570b1c,0x34fd450,0x3b3da14,0x2b528cc,0x2b51808,0x2b068f0,0x340e5d4,0x157507c,0x1e93ba0,0x1e8ac24,0x245f088,0x4a66cc,0x2699a90,0x4c8140,0x257eec4,0x25765d4,0x1e5d8b8,0x269741c,0x26991b0,0x250b754,0x1e60570,0x1e5b964,0x2472758,0x1605144,0x2373a08]
 for address in io_entries:
  if address==0x4a66cc:
   guards.append((address,12));continue # Native sized-delete PLT, outside exidx.
  start=max(x for x in starts if x<=address);end=min(x for x in starts if x>start)
  if (start,end-start) not in guards:guards.append((start,end-start))
for a,_ in sites:
 found=[x for x in starts if x<=a]
 if not found:raise SystemExit('no unwind range')
 start=max(found);end=min(x for x in starts if x>start)
 print(f'anchor={a:08x} unwind=[{start:08x},{end:08x}) bytes={at(a,8).hex()}')
lines=['/* Generated from exact raw ARM ELF; regenerate with model-prepare.py. */',
 'static const uint32_t anchor[PATCH_COUNT]={'+','.join(hex(a) for a,h in sites)+'};',
 'static const unsigned char expected[PATCH_COUNT][8]={'+','.join('{'+','.join(hex(b) for b in at(a,8))+'}' for a,h in sites)+'};']
# A zero relocation entry means ordinary position-independent displaced word.
reloc=[]
for a,_ in sites:
 row=[]
 for offset in (0,4):
  w=struct.unpack('<I',at(a+offset,4))[0]
  if w&0xffff0000==0xe59f0000:row.append(struct.unpack('<I',at(a+offset+8+(w&4095),4))[0])
  else:row.append(0)
 reloc.append(row)
lines+=['static const unsigned char capture_after_pair[PATCH_COUNT]={'+','.join('1' if a in (0x1e75f48,0x24713cc,0x236f204,0x236f210,0x236f21c,0x236fc74,0x236fc80,0x236fc8c,0x236f274,0x236f280,0x236f294,0x236fce0,0x236fcf0,0x236fd0c,0x16051a4,0x16051b0,0x16051c0,0x2373a8c,0x2373a9c,0x2373aa8,0x2471e84,0x24712cc,0x2471f60,0x2471fa4,0x2471420,0x11fabd0,0xf57e0c,0x245e8bc,0x1351008,0x245e63c,0x245e650,0x245ee98,0x1462ed0,0x245ec18,0x245ec2c,0x1e8cc2c,0x1e8cca0,0x26522e4,0x2652328,0x265236c,0x26523b0,0xeeee58,0xe4fb9c,0x1462cec,0x128f750,0x12c5aec,0x1c3752c,0x1c37708,0x23669d0,0x2366b44,0x1bea98c,0x1c2f644,0x28cb31c,0x264fccc,0x1bda804,0x1bda9f4,0x2651d04,0x2652160,0x25178d4,0x236f628,0x236f3d8,0x236fe14,0x26527dc,0x257b858,0x254c9c0,0x23755ac,0x2374f88,0x23755ec,0x2375288,0x236f444,0x236fe98,0x135a760,0x1c371fc,0x238f964,0x1c3b740) else '0' for a,h in sites)+'};','static const uint32_t relocated[PATCH_COUNT][2]={'+','.join('{'+','.join(hex(x) for x in row)+'}' for row in reloc)+'};']
branches=[]
for a,_ in sites:
 row=[]
 for offset in (0,4):
  w=struct.unpack('<I',at(a+offset,4))[0]
  if w>>24==0xea:
   if a!=0x11b77f0 or offset!=4:raise SystemExit('unqualified displaced branch')
   d=w&0xffffff
   if d&0x800000:d-=1<<24
   target=a+offset+8+4*d
   if target!=0x14e040c:raise SystemExit('mode bridge continuation changed')
   row.append(target)
  else:row.append(0)
 branches.append(row)
lines+=['static const uint32_t branch_target[PATCH_COUNT][2]={'+','.join('{'+','.join(hex(x) for x in row)+'}' for row in branches)+'};']
for i,(a,n) in enumerate(guards):lines+=['static const unsigned char guard_bytes_'+str(i)+'[]={'+','.join(hex(b) for b in at(a,n))+'};']
lines+=['static const struct {uint32_t address;unsigned length;const unsigned char *bytes;} guards[]={'+','.join('{'+hex(a)+','+str(n)+',guard_bytes_'+str(i)+'}' for i,(a,n) in enumerate(guards))+'};']
if mirror:lines+=['static const unsigned hook_ids[PATCH_COUNT]={'+','.join(str(i) for i in selected)+'};']
out=Path(sys.argv[2]);out.write_text('\n'.join(lines)+'\n')
ranges=[(va,va+size,flags) for typ,off,va,pa,size,mem,flags,align in loads if typ==1 and size]
out.with_name('module-ranges.h').write_text('static const uint32_t module_ranges[][3]={'+','.join('{'+','.join(hex(n) for n in row)+'}' for row in ranges)+'};\n')
