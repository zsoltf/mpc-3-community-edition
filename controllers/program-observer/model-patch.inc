/* Same gate and all-or-nothing installer; model mode has independent anchors. */
static void jump(uint32_t **p,uint32_t to){*(*p)++=0xe51ff004u;*(*p)++=to;}
static void mov32(uint32_t **p,unsigned reg,uint32_t n){*(*p)++=0xe3000000u|(reg<<12)|((n&0xf000)<<4)|(n&0xfff);*(*p)++=0xe3400000u|(reg<<12)|((n&0xf0000000)>>12)|((n>>16)&0xfff);}
static size_t build_stub(unsigned char *dest,unsigned site,uint32_t bias){
 unsigned char *start=dest;
 /* Successful STREX sites replay the original pair including DMB before even
  * the gate pushes context. Program stores likewise precede their observation. */
 if(capture_after_pair[site]){memcpy(dest,expected[site],8);dest+=8;}
 size_t n=(size_t)(gate_end-gate_begin);
 memcpy(dest,gate_begin,n);
 *(uint32_t*)(dest+(gate_handler-gate_begin))=(uint32_t)(uintptr_t)&observer_hook;
 #ifdef VOLUME_MIRROR
 *(uint32_t*)(dest+(gate_handler-gate_begin)+4)=MODEL_HOOK_BASE+hook_ids[site];
#else
 *(uint32_t*)(dest+(gate_handler-gate_begin)+4)=MODEL_HOOK_BASE+site;
#endif
 dest+=n;
 uint32_t *out=(uint32_t*)dest;
 for(unsigned j=0;!capture_after_pair[site]&&j<2;j++){
  uint32_t instruction;memcpy(&instruction,expected[site]+4*j,4);
  if(branch_target[site][j]){jump(&out,bias+branch_target[site][j]);return (unsigned char*)out-start;}
  if(relocated[site][j])mov32(&out,(instruction>>12)&15,relocated[site][j]);
  else *out++=instruction;
 }
 jump(&out,bias+anchor[site]+8);return (unsigned char*)out-start;
}
