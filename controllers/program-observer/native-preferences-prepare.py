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
 (0x00bddb58,0x390,'Label constructor'),
 (0x00bdd4ac,0x220,'Label complete destructor'),
 (0x00c0c9d4,0x1b0,'Label silent text update'),
 (0x00b9008c,0x018,'Label justification setter'),
 (0x00b38eac,0x024,'Component mouse interception setter'),
 (0x00b870ac,0x098,'Label inherited paint'),
 (0x033b4ecc,0x054,'stock Label construction caller'),
 (0x033b518c,0x0a0,'stock Label setup caller'),
 (0x033b385c,0x03c,'stock Label silent text caller'),
 (0x00be5920,0x0c8,'ListBox constructor'),
 (0x00bd6a38,0x138,'ListBox complete and deleting destructors'),
 (0x00bc7a94,0x034,'ListBox setModel'),
 (0x00bc71c0,0x368,'ListBox updateContent'),
 (0x00bc770c,0x060,'ListBox setRowHeight'),
 (0x00bfadc8,0x01c,'ListBox selectRow'),
 (0x00b67470,0x008,'ListBox getViewport'),
 (0x00b89c64,0x54c,'Viewport setScrollOnDragEnabled with threshold'),
 (0x00c12c94,0x1b4,'Slider default constructor'),
 (0x00c020d8,0x144,'Slider complete destructor'),
 (0x00c12918,0x040,'Slider style entry'),
 (0x00c13d44,0x040,'Slider range entry'),
 (0x00c12860,0x040,'Slider text-box entry'),
 (0x00b67cac,0x0f4,'Slider addListener'),
 (0x00b67da0,0x0a0,'Slider removeListener entry'),
 (0x00b68148,0x020,'Slider value getter entry'),
 (0x00c13b64,0x008,'Slider silent value setter entry'),
 (0x02918c3c,0x094,'stock Slider listener callback'),
 (0x02918cd0,0x0b0,'stock Slider construction and binding'),
 (0x02c11184,0x020,'stock silent Slider owner refresh'),
 (0x00c4b10c,0x018,'Slider value listener dispatch'),
 (0x00c4b594,0x024,'Slider drag-start listener slot'),
 (0x00c4b6f8,0x00c,'Slider drag-start indirect call'),
 (0x00c4b32c,0x024,'Slider drag-end listener slot'),
 (0x00c4b498,0x00c,'Slider drag-end indirect call'),
 (0x00c02648,0x028,'Slider mouse-up drag-end caller'),
 (0x03752b10,0x010,'stock Viewport drag-scroll enable caller'),
 (0x00c20c9c,0x058,'ListBox stock model paint dispatch'),
 (0x030f818c,0x0b0,'stock Preferences embedded ListBox construction'),
 (0x00ba8680,0x23c,'Component setBounds'),
 (0x00bbf2fc,0x290,'Component setVisible'),
 (0x00bbba14,0x008,'Button setToggleState no-notification entry'),
 (0x00b38574,0x0a0,'Component default constructor'),
 (0x00bd2388,0x450,'Component complete destructor'),
 (0x00bc5c00,0x064,'Component removeChildComponent'),
 (0x00bc5c64,0x1ac,'Component addChildComponent'),
 (0x00bc2868,0x204,'Component setAlwaysOnTop'),
 (0x00b8f258,0x040,'Component repaint'),
 (0x00b397a0,0x170,'Component setColour'),
 (0x00a4c268,0x190,'Graphics setColour'),
 (0x00a4cae0,0x2a4,'Graphics setFont float'),
 (0x00a4d0a4,0x1cc,'Graphics fillAll Colour'),
 (0x00aa85e8,0x34c,'Graphics drawFittedText integer bounds'),
 (0x00b2bbc0,0x090,'MPC fitted-text paint caller'),
 (0x008cc700,0x028,'MPC fillAll caller'),
 (0x00b7a64c,0x268,'TextButton label drawing helper'),
 (0x00b7a8bc,0x14c,'TextButton paintButton'),
 (0x036b0258,0x0bc,'NotImplementedTab active callback'),
 (0x00b24f60,0x084,'Button clicked and modifier dispatch'),
 (0x03663968,0x2a4,'TouchModeMenuOverlay constructor'),
 (0x03455430,0x3c4,'launcher Page constructor'),
 (0x03456008,0x0e4,'outer launcher Page layout and child rebuild iteration'),
 (0x034579d4,0x43c,'launcher Page rebuild'),
 (0x03457e10,0x050,'launcher child rebuild wrapper'),
 (0x019888d8,0x0f8,'launcher mode-list getter'),
 (0x03664150,0x010,'launcher overlay child addition'),
 (0x03663334,0x16c,'launcher overlay destructors'),
 (0x00e405c0,0x070,'Global MIDI Learn owner and view-controller startup'),
 (0x02ad8444,0x218,'Global MIDI Learn view-controller constructor owner/file store'),
 (0x018737dc,0x418,'Global MIDI Learn selected Target property setter'),
 (0x018b49f8,0x008,'Global MIDI Learn file-handler getter'),
 (0x018b5840,0x0fc,'Global MIDI Learn live pairing lookup'),
 (0x018bdc08,0x19c,'Global MIDI Learn target construction and lifetime'),
 (0x018bed00,0x05c,'Global MIDI Learn assignment formatter'),
 (0x02fa54b0,0x32c,'Global MIDI Learn stock table caller'),
 (0x02fa6e84,0xb80,'Global MIDI Learn target label decoder'),
 (0x02ad53f4,0x178,'Global MIDI Learn owner-property listener and mirror queue'),
 (0x02ad5e0c,0x020,'Global MIDI Learn queued Learn intent entry prefix'),
 (0x02ad5e0c,0x17c,'Global MIDI Learn queued Learn intent entry'),
 (0x02ad7d5c,0x020,'Global MIDI Learn view-controller mirror closure'),
 (0x02ad7da4,0x020,'Global MIDI Learn owner-property closure'),
 (0x012929b8,0x23c,'Global MIDI Learn bool Property setter'),
 (0x018a7da8,0x0cc,'Global MIDI Learn selected-file name'),
 (0x018a85bc,0x1f8,'Global MIDI Learn file construction and vtable store'),
 (0x018ab5d0,0x324,'Global MIDI Learn file collection enumeration'),
 (0x02ad5f88,0x008,'Global MIDI Learn duplicate file action'),
 (0x02ad60f0,0x0c4,'Global MIDI Learn clear target action'),
 (0x02ad6bac,0x3ac,'Global MIDI Learn synchronous import/export chooser action'),
 (0x02ad71e0,0x008,'Global MIDI Learn native file selection action'),
 (0x02ad7adc,0x280,'Global MIDI Learn create file action'),
 (0x01801cd4,0x444,'Global MIDI Learn synchronous chooser vslot'),
 (0x00986680,0x078,'Global MIDI Learn File String constructor'),
 (0x02c1c328,0x00c,'Global MIDI Learn File String constructor veneer'),
 (0x015a7cf0,0x1b8,'Global MIDI Learn writable user mapping directory'),
 (0x015a8034,0x144,'Global MIDI Learn chooser initial directory'),
 (0x018b2068,0x500,'Global MIDI Learn native export writer'),
 (0x00956d44,0x0dc,'Global MIDI Learn source filename'),
 (0x0093352c,0x00c,'Global MIDI Learn HostString UTF-8 borrow'),
 (0x009866f8,0x538,'Global MIDI Learn child File construction'),
 (0x0094da2c,0x03c,'Global MIDI Learn destination collision predicate'),
 (0x0094da68,0x058,'Global MIDI Learn source regular-file predicate'),
 (0x0096b708,0x0a8,'Global MIDI Learn byte-copy'),
 (0x018bed5c,0x0fc,'Global MIDI Learn mapping open from copied File'),
 (0x018ad794,0x50c,'Global MIDI Learn mapping registration'),
 (0x018b3420,0x2a0,'Global MIDI Learn file-handler mapping selection'),
 (0x00c96a68,0x0f4,'Global MIDI Learn temporary shared-reference release'),
 (0x007d6d8c,0x044,'Global MIDI Learn note-message factory'),
 (0x007d6d4c,0x040,'Global MIDI Learn controller-message factory'),
 (0x007d6c90,0x038,'Global MIDI Learn pitch-message factory'),
 (0x007d594c,0x040,'Global MIDI Learn message destructor'),
 (0x018ba570,0x1e0,'Global MIDI Learn Mapping constructor'),
 (0x018b7558,0x338,'Global MIDI Learn Mapping owner commit'),
 (0x018b6220,0x140,'Global MIDI Learn Mapping control setter'),
 (0x018b6360,0x140,'Global MIDI Learn Mapping reverse setter'),
 (0x018b798c,0x020,'Global MIDI Learn Mapping cleanup caller'),
 (0x018a1764,0x024,'Global MIDI Learn learned-Mapping cleanup caller'),
 (0x01efc91c,0x148,'Global MIDI Learn Mapping signal destructor'),
 (0x004a68f4,0x00c,'Global MIDI Learn Mapping allocation free veneer'),
 (0x069cd0b4,0x040,'NotImplementedTab vtables'),
 (0x068905a0,0x0d4,'TextButton primary vtable'),
 (0x068ca8c4,0x044,'ListBoxModel primary vtable'),
 (0x0688eb70,0x0b4,'Component primary vtable'),
 (0x069cabac,0x064,'launcher overlay primary vtable'),
 (0x0689c878,0x020,'Global MIDI Learn file primary vtable'),
 (0x0689c134,0x020,'Global MIDI Learn chooser primary vtable'),
]
expected_digests={
 (0x00c02648,0x028):'76b8738a5de8b5707e0afd7af791f9743ba5e20b1c24da1383e02a2d445c8978',
 (0x00c4b498,0x00c):'b718f2c618919fe1ceb7c253994be1a022c8cba499e5a4577d8534f8dc013310',
 (0x00c4b32c,0x024):'dac00b00c1a25a173ce8db09bd4f13faeebd55c5398b6c97a694a019528f4f78',
 (0x00c4b6f8,0x00c):'ba5de5a859cc6ab971c2b3e69e5abe1850dd8dfd73204a5bc3807b8c508c9b16',
 (0x00c4b594,0x024):'b920c215040791cdbeb615c25a800f388bff903a91aff30221b9c1164bf817be',
 (0x00c4b10c,0x018):'85a5b84e6a965c7a12bae9fce93953203ace9864b8e7cb55473e52759114fcdd',
 (0x02c11184,0x020):'c591dd0d5a9dbde4b04e9ec6e45ae950bebad3d6e486aa9cfd55b71d66209e54',
 (0x02918cd0,0x0b0):'7b0cd41ea9dc033e6e353dad8a0fca9da64f29e7b9b4f39d37604aa12b4c448e',
 (0x02918c3c,0x094):'b6afd2cba2a73ebf09728190744bc3a415cda7a62c0676c98a465812d64d9689',
 (0x00c13b64,0x008):'e1bb510060e56e8164015dda3544dcb5bc9102f29ebcacde09517de95df2d938',
 (0x00b68148,0x020):'2a72a4734a851bd12b18ee05664374694b47296018cacc8c4bf56626749c9040',
 (0x00b67da0,0x0a0):'d3fd5c9e966ec971e3408a1835744e95d6103d4ce4627094d36b849a92b93842',
 (0x00b67cac,0x0f4):'14f908a7b8cc453fa7fa85d447abb7671c8122736e46a072586d4474997e1c1a',
 (0x00c12860,0x040):'810bf75a079706e0809913b4edec93837caa0f1ec0895d3e7fe376d09b28eef4',
 (0x00c13d44,0x040):'d1269f1e0942e22beda7e05cd17913e187699b2dd53c4c3e378b83b513adc849',
 (0x00c12918,0x040):'306e3d1b687b788fdb69b2c4fb9cec56ddd89304f8b31972482995b20c6357d3',
 (0x00c020d8,0x144):'8f3260c916c33865edb49523f9566416e8f63bc45bed46fbb5d5b0e45e358fad',
 (0x00c12c94,0x1b4):'dfeff5229879b1687c441739fa13d17404788299a89c14ca4c61cd82112c060b',
 (0x033b385c,0x03c):'d73137114e8bf845036a3c49c1bf7ecd77422ac153fa74f990fece28b99608ce',
 (0x033b518c,0x0a0):'676128668146c00f610b705475e4c33b4c7f0e48bb511755522c9f64a5f68e80',
 (0x033b4ecc,0x054):'de2dd0cfb12e2ab9e4cdd82d7f694d7295d3e759ada7dec7a25022471dfffcdd',
 (0x00b870ac,0x098):'368ed4a00eef00b54c39c54c8000327fd77330ab65461fcb02fea66fd1f5bca6',
 (0x00b38eac,0x024):'93c86630b66e68b7e229406a3ac636fcf40a0c5f77883d1cc35181b55124f5e2',
 (0x00b9008c,0x018):'36047a0c25948a419857ded8542e59c19584bfe5b3c45e4e36e24aaa6463f1c4',
 (0x00c0c9d4,0x1b0):'be477402b2136677286e68df4f629aa300b9948cfc123f9ce5f2977082614d59',
 (0x00bdd4ac,0x220):'eba5920aa18f9795407e13938279cde68ce3e81537fa00ff60d2e4c16f50a2f4',
 (0x00bddb58,0x390):'7d93d500df8f94332c9e3171e8f77f86544c1c9dd591b6bffca8ebcf28a50cd9',
 (0x00bbba14,0x008):'48ba116993b3b39a006802d6bd0cd32fa6bf2b57528828c92b39116fbdaca6d3',
 (0x00be5920,0x0c8):'9a9f29a7489f4dc5e718c33273bba8c70a1e2a9fda978b617a72ad3c1638f0b3',
 (0x00bd6a38,0x138):'fd49143e92bb767b404d8e1bde5ed3ac6cc33f1476bb9d5b4797b9c7d7f2c86b',
 (0x00bc7a94,0x034):'842147c370836cbefde377403a5b0a60e106b3991305f9a34870fce36fd64164',
 (0x00bc71c0,0x368):'d4bda20a6a8f7df697a95ed0d0caee3e68f411d49b6b2296d561fb0b8e269a80',
 (0x00bc770c,0x060):'d84aa673996cd76b2dc1d08cbc3fabbe2d6c90c5cadb671c97f9c2430e22e25f',
 (0x00bfadc8,0x01c):'2cb10bea864e64111554d465c3a6799841fc69dd713df729eb27128cf7aca8a4',
 (0x00b67470,0x008):'2769a7361987f02cd9992286851b50892fb5d98c7b44c3a4cc7587f46a802807',
 (0x00b89c64,0x54c):'40eacf93eb933cfead3eaf0228a11be065da8fcf6332bc9d3c8b02f525019f9b',
 (0x03752b10,0x010):'527c395612cf6141ccd608253f0eb2c488d92b2e60ca571dda2735527115aeef',
 (0x00c20c9c,0x058):'50a0e055969183b03632cb62ec143f89e429421f12df510c1dbe91697a444ec2',
 (0x030f818c,0x0b0):'0307e5f054944b4f34fd92fe9e191fd10310e411340156eb2f1627e94716c755',
 (0x068ca8c4,0x044):'d0954290da8fb400ca790be74ac2ed4bac2f570e603bd0714d49cfd58fdbda74',
 (0x00bc2868,0x204):'f3d35849d955f98b73a0f3405cd5f832803f3e56c8a3ab702b4202124b3d23b4',
 (0x036b0258,0x0bc):'6a2574bd16d2b78b97a8ea4605cd2d2f5f186e3b3f0fa8955f3523882b612f93',
 (0x00e405c0,0x070):'0dfb7cacb874b07f9ad9399eea29962b51e926f48e41836f1cc5da352b97d4d4',
 (0x02ad8444,0x218):'0a1aa286da6a32e4ef6d2941bad8a7a0096041205833e35f3c0f655abb945961',
 (0x018b49f8,0x008):'0351c869d4f0a02e9e720bc24d05eb8e138dd1aae901f2808f4fa81b1cab75c6',
 (0x018b5840,0x0fc):'98523dae1b8cc1905320a629f064bbc0aa43f01ad420e86ca85fc2838c18901d',
 (0x018bdc08,0x19c):'215e989f5e4ba07cbafe0c7d8c532f400b97149765f8749d6d2092db8830df44',
 (0x018bed00,0x05c):'90ec18924e5a96d7027317a6f213f6050a44e1a34227a817d85ef072db35accd',
 (0x02fa54b0,0x32c):'36108514956d416bc4a6811ad8f5d4eeb5b0543e661beadc35f6374db89da18c',
 (0x02fa6e84,0xb80):'7f077e454d6deb4d2d4fcdea2dfc4b72e8b87f17a346de34e4714a59ce851c51',
 (0x02ad5e0c,0x020):'61c46a22baa1029a47a25f0888513d58f582819236db5194187cd0210a65fae6',
 (0x02ad7d5c,0x020):'68e48a6eeb0234f0954333b9eb7cf92461f0b5b532ac756aa55b87bfc0fc940b',
 (0x02ad7da4,0x020):'d631ba6e77d3ae7ebd7ef9bec5d41e06d0bd9348164c260cc784c0651a1e6ef5',
 (0x018a7da8,0x0cc):'6c0d3fb21a0263e3cdce95d7b83e3f1f8f28f7ec3da4265b5ffe98cb43b2f38b',
 (0x018a85bc,0x1f8):'b8cdf10a9f195fc5b75e96122293cbdaa5cddb24cf7265583dc715c1d7c53ade',
 (0x018ab5d0,0x324):'47938cabb7ab35df48fda51c1f6db632acd8d0593d8862fb4be281905994219b',
 (0x02ad5f88,0x008):'6da4dc8fe577d7c8edd7ca9af3b180d4440cd81a6127ee381899c2fb1b53226e',
 (0x02ad60f0,0x0c4):'bb0bcd2807980d13d7e0ff55dff556c8bc2224972375223110252261107a23f5',
 (0x02ad6bac,0x3ac):'e6979665ac6aa6610d7e80999201c274838cce084e89528424628392aac5d4fa',
 (0x02ad71e0,0x008):'43e098a14f718667e1d11ef2a0601f6fd0dd7ffc3a8722e41d9224e17b3e8c17',
 (0x02ad7adc,0x280):'9e6596e724241a3b3d4bb6d92a218a0e6b767a87ef0037c606cd41e54d672124',
 (0x01801cd4,0x444):'4d38ac384d800f4aaa89028f7cc692680851e9e9651d63e19279ee0d0b0b3a09',
 (0x00986680,0x078):'0fed91fd2c27b85d75610ca31509651fe4ecc6e4c4ddef7176f7a97065b5441f',
 (0x02c1c328,0x00c):'30d14b39fb43fd053aea86021fdd179feb32bd4a05dbd8d7a4646d0a274a2348',
 (0x015a7cf0,0x1b8):'dacafc1ba0e8422720f25de2361fa5fe031b0677d7fa4c95cb8692e1b00191ac',
 (0x015a8034,0x144):'984bf0b93f694c174ae61dcd152363933446990cdb9e5e716b541cc280832e4d',
 (0x018b2068,0x500):'12c97fd2ed47b6f14a3702c1d57ee633912f3835052c50f0be1b8dd24fe82e6b',
 (0x00956d44,0x0dc):'2415409eec56acdd88e769e37ab7bedab19ad93be20cca81f42441904ec77f73',
 (0x0093352c,0x00c):'233df71df779d3345a7ab19d42b1cbe9ea7a93fc7a3d39ca6eb67d424cd03382',
 (0x009866f8,0x538):'c33c7a300a87c26752a980b571b489b65cee4920ba774ac8392f91bf8320d209',
 (0x0094da2c,0x03c):'27011db9c3075183e031c49598d875c62bf0071332e2aa4f2a9692206fb96aa8',
 (0x0094da68,0x058):'893971a139b70b25c532e850c227015a8de0ce10eaae0bad1ca3c84c2008c129',
 (0x0096b708,0x0a8):'adb0fae906285bdcca7462d82d3ef5b8241806c292d7c4064666f2e518fbebda',
 (0x018bed5c,0x0fc):'ebc95eda9265402bbfb969f68ba160a309edd31dc1a0c6a415beea5ac15bded4',
 (0x018ad794,0x50c):'81309fc4d4c476d068edd77fd81ea7a7457afb38b1418220ead0989c36dfb77f',
 (0x018b3420,0x2a0):'0366954c6a4ae9d03dad6fc355dd257d1d233962189cd29e0a225f7f6f37458b',
 (0x00c96a68,0x0f4):'e431876c7934c665a22a513e65c6dfedfcf24d076138eb7028972abbcc8d049c',
 (0x007d6d8c,0x044):'5aa40636a8dc8d02f72dcb838558cbcea2d4001b2393c31937331799e7c4ca01',
 (0x007d6d4c,0x040):'e47fbd46b5a47f45841fb5c1aff407bfd5aacb755e782bae3dc3771c5e8f82af',
 (0x007d6c90,0x038):'c60ace7c399b957d7b6008c329366644fcba6c7e1f13c9d247665ecd23f8ea24',
 (0x007d594c,0x040):'f31314d0d9c5d7809cebee00fcbb3f04f9c9453e6817ae35ad56713338458a69',
 (0x018ba570,0x1e0):'2a1cf02d085a563a6b836a3855e9d230072f1d02b9d1ac2a25be971926fc00a9',
 (0x018b7558,0x338):'e1111e9d0922f12160c614f86a73bdc3199b8e80dd3f9b24f34e49dba29d39f8',
 (0x018b6220,0x140):'22591af2135e0af6f895765d5008631ab59281f5d4c4fb814077dcd866e0c4f8',
 (0x018b6360,0x140):'80bf14ff0d8fc5786a4411dff3a28b4377970bb3a709067ec56861d45c9d4a06',
 (0x018b798c,0x020):'8e0d44b5933a54c7927e04522883e098d8da50655b2064fc9bf6897e3096481d',
 (0x018a1764,0x024):'152f09cfe8bd5dde000696c204eb208033ca0687507a918b3f3ce851cf27fc60',
 (0x01efc91c,0x148):'b003e4757af9a67121e49a458c7e7ba04ec023ac80eab03c4f03b4c8c86be3b4',
 (0x004a68f4,0x00c):'60d3352509ad7a822141e5f7e3eda9cbc611f1a46d73828bc379ec1547f765df',
 (0x0689c878,0x020):'6ce29fccd22c1d39ec3c6416af37d23a2801ca6559051277d8ec3df41469078d',
 (0x0689c134,0x020):'ae6299ba02890538ac25cf8c43be9dc58ad3ef62f25973be7b5fef21174941f7',
}
for address,length,name in ranges:
 a=at(stock,stock_loads,address,length);b=at(current,current_loads,address,length)
 if a!=b:raise SystemExit(f'{name} changed between stock and current CE')
 if (address,length) in expected_digests and hashlib.sha256(b).hexdigest()!=expected_digests[address,length]:raise SystemExit(f'{name} digest changed')

