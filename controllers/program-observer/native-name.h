/* Bounded UTF-8 copy from an owner-qualified native String. */
#ifndef MPC_NATIVE_NAME_H
#define MPC_NATIVE_NAME_H
static int copy_name(uint32_t pointer_value,unsigned char out[NAME_BYTES],uint32_t *length,uint32_t *status){
 if(pointer_value<4096||pointer_value>UINT32_MAX-NAME_BYTES-1)return 0;
 const volatile unsigned char *text=(const volatile unsigned char*)(uintptr_t)pointer_value;
 unsigned n=0;while(n<NAME_BYTES&&text[n]){out[n]=text[n];n++;}
 int capped=n==NAME_BYTES&&text[n]!=0;unsigned i=0,complete=0;
 while(i<n){
  unsigned start=i,c=out[i++],extra=0,point=0,min=0;
  if(c<0x80){complete=i;continue;}
  if(c>=0xc2&&c<=0xdf){extra=1;point=c&31;min=0x80;}
  else if(c>=0xe0&&c<=0xef){extra=2;point=c&15;min=0x800;}
  else if(c>=0xf0&&c<=0xf4){extra=3;point=c&7;min=0x10000;}
  else return 0;
  if(extra>n-i){if(capped){n=start;break;}return 0;}
  for(unsigned j=0;j<extra;j++){unsigned tail=out[i++];if((tail&0xc0)!=0x80)return 0;point=(point<<6)|(tail&63);}
  if(point<min||point>0x10ffff||(point>=0xd800&&point<=0xdfff))return 0;
  complete=i;
 }
 if(capped&&n>complete)n=complete;
 *length=n;*status=capped?2u:n?1u:0u;return 1;
}
#endif
