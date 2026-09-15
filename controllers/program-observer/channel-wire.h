/* MCU wire encoding grounded in Ardour 74e5e800, mackie/{pot,strip,surface}.
 * Encoding facts only; local names use bounded ASCII substitution. */
#ifndef MPC_CHANNEL_WIRE_H
#define MPC_CHANNEL_WIRE_H
typedef struct {unsigned char name[7],value[7],led[4],ring,color;} ChannelWire;
static inline void channel_ascii(unsigned char out[7],const char *text,unsigned length){
 memset(out,' ',7);unsigned at=0;
 for(unsigned i=0;i<length&&at<6;i++){
  unsigned char c=text[i];if(c>=32&&c<127)out[at++]=c;
  else if((c&0xc0)!=0x80)out[at++]='_';
 }
}
static inline void channel_label(unsigned char out[7],const char *text,unsigned length){
 unsigned char compact[CHANNEL_TEXT];unsigned n=0;
 for(unsigned i=0;i<length&&n<CHANNEL_TEXT;i++){unsigned char c=text[i];if(c==' '||c=='\t')continue;if(c>=32&&c<127)compact[n++]=c;else if((c&0xc0)!=0x80)compact[n++]='_';}
 memset(out,' ',7);if(n<=6)memcpy(out,compact,n);else{memcpy(out,compact,3);memcpy(out+3,compact+n-3,3);}
}
/* Values never use name-style middle elision: it can turn -12.81dB into
 * -121dB. Omit a unit before rounding a long scalar; visibly mark long enums
 * or compound values instead of concatenating unrelated numeric fragments. */
static inline void channel_value(unsigned char out[7],const char *text,unsigned length){
 char compact[CHANNEL_TEXT+1];unsigned n=0;
 for(unsigned i=0;i<length&&n<CHANNEL_TEXT;i++){unsigned char c=text[i];if(c==' '||c=='\t')continue;if(c>=32&&c<127)compact[n++]=c;else if((c&0xc0)!=0x80)compact[n++]='_';}compact[n]=0;
 if(n<=6){channel_ascii(out,compact,n);return;}
 char *end=NULL;double value=strtod(compact,&end);unsigned numeric=(unsigned)(end-compact);
 int scalar=numeric&&isfinite(value);
 for(unsigned i=numeric;i<n;i++)if(!((compact[i]>='A'&&compact[i]<='Z')||(compact[i]>='a'&&compact[i]<='z')||compact[i]=='%'))scalar=0;
 if(!scalar&&numeric<=6&&isinf(value)&&((compact[0]=='-'||compact[0]=='+')?(compact[1]=='i'||compact[1]=='I'):(compact[0]=='i'||compact[0]=='I'))){channel_ascii(out,compact,numeric);return;}
 if(scalar){
  if(numeric<=6){channel_ascii(out,compact,numeric);return;}
  char reduced[64];
  for(int precision=4;precision>=0;precision--){int size=snprintf(reduced,sizeof(reduced),"%.*f",precision,value);if(size>0&&size<=6&&(value==0||strtod(reduced,NULL)!=0)){channel_ascii(out,reduced,(unsigned)size);return;}}
  for(int precision=3;precision>=1;precision--){int size=snprintf(reduced,sizeof(reduced),"%.*g",precision,value);if(size>0&&size<=6){channel_ascii(out,reduced,(unsigned)size);return;}}
 }
 channel_ascii(out,compact,n);out[5]='~';
}
/* Routing choices are names, not scalar parameter values. Preserve the
 * destination suffix and shorten the known native Submix prefix explicitly. */
