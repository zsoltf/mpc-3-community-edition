#define _GNU_SOURCE
#include <alsa/asoundlib.h>
static int relay_output(snd_seq_t*,snd_seq_event_t*);
#define snd_seq_event_output_direct relay_output
/* Post-implementation regression checks. Synthetic events, no MIDI devices. */
#define main adapter_program_main
#include "adapter.c"
#undef main
#undef snd_seq_event_output_direct
#include <assert.h>

static unsigned relay_calls;static int relay_result;static snd_seq_addr_t relay_dest[2];
static int relay_output(snd_seq_t *seq,snd_seq_event_t *event){(void)seq;assert(relay_calls<2);relay_dest[relay_calls++]=event->dest;return relay_calls==1?relay_result:0;}
static void relay_checks(void){
 snd_seq_event_t event;snd_seq_ev_clear(&event);snd_seq_ev_set_noteon(&event,0,93,127);snd_seq_addr_t target={140,2};
 for(int matched=-1;matched<=1;matched++){
  relay_calls=0;relay_result=0;assert(stop_deliver(NULL,&event,7,matched,target)==0&&relay_calls==1);
  assert(relay_dest[0].client==(matched==1?140:SND_SEQ_ADDRESS_SUBSCRIBERS));
 }
 for(unsigned i=0;i<3;i++){
  relay_calls=0;relay_result=i==0?-ENOENT:i==1?-ENXIO:-EAGAIN;int result=stop_deliver(NULL,&event,7,1,target);
  assert(relay_calls==(i<2?2:1));assert(result==(i<2?0:-EAGAIN));
  if(i<2)assert(relay_dest[1].client==SND_SEQ_ADDRESS_SUBSCRIBERS);
 }
 assert(stop_executable(getpid(),"/proc/self/exe")&&!stop_executable(getpid(),"/does-not-exist"));
 puts("PASS actual adapter one-destination delivery, absent/mismatched relay fallback and definite nondelivery fallback; ALSA sends substituted");
}
int main(void) {
    relay_checks();
    const snd_seq_addr_t mini = {24, 0}, full = {32, 0}, ingress = {131, 0};
    snd_seq_event_t in, out;
    for (int channel = 0; channel < 16; channel++) {
        for (int cc = 0; cc < 128; cc++) {
            for (int value = 0; value < 128; value++) {
                snd_seq_ev_clear(&in);
                in.source = mini;
                snd_seq_ev_set_controller(&in, channel, cc, value);
                in.dest = full;
                in.queue = 7;
                in.time.tick = 12345;
                int accepted = translate(&in, mini, full, &out);
                assert(accepted == (channel == 10 && cc >= 1 && cc <= 8));
                if (accepted) {
                    assert(out.type == SND_SEQ_EVENT_CONTROLLER);
                    assert(out.data.control.channel == 0 && out.data.control.param == (unsigned)cc + 12);
                    assert(out.data.control.value == value);
                    assert(out.queue == 0 && out.time.tick == 0);
                    assert(out.dest.client == 0 && out.dest.port == 0);
                    assert(out.source.client == 0 && out.source.port == 0);
                }
            }
        }
    }
    for (int channel = 0; channel < 16; channel++) {
        for (int note = 0; note < 128; note++) {
            for (int velocity = 0; velocity < 128; velocity++) {
                for (int off = 0; off < 2; off++) {
                    snd_seq_ev_clear(&in);
                    if (off) snd_seq_ev_set_noteoff(&in, channel, note, velocity);
                    else snd_seq_ev_set_noteon(&in, channel, note, velocity);
                    in.source = full;
                    int accepted = translate(&in, mini, full, &out);
                    assert(!accepted);
                }
            }
        }
    }
    /* Full X-Touch transport/history/modifier input stays out of the adapter;
       the native bridge owns it even when the Mini is absent. */
    const unsigned notes[]={70,81,70,81,81,81,82,94,94};
    const unsigned velocities[]={127,127,0,0,127,0,127,127,0};
    for(unsigned i=0;i<sizeof(notes)/sizeof(notes[0]);i++){
        snd_seq_ev_clear(&in);snd_seq_ev_set_noteon(&in,0,notes[i],velocities[i]);in.source=full;
        assert(!translate(&in,absent_source,full,&out));
    }
    in.source=mini;snd_seq_ev_set_controller(&in,10,1,64);
    assert(!translate(&in,absent_source,full,&out));
    snd_seq_ev_clear(&in);
    snd_seq_ev_set_controller(&in, 10, 1, 64);
    /* LC, MPC output, full DIN, our own source, wrong Mini port are rejected. */
    const snd_seq_addr_t excluded[] = {{28, 0}, {129, 22}, {32, 1}, {131, 1}, {24, 1}};
    for (size_t i = 0; i < sizeof(excluded) / sizeof(excluded[0]); i++) {
        in.source = excluded[i];
        assert(!translate(&in, mini, full, &out));
    }
    in.source = mini;
    in.data.control.value = -1;
    assert(!translate(&in, mini, full, &out));
    in.data.control.value = 128;
    assert(!translate(&in, mini, full, &out));
    for (int type = 0; type < 256; type++) {
        snd_seq_ev_clear(&in);
        in.type = type;
        if (type == SND_SEQ_EVENT_CONTROLLER || type == SND_SEQ_EVENT_NOTEON || type == SND_SEQ_EVENT_NOTEOFF)
            continue;
        in.source = mini;
        assert(!translate(&in, mini, full, &out));
        in.source = full;
        assert(!translate(&in, mini, full, &out));
    }
    snd_seq_ev_clear(&in);
    in.source = (snd_seq_addr_t){SND_SEQ_CLIENT_SYSTEM, SND_SEQ_PORT_SYSTEM_ANNOUNCE};
    in.type = SND_SEQ_EVENT_PORT_EXIT;
    in.data.addr = mini;
    assert(source_lost(&in, mini, full, ingress));
    in.data.addr = (snd_seq_addr_t){32, 1};
    assert(!source_lost(&in, mini, full, ingress));
    in.type = SND_SEQ_EVENT_CLIENT_EXIT;
    assert(source_lost(&in, mini, full, ingress));
    in.type = SND_SEQ_EVENT_PORT_UNSUBSCRIBED;
    in.data.connect.sender = full;
    in.data.connect.dest = ingress;
    assert(source_lost(&in, mini, full, ingress));
    in.data.connect.dest.port = 1;
    assert(!source_lost(&in, mini, full, ingress));
    in.data.connect.dest = ingress;
    in.source = full;
    assert(!source_lost(&in, mini, full, ingress));
    puts("PASS: Mini-only mapping, exhaustive full X-Touch exclusion, fresh event fields, source exclusions and loss notifications (synthetic events)");
    return 0;
}