# Derive the paint and launcher rebuild contracts from the pinned instructions
# instead of guessed symbols or another host class's layout.
def arm_branch_target(address,word):
 displacement=word&0xffffff
 if displacement&0x800000:displacement-=1<<24
 return address+8+4*displacement

# Four independently checked MPC calls pass Component in r0, a TextButton colour ID in
# r1, and ARGB in r2 through the same veneer to raw 0x00b397a0. The function
# preserves r2, derives the property identifier from r1, and dispatches the
# virtual colourChanged slot only after the property changes, matching the
# pinned JUCE Component::setColour contract.
for call,id_word,id_high,colour_load in [
 (0x02d731c4,0xe3001103,0xe3401100,0xe59d2004),
 (0x02d731d8,0xe3001102,0xe3401100,0xe59d2000),
 (0x02d731ec,0xe3a01c01,0xe3401100,0xe59d2024),
 (0x02d73200,0xe3001101,0xe3401100,0xe59d2020),
]:
 words=struct.unpack('<5I',at(current,current_loads,call-16,20))
 if words[:3]!=(id_word,id_high,colour_load) or words[3]!=0xe1a00004 or arm_branch_target(call,words[4])!=0x03011304:
  raise SystemExit('Component setColour caller ABI changed')
veneer=struct.unpack('<3I',at(current,current_loads,0x03011304,12));literal=veneer[2]
if literal&0x80000000:literal-=1<<32
set_colour_words=struct.unpack('<58I',at(current,current_loads,0x00b397a0,232))
if (veneer[:2]!=(0xe59fc000,0xe08ff00c) or 0x03011310+literal!=0x00b397a0 or
 set_colour_words[4]!=0xe58d2004 or set_colour_words[10]!=0xe1a04000 or
 set_colour_words[11]!=0xe2805058 or set_colour_words[55]!=0xe59330a8):
 raise SystemExit('Component setColour implementation ABI changed')

