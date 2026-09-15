/* MAIN/STOP native baseline; Play helper events below are local synthetic composition. */
#define main main_button_program_main
#include "main-button.c"
#undef main
#include <assert.h>
int main(void) {
    assert(owns_executable(getpid(), "/proc/self/exe"));
    assert(!owns_executable(0, "/proc/self/exe"));
    assert(!owns_executable(getpid(), "/bin/sh"));
    struct route a = {{129, 8}, {130, 1}, 7107, 6746}, b = a;
    assert(unchanged(&a, &b));
    b.input.port = 11; assert(!unchanged(&a, &b));
    b = a; b.mpc_pid++; assert(!unchanged(&a, &b));
    b = a; b.adapter.client++; assert(!unchanged(&a, &b));
    char *args[]={"main-button","--send","--adapter-exe","/retained/mpclearn-controls","--action","main"};
    assert(parse_action(4,args)==&actions[0]);
    assert(parse_action(6,args)==&actions[0]);
    args[1]="--inspect";assert(parse_action(4,args)==&actions[0]);
    args[5]="stop";assert(parse_action(6,args)==&actions[1]);
    args[1]="--send";assert(parse_action(6,args)==&actions[1]);
    args[5]="play";assert(parse_action(6,args)==&actions[2]);
    args[1]="--inspect";assert(parse_action(6,args)==&actions[2]);
    assert(sizeof(actions)/sizeof(actions[0])==5);
    for(unsigned i=3;i<5;i++){args[5]=(char*)actions[i].name;assert(parse_action(6,args)==actions+i);}
    for(unsigned i=0;i<7;++i) {
        const char *bad[]={"save","STOP","","main,stop","record","cycle","click"};args[5]=(char*)bad[i];
        assert(!parse_action(6,args));
        assert(main_button_program_main(6,args)==2); /* rejects before any ALSA open */
    }
    assert(!parse_action(5,args));assert(!parse_action(1,args));
    args[5]="stop";args[4]="--note";assert(!parse_action(6,args));args[4]="--action";
    assert(!strcmp(actions[0].client,"mpclearn-main-once")&&!strcmp(actions[0].port,"MAIN once"));
    assert(!strcmp(actions[1].client,"mpclearn-stop-once")&&!strcmp(actions[1].port,"STOP once"));
    assert(!strcmp(actions[2].client,"mpclearn-play-once")&&!strcmp(actions[2].port,"PLAY once"));
    const unsigned char notes[]={119,93,94,81,82};
    snd_midi_event_t *coder=NULL;assert(!snd_midi_event_new(16,&coder));
    for(unsigned action=0;action<5;++action)for (int release = 0; release <= 1; release++) {
        snd_seq_event_t event;
        memset(&event, 0xff, sizeof(event));
        action_event(&actions[action], &event, 3, a.input, release);
        assert(event.type == (release ? SND_SEQ_EVENT_NOTEOFF : SND_SEQ_EVENT_NOTEON));
        assert(event.data.note.channel == (action?0:15) && event.data.note.note == notes[action]);
        assert(event.data.note.velocity == (release ? 0 : 127));
        assert(event.source.port == 3 && same_address(event.dest, a.input));
        assert(event.queue == SND_SEQ_QUEUE_DIRECT);
        assert(event.time.tick == 0 && event.data.note.duration == 0);
        unsigned char wire[3]={0};snd_midi_event_reset_decode(coder);
        assert(snd_midi_event_decode(coder,wire,sizeof(wire),&event)==3);
        assert(wire[0]==((release?0x80:0x90)|(action?0:15)));
        assert(wire[1]==notes[action]&&wire[2]==(release?0:127));
    }
    snd_midi_event_free(coder);
    puts("PASS: default/explicit MAIN bytes9f777f/8f7700 STOP905d7f/805d00, PLAY905e7f/805e00, Undo/Redo exact notes81/82; Record/Cycle/Click rejected for bridge ownership, rejected actions, source/routing preservation (synthetic events; no ALSA device)");
    return 0;
}
