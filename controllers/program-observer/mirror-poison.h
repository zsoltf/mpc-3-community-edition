/* Fixture-only helper. Shipped binaries never include it.
 *
 * Poisoning a copy destination is how a fixture proves that every byte the
 * copy MUST write really is written rather than inherited from the buffer.
 * That check still applies to everything except one region: copy_channel
 * maintains each field's text rather than re-clearing it, so a destination is
 * required to start with its text arrays and lengths zero (see MIRROR_COPY in
 * mirror-read.c). This fills the destination and then restores exactly that
 * region, leaving a buffer that is simultaneously a valid destination and
 * poison everywhere the copy is obliged to overwrite.
 *
 * Whether the text maintenance itself is exact is a different question, and a
 * poisoned pair cannot answer it. That is pinned separately, by reusing one
 * destination across copies whose contents differ (snapshot_clear_checks). */
#ifndef MPC_MIRROR_POISON_H
#define MPC_MIRROR_POISON_H
/* The region a destination must start zero is everything from text to the end
 * of the field: the 97-byte array and the three bytes of trailing padding
 * after it, neither of which copy_channel writes. length goes with them,
 * because it is what says how much of the text the copy has to restore. */
static void mirror_poison_field(CopiedField *f){
 f->length=0;memset((char*)f+offsetof(CopiedField,text),0,sizeof(*f)-offsetof(CopiedField,text));
}
static void mirror_poison(CopiedMirror *m,unsigned char v){
 memset(m,v,sizeof(*m));
 CopiedField *header[]={&m->selection,&m->master,&m->playing,&m->automation,&m->loop,&m->record_mode,&m->click};
 for(unsigned i=0;i<sizeof(header)/sizeof(header[0]);i++)mirror_poison_field(header[i]);
 for(unsigned i=0;i<MIRROR_TRACKS;i++)for(unsigned f=0;f<CF_COUNT;f++)mirror_poison_field(m->tracks[i].fields+f);
 for(unsigned i=0;i<PAD_SLOTS;i++)for(unsigned f=0;f<CF_COUNT;f++)mirror_poison_field(m->pads[i].fields+f);
}
#endif