# The launcher walks the captured outer Page's existing Component hierarchy to
# the second viewport page and its empty row-two app slot. Component::setBounds
# stores x/y/width/height at +0x10, while addChildComponent proves the parent
# pointer at child +0x0c and the parent's child array/capacity/count at
# +0x28/+0x2c/+0x30. Repair-9's live ordered child array exposed a later stock
# Button in front of the injected child. The exact setAlwaysOnTop body changes
# byte +0x69 bit 0, calls toFront(false), and addChildComponent keeps later
# normal children before the trailing always-on-top group.
set_bounds_words=struct.unpack('<30I',at(current,current_loads,0x00ba8680,120))
add_child_words=struct.unpack('<49I',at(current,current_loads,0x00bc5c64,196))
always_on_top_words=struct.unpack('<129I',at(current,current_loads,0x00bc2868,516))
if (set_bounds_words[1]!=0xe1a04000 or set_bounds_words[2]!=0xe5900018 or
 set_bounds_words[5]!=0xe5940010 or set_bounds_words[27]!=0xe2843010 or
 set_bounds_words[28]!=0xf4038a8f or
 add_child_words[2]!=0xe591000c or add_child_words[15]!=0xe586500c or
 add_child_words[18]!=0xe5950028 or add_child_words[19]!=0xe5957030 or
 add_child_words[17]!=0xe5d63069 or add_child_words[20]!=0xe3130001 or
 add_child_words[30]!=0xe5dcc069 or add_child_words[31]!=0xe31c0001 or
 add_child_words[37]!=0xe595202c or add_child_words[47]!=0xe5857030):
 raise SystemExit('Component bounds/parent/children layout changed')
