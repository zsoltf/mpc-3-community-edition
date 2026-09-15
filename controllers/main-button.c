#define _GNU_SOURCE
#include <alsa/asoundlib.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

struct action {const char *name,*label,*client,*port; unsigned char channel,note;};
static const struct action actions[] = {
    {"main", "MAIN", "mpclearn-main-once", "MAIN once", 15, 119},
    /* configure.py target42: XMM channel1/note93; adapter.c uses ALSA channel0. */
    {"stop", "STOP", "mpclearn-stop-once", "STOP once", 0, 93},
    /* Existing target41: XMM channel1/note94; the verified full X-Touch Play route. */
    {"play", "PLAY", "mpclearn-play-once", "PLAY once", 0, 94},
    /* add-general profile: fixed existing global action targets, no raw-note CLI. */
    {"undo", "UNDO", "mpclearn-undo-once", "UNDO once", 0, 81},
    {"redo", "REDO", "mpclearn-redo-once", "REDO once", 0, 82}
};
static const struct action *parse_action(int argc,char **argv) {
    if((argc!=4 && argc!=6) || (strcmp(argv[1],"--inspect") && strcmp(argv[1],"--send")) ||
       strcmp(argv[2],"--adapter-exe") || argv[3][0]!='/')return NULL;
    if(argc==4)return &actions[0];
    if(strcmp(argv[4],"--action"))return NULL;
    for(unsigned i=0;i<sizeof(actions)/sizeof(actions[0]);++i)
        if(!strcmp(argv[5],actions[i].name))return &actions[i];
    return NULL;
}

struct route { snd_seq_addr_t input, adapter; int mpc_pid, adapter_pid; };
static int same_address(snd_seq_addr_t a, snd_seq_addr_t b) {
    return a.client == b.client && a.port == b.port;
}
static int owns_executable(int pid, const char *expected) {
    char proc[64];
    struct stat running, file;
    if (pid <= 0) return 0;
    snprintf(proc, sizeof(proc), "/proc/%d/exe", pid);
    return !stat(proc, &running) && !stat(expected, &file) && S_ISREG(file.st_mode) &&
           running.st_dev == file.st_dev && running.st_ino == file.st_ino;
}
static int discover(snd_seq_t *seq, const char *adapter_exe, struct route *route) {
    snd_seq_client_info_t *client;
    snd_seq_port_info_t *port;
    snd_seq_port_subscribe_t *subscription;
    snd_seq_client_info_alloca(&client);
    snd_seq_port_info_alloca(&port);
    snd_seq_port_subscribe_alloca(&subscription);
    snd_seq_client_info_set_client(client, -1);
    int inputs = 0, sources = 0, rc;
    while ((rc = snd_seq_query_next_client(seq, client)) >= 0) {
        const char *name = snd_seq_client_info_get_name(client);
        int mpc = !strcmp(name, "MPC"), adapter = !strcmp(name, "mpclearn-controls");
        if (!mpc && !adapter) continue;
        int pid = snd_seq_client_info_get_pid(client);
        if (snd_seq_client_info_get_type(client) != SND_SEQ_USER_CLIENT ||
            !owns_executable(pid, mpc ? "/usr/bin/MPC" : adapter_exe)) return -1;
        snd_seq_port_info_set_client(port, snd_seq_client_info_get_client(client));
        snd_seq_port_info_set_port(port, -1);
        int prc;
        while ((prc = snd_seq_query_next_port(seq, port)) >= 0) {
            if (strcmp(snd_seq_port_info_get_name(port), "mpclearn-controls MIDI")) continue;
            unsigned caps = snd_seq_port_info_get_capability(port);
            if (mpc) {
                if (!(caps & SND_SEQ_PORT_CAP_WRITE) || (caps & SND_SEQ_PORT_CAP_READ)) return -1;
                route->input = *snd_seq_port_info_get_addr(port);
                route->mpc_pid = pid;
                inputs++;
            } else {
                unsigned needed = SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ;
                if ((caps & needed) != needed || (caps & SND_SEQ_PORT_CAP_WRITE)) return -1;
                route->adapter = *snd_seq_port_info_get_addr(port);
                route->adapter_pid = pid;
                sources++;
            }
        }
        if (prc != -ENOENT) return -1;
    }
    if (rc != -ENOENT || inputs != 1 || sources != 1) return -1;
    snd_seq_port_subscribe_set_sender(subscription, &route->adapter);
    snd_seq_port_subscribe_set_dest(subscription, &route->input);
    return snd_seq_get_port_subscription(seq, subscription) < 0 ? -1 : 0;
}
static int unchanged(const struct route *a, const struct route *b) {
    return same_address(a->input, b->input) && same_address(a->adapter, b->adapter) &&
        a->mpc_pid == b->mpc_pid && a->adapter_pid == b->adapter_pid;
}
static void action_event(const struct action *action, snd_seq_event_t *event, int source, snd_seq_addr_t destination, int release) {
    snd_seq_ev_clear(event);
    if (release) snd_seq_ev_set_noteoff(event, action->channel, action->note, 0);
    else snd_seq_ev_set_noteon(event, action->channel, action->note, 127);
    snd_seq_ev_set_source(event, source);
    snd_seq_ev_set_dest(event, destination.client, destination.port);
    snd_seq_ev_set_direct(event);
}
int main(int argc, char **argv) {
    const struct action *action=parse_action(argc,argv);
    if (!action) {
        fputs("main-button --inspect|--send --adapter-exe /absolute/retained/mpclearn-controls [--action main|stop|play|undo|redo]\n", stderr);
        return 2;
    }
    /* Nonblocking ALSA calls plus a hard lifetime bound; no retry or resident
       process. Closing the client removes the helper's only temporary port. */
    alarm(3);
    snd_seq_t *seq = NULL;
    int result = 1;
    if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_OUTPUT, SND_SEQ_NONBLOCK) < 0) goto done;
    if (snd_seq_set_client_name(seq, action->client) < 0) goto done;
    struct route route = {0}, current = {0};
    if (discover(seq, argv[3], &route)) {
        fprintf(stderr,"%s route missing, ambiguous, wrong process owner or unsubscribed\n",action->label); goto done;
    }
    printf("MPC pid=%d input=%u:%u adapter pid=%d source=%u:%u\n", route.mpc_pid,
        route.input.client, route.input.port, route.adapter_pid, route.adapter.client, route.adapter.port);
    printf("ACTION %s channel=%u note=%u press=127 release=0\n",action->label,action->channel+1,action->note);
    if (!strcmp(argv[1], "--inspect")) { result = 0; goto done; }
    /* READ identifies a normal source. No SUBS_READ: this one-shot helper uses
       an explicit destination and does not advertise a new controller input. */
    int source = snd_seq_create_simple_port(seq, action->port, SND_SEQ_PORT_CAP_READ,
        SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
    if (source < 0 || discover(seq, argv[3], &current) || !unchanged(&route, &current)) goto done;
    for (int release = 0; release < 2; release++) {
        snd_seq_event_t event;
        action_event(action, &event, source, route.input, release);
        int rc = snd_seq_event_output_direct(seq, &event);
        if (rc < 0) {
            fprintf(stderr, "%s %s failed: %s\n", action->label, release ? "release" : "press", snd_strerror(rc)); goto done;
        }
    }
    if (snd_seq_drain_output(seq) < 0) goto done;
    printf("%s press/release delivered; observe MPC UI separately\n",action->label);
    result = 0;
done:
    if (seq) snd_seq_close(seq);
    alarm(0);
    if (result) fprintf(stderr,"%s helper failed; no retries\n",action->label);
    return result;
}