static inline void channel_routing(unsigned char out[7],const char *text,unsigned length){
 if(length>7&&!memcmp(text,"Submix ",7)){
  unsigned end=7;while(end<length&&text[end]>='0'&&text[end]<='9')end++;
  if(end==length&&end>7&&end-7<=2){char label[7];memcpy(label,"Sub ",4);memcpy(label+4,text+7,end-7);channel_ascii(out,label,4+end-7);return;}
 }
 channel_label(out,text,length);
}
static inline void channel_qlink_label(unsigned char out[7],const char *text,unsigned length,unsigned index){
 char compact[CHANNEL_TEXT+1];unsigned n=0;
 for(unsigned i=0;i<length&&n<CHANNEL_TEXT;i++)if(text[i]!=' '&&text[i]!='\t')compact[n++]=text[i];
 compact[n]=0;
 unsigned left=0,right=0;char property[16];int end=0;
 if(sscanf(compact,"Out%u/%u(%15[^)])%n",&left,&right,property,&end)==3&&end==(int)n&&left&&left<100&&right&&right<100){
  const char *kind=!strcmp(property,"Volume")?"Vol":!strcmp(property,"Pan")?"Pan":!strcmp(property,"Mute")?"Mut":NULL;
  if(kind){char label[16],destination[8];int size=snprintf(destination,sizeof(destination),"%u/%u",left,right);snprintf(label,sizeof(label),"%s%.*s",destination,size<=3?3:1,kind);channel_ascii(out,label,strlen(label));return;}
 }
 if(n)channel_label(out,text,length);
 else{char label[8];snprintf(label,sizeof(label),"Q%u",index+1);channel_ascii(out,label,strlen(label));}
}
static inline unsigned channel_palette(uint32_t argb){
 unsigned r=(argb>>16)&255,g=(argb>>8)&255,b=argb&255,max=r>g?r:g;if(b>max)max=b;
 if(!max)return 0;
 /* X-Touch has seven RGB combinations, not an arbitrary RGB display. */
 return (r*2>=max?1:0)|(g*2>=max?2:0)|(b*2>=max?4:0);
}
static inline ChannelWire channel_wire(const CopiedMirror *s,const CopiedTrack *t,unsigned display_field){
 ChannelWire w;memset(&w,0,sizeof(w));memset(w.name,' ',7);memset(w.value,' ',7);
 if(!t||!s->ready)return w;
 const CopiedField *f=t->fields;
 if(t->pad_owner){char label[8];snprintf(label,sizeof(label),"%c%02u",'A'+t->pad_index/16,t->pad_index%16+1);channel_ascii(w.name,label,strlen(label));}
 else channel_label(w.name,f[CF_NAME].available?f[CF_NAME].text:"------",f[CF_NAME].available?f[CF_NAME].length:6);
 if(f[CF_COLOR].available)w.color=channel_palette(f[CF_COLOR].bits);
 if(!w.color)w.color=7; /* populated black/default stays readable; empty is off */
 unsigned fields[3]={CF_ARM,CF_SOLO,CF_MUTE};for(unsigned i=0;i<3;i++)if(f[fields[i]].available)w.led[i]=f[fields[i]].bits?127:0;
 w.led[3]=!t->pad_owner&&s->selection.available&&s->selected_serial==t->serial?127:0;
 if(f[CF_PAN].available&&mixable(t->vptr)){float pan;memcpy(&pan,&f[CF_PAN].bits,4);w.ring=(unsigned)(pan*10.0f+0.5f)+1;if(pan==0.5f)w.ring|=64;}
 char value[24];
 if(midi_volume(t->vptr)&&t->available&&(display_field==CF_VOLUME||display_field==CF_MIDI_VOLUME)){float volume;memcpy(&volume,&t->bits,4);snprintf(value,sizeof(value),"%3u",(unsigned)(volume*127.0f+0.5f));}
 else if(display_field==CF_PAN&&f[CF_PAN].available){float pan;memcpy(&pan,&f[CF_PAN].bits,4);snprintf(value,sizeof(value),"P%+04d",(int)((pan-0.5f)*200.0f));}
 else if(display_field>=CF_SEND1&&display_field<=CF_SEND4&&f[display_field].available){float send;memcpy(&send,&f[display_field].bits,4);snprintf(value,sizeof(value),"%u:%4.0f",display_field-CF_SEND1+1,send*100.0f);}
 else if(t->available&&mixable(t->vptr)){float volume;memcpy(&volume,&t->bits,4);/* MPC3.9.1 controller7/206 formatter3b7b09c: square taper with +6dB
  * gain, followed by3b7ada4 display floor and near-zero normalization. */
  float gain=volume*volume*1.9952623844146729f;
  float db=gain>0?20.0f*log10f(gain):-100.0f;
  if(db<=-96.0f)snprintf(value,sizeof(value),"-INF");
  else{if(fabsf(db)<0.005f)db=0;snprintf(value,sizeof(value),db>0?"+%.2f":"%.2f",db);}}
 else snprintf(value,sizeof(value),"------");
 channel_ascii(w.value,value,(unsigned)strlen(value));return w;
}
/* Default native playhead presentation adds one to bar/beat only.
 * Current MPC tick display uses native pulses / 10. Raw export is unchanged.
 * Unavailable positions remain visible gaps. */
static inline void channel_position(unsigned char out[10],const CopiedMirror *s){
 memset(out,'-',10);if(!s->position_available||s->bar>=9999||s->beat>=99||s->clock>99999)return;
 char text[11];snprintf(text,sizeof(text),"%4u%2u%4u",s->bar+1,s->beat+1,s->clock/10);memcpy(out,text,10);
}
static inline unsigned channel_lcd(unsigned char bytes[15],unsigned strip,unsigned row,const unsigned char text[7]){
 static const unsigned char header[]={0xf0,0,0,0x66,0x14,0x12};memcpy(bytes,header,6);bytes[6]=(unsigned char)(strip*7+row*56);memcpy(bytes+7,text,7);bytes[14]=0xf7;return 15;
}
static inline unsigned channel_colors(unsigned char bytes[15],const unsigned char colors[8]){
 static const unsigned char header[]={0xf0,0,0,0x66,0x14,0x72};memcpy(bytes,header,6);memcpy(bytes+6,colors,8);bytes[14]=0xf7;return 15;
}
#endif