if (always_on_top_words[1]!=0xe1a05001 or always_on_top_words[4]!=0xe5d03069 or
 always_on_top_words[11]!=0xe2033001 or always_on_top_words[39]!=0xe7c03015 or
 always_on_top_words[41]!=0xe5c43069 or
 arm_branch_target(0x00bc2938,always_on_top_words[52])!=0x00bb8ffc or
 arm_branch_target(0x00bc294c,always_on_top_words[57])!=0x00b3900c):
 raise SystemExit('Component setAlwaysOnTop ABI changed')

# The exact host theme defaults TextButton text to black in both states.
theme=struct.unpack('<8I',at(current,current_loads,0x04d29f10,32))
if theme!=(0x1000100,0xffbbbbff,0x1000101,0xff4444ff,0x1000103,0xff000000,0x1000102,0xff000000):
 raise SystemExit('TextButton default colour table changed')

# Repair-3 established that MPC's inherited TextButton LookAndFeel ignores the
# injected component colours. The replacement paint slot uses four host Graphics
# methods. An MPC paint caller proves setColour/setFont and integer-bound
# drawFittedText argument placement, another proves fillAll, and TextButton's
# own drawing helper proves that its label is a JUCE String at object +0xa8.
paint_words=struct.unpack('<32I',at(current,current_loads,0x00b2bbc0,128))
if (arm_branch_target(0x00b2bbf0,paint_words[12])!=0x00a4c268 or
 paint_words[13:15]!=(0xeeb20a0c,0xe1a00005) or arm_branch_target(0x00b2bbfc,paint_words[15])!=0x00a4cae0 or
 paint_words[20]!=0xe2841e19 or paint_words[21]!=0xe58dc008 or paint_words[25]!=0xe58d300c or
 arm_branch_target(0x00b2bc3c,paint_words[31])!=0x00aa85e8):
 raise SystemExit('Graphics fitted-text caller ABI changed')
fill_words=struct.unpack('<10I',at(current,current_loads,0x008cc700,40))
if fill_words[7]!=0xe1a00005 or arm_branch_target(0x008cc720,fill_words[8])!=0x00a4d0a4:
 raise SystemExit('Graphics fillAll caller ABI changed')
button_draw_words=struct.unpack('<112I',at(current,current_loads,0x00b7a64c,448))
if (button_draw_words[58]!=0xe5954018 or button_draw_words[59]!=0xe595801c or
 button_draw_words[95]!=0xe28510a8):
 raise SystemExit('TextButton bounds/label layout changed')

# Each launcher overlay captures an outer Page. Its layout starts with Page in
# r5 and lays out the viewport. LDRD at 0x034560c0 then replaces r4/r5 with the
# record begin/end before invoking every child rebuild; at the populated-vector
# epilogue both registers equal end. The injected code recovers the bound Page
# only by matching that iterator to its guarded +0xc8/+0xcc/+0xd0 envelope.
# The empty-vector branch reaches the epilogue before LDRD and retains Page in
# r5. In both cases the hook is after all child rebuild calls made by this pass.
outer_words=struct.unpack('<56I',at(current,current_loads,0x03456008,224))
wrapper_words=struct.unpack('<19I',at(current,current_loads,0x03457e10,76))
if (outer_words[6]!=0xe1c52cd8 or outer_words[46]!=0xe1c54cd8 or outer_words[49]!=0xe4940008 or
 arm_branch_target(0x034560d0,outer_words[50])!=0x03457e10 or wrapper_words[11]!=0xe1a05000 or
 arm_branch_target(0x03457e54,wrapper_words[17])!=0x034579d4 or
 outer_words[51:56]!=(0xe1550004,0x1afffffb,0xe28dd02c,0xe1cd40d0,0xe28dd008)):
 raise SystemExit('launcher outer Page post-layout iterator ownership changed')

# The child Page rebuild obtains its stock mode vector, then loops over it to
# create both an EngineMode tile and its companion. The injected entry never
# changes the vector: repair-2 showed that a donor renders its stock label and
# action in both places.
page_words=struct.unpack('<102I',at(current,current_loads,0x034579d4,408))
if (page_words[14]!=0xe1a04000 or
 arm_branch_target(0x03457a74,page_words[40])!=0x019888d8 or
 page_words[41:44]!=(0xe1cd21d8,0xe0433002,0xe3530000) or
 page_words[86]!=0xe7927108 or
 arm_branch_target(0x03457b58,page_words[97])!=0x03500cd0 or
 page_words[98]!=0xe3a00f46 or
 arm_branch_target(0x03457b60,page_words[99])!=0x037fa0e0):
 raise SystemExit('launcher Page mode-vector/paired-child rebuild changed')
sites=[
 (0x03667a94,'50709de5a72100e3','Preferences insertion'),
 (0x03663ac8,'0030a0e30110a0e3','launcher overlay construction'),
 (0x034560dc,'2cd08de2d040cde1','launcher outer Page after layout and child rebuilds'),
 (0x00e40628,'4800a0e3cc5784e5','Global MIDI Learn owner/view-controller capture'),
]
site_bytes=[]
for address,expected,name in sites:
 data=at(current,current_loads,address,8)
 if data.hex()!=expected:raise SystemExit(f'{name} hook site changed')
 site_bytes.append(data)
pointers=[
 (0x069cd0b4+8,0x0369cdec,'tab complete destructor'),
 (0x069cd0b4+12,0x0369cf60,'tab deleting destructor'),
 (0x068905a8+46*4,0x00b24f60,'TextButton clicked'),
 (0x068905a8+47*4,0x00b24fd8,'TextButton modifier dispatch'),
 (0x068905a8+48*4,0x00b7a8bc,'TextButton paintButton'),
 (0x069cd0b4+16,0x036b0258,'NotImplementedTab active callback'),
 (0x0688eb70+8,0x00bd2388,'Component complete destructor'),
 (0x0688eb70+12,0x00bd27d8,'Component deleting destructor'),
 (0x069cabac+8,0x03663334,'launcher overlay complete destructor'),
 (0x069cabac+12,0x03663404,'launcher overlay deleting destructor'),
 (0x068ca8c4+4,0x068ca478,'ListBoxModel typeinfo'),
 (0x068ca8c4+8*4,0x00b257b0,'ListBoxModel double-click default'),
 (0x068ca8c4+9*4,0x00b24fc0,'ListBoxModel background-click default'),
 (0x068ca8c4+11*4,0x00b24f68,'ListBoxModel delete-key default'),
 (0x068ca8c4+12*4,0x00b24f68,'ListBoxModel return-key default'),
 (0x068ca8c4+13*4,0x00b24f60,'ListBoxModel scrolled default'),
 (0x068ca8c4+14*4,0x00b275e8,'ListBoxModel no-drag default'),
 (0x068ca8c4+15*4,0x00b26774,'ListBoxModel tooltip default'),
 (0x068ca8c4+16*4,0x00b2608c,'ListBoxModel cursor default'),
 (0x0689c878+0x10,0x018a7da8,'Global MIDI Learn selected-file name vslot'),
 (0x0689c134+0x08,0x01801cd4,'Global MIDI Learn synchronous chooser vslot'),
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
  target=va+delta+8+4*displacement
  for site,expected,name in sites:
   if target==site+4:raise SystemExit(f'direct branch enters {name} hook interior')

def bytes_literal(data):return ','.join(f'0x{x:02x}' for x in data)
lines=['#ifndef NATIVE_PREFERENCES_PINNED_H','#define NATIVE_PREFERENCES_PINNED_H','#include <stdint.h>',
 '#define NATIVE_PREFERENCES_TAB_VTABLE_BYTES 32u',
 '#define NATIVE_PREFERENCES_BUTTON_VTABLE_BYTES 212u',
 '#define NATIVE_PREFERENCES_SITE_COUNT 4u',
 'typedef struct {uint32_t address,length;const unsigned char *bytes;} NativePreferencesRange;',
 'typedef struct {uint32_t address;unsigned char bytes[8];} NativePreferencesSite;',
 'typedef struct {uint32_t slot,target;} NativePreferencesPointer;']
lines.append('static const NativePreferencesSite native_preferences_sites[NATIVE_PREFERENCES_SITE_COUNT]={')
for (address,expected,name),data in zip(sites,site_bytes):lines.append(f' {{0x{address:08x}u,{{{bytes_literal(data)}}}}},')
lines.append('};')
# Runtime code and PLT bytes are unchanged. The vtable ranges are still
# compared between stock and current above, but their pointer words have been
# relocated by the loader before the injected observer runs. The command
# mirror installer also runs before native Preferences admission and replaces
# the first two instructions at 0x01efc91c with its already-admitted detour.
# Keep the full Mapping signal destructor in the static stock/current digest
# above, while comparing the still-original tail in the live process. The
# command mirror's own generated anchor admits the displaced eight bytes
# before it changes them.
vtable_addresses={0x069cd0b4,0x068905a0,0x068ca8c4,0x0688eb70,0x069cabac,0x0689c878,0x0689c134}
runtime_ranges=[]
for address,length,name in ranges:
 if address in vtable_addresses:continue
 if address==0x01efc91c:
  if at(current,current_loads,address,8).hex()!='34119fe534219fe5':raise SystemExit('Mapping signal destructor mirror anchor changed')
  runtime_ranges.append((address+8,length-8,name))
 else:runtime_ranges.append((address,length,name))
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
print(f'PASS exact stock/current native UI ranges={len(ranges)}; runtime code guards={len(runtime_ranges)}; relocated pointer guards={len(pointers)}; hooks={len(sites)}; Graphics full-button paint ABI, TextButton label layout, Component bounds/parent/children and setAlwaysOnTop layout, and outer-Page recovery from the post-layout record-end envelope; no direct branch enters displaced second words')
