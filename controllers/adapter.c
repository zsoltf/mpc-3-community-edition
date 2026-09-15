#define _GNU_SOURCE
#include <alsa/asoundlib.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "stop-route.h"

#define CLIENT_NAME "mpclearn-controls"
#define SOURCE_NAME "mpclearn-controls MIDI"
#define BURST_LIMIT 256

static volatile sig_atomic_t stopping, arm_requested;
static unsigned stop_down;
static const char *stop_bridge_exe;
static const snd_seq_addr_t absent_source={255,255};
static int present(snd_seq_addr_t a){return a.client<254;}
static void on_signal(int sig) {
    if (sig == SIGUSR1) arm_requested = 1;
    else stopping = 1;
}
static int equal(snd_seq_addr_t a, snd_seq_addr_t b) {
    return a.client == b.client && a.port == b.port;
}

/* Exact names plus readable/subscribable capability: never use cached USB IDs. */
static int resolve(snd_seq_t *seq, const char *client, const char *port,
                   snd_seq_addr_t *address) {
    snd_seq_client_info_t *ci;
    snd_seq_port_info_t *pi;
    snd_seq_client_info_alloca(&ci);
    snd_seq_port_info_alloca(&pi);
    snd_seq_client_info_set_client(ci, -1);
    int matches = 0, rc;
    while ((rc = snd_seq_query_next_client(seq, ci)) >= 0) {
        if (strcmp(snd_seq_client_info_get_name(ci), client)) continue;
        snd_seq_port_info_set_client(pi, snd_seq_client_info_get_client(ci));
        snd_seq_port_info_set_port(pi, -1);
        int prc;
        while ((prc = snd_seq_query_next_port(seq, pi)) >= 0) {
            if (strcmp(snd_seq_port_info_get_name(pi), port)) continue;
            unsigned caps = snd_seq_port_info_get_capability(pi);
            unsigned required = SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ;
            if ((caps & required) != required) return -1;
            *address = *snd_seq_port_info_get_addr(pi);
            matches++;
        }
        if (prc != -ENOENT) return -1;
    }
    return rc != -ENOENT || matches > 1 ? -1 : matches == 1 ? 0 : 1;
}

/* Keep the established virtual control port alive across hardware reconnects.
 * Missing Mini and missing full surface are separate availability states. */
static int refresh_sources(snd_seq_t *seq,int sink,snd_seq_addr_t *mini,snd_seq_addr_t *full){
 snd_seq_addr_t a=absent_source,b=absent_source;int ma=resolve(seq,"X-TOUCH MINI","X-TOUCH MINI MIDI 1",&a),fb=resolve(seq,"X-Touch","X-TOUCH_INT",&b);
 if(ma<0||fb<0)return 0;
 if(ma)a=absent_source;
 if(fb)b=absent_source;
 if(equal(a,*mini)&&equal(b,*full))return 1;
 if(present(*mini))snd_seq_disconnect_from(seq,sink,mini->client,mini->port);
 if(present(*full))snd_seq_disconnect_from(seq,sink,full->client,full->port);
 if(snd_seq_drop_input(seq)<0)return 0;
 stop_down=0;*mini=a;*full=b;
 if((present(a)&&snd_seq_connect_from(seq,sink,a.client,a.port)<0)||(present(b)&&snd_seq_connect_from(seq,sink,b.client,b.port)<0))return 0;
 printf("SOURCES mini=%s full=%s; old input discarded, virtual control port retained\n",present(a)?"connected":"absent",present(b)?"connected":"absent");fflush(stdout);return 1;
}

/* Build new fixed-length events; no forwarding of addresses, queue or payload. */
static int translate(const snd_seq_event_t *in, snd_seq_addr_t mini,
                     snd_seq_addr_t full, snd_seq_event_t *out) {
    snd_seq_ev_clear(out);
    if (present(mini) && equal(in->source, mini) && in->type == SND_SEQ_EVENT_CONTROLLER &&
        in->data.control.channel == 10 &&
        in->data.control.param >= 1 && in->data.control.param <= 8 &&
        in->data.control.value >= 0 && in->data.control.value <= 127) {
        snd_seq_ev_set_controller(out, 0, in->data.control.param + 12, in->data.control.value);
        return 1;
    }
    /* Full X-Touch input is owned by the native bridge. The adapter retains
       its physical subscription for lifecycle compatibility, but never
       duplicates transport or history into Global MIDI Learn. */
    (void)full;
    return 0;
}

static int source_lost(const snd_seq_event_t *e, snd_seq_addr_t mini,
                       snd_seq_addr_t full, snd_seq_addr_t ingress) {
    if (e->source.client != SND_SEQ_CLIENT_SYSTEM ||
        e->source.port != SND_SEQ_PORT_SYSTEM_ANNOUNCE) return 0;
    if (e->type == SND_SEQ_EVENT_CLIENT_EXIT)
        return e->data.addr.client == mini.client || e->data.addr.client == full.client;
    if (e->type == SND_SEQ_EVENT_PORT_EXIT)
        return equal(e->data.addr, mini) || equal(e->data.addr, full);
    if (e->type == SND_SEQ_EVENT_PORT_UNSUBSCRIBED)
        return equal(e->data.connect.dest, ingress) &&
               (equal(e->data.connect.sender, mini) || equal(e->data.connect.sender, full));
    return 0;
}

int main(int argc, char **argv) {
    if(argc==2&&!strncmp(argv[1],"--stop-bridge-exe=",18)&&argv[1][18]=='/')stop_bridge_exe=argv[1]+18;
    else if(argc!=1){fputs("adapter [--stop-bridge-exe=/exact/bridge]; starts disarmed\n",stderr);return 2;}
    int result = 1, lockfd = -1;
    snd_seq_t *seq = NULL;
    struct pollfd *fds = NULL;
    struct sockaddr_un lock = { .sun_family = AF_UNIX };
    const char lockname[] = "mpclearn-controls-single-instance";
    memcpy(lock.sun_path + 1, lockname, sizeof(lockname) - 1);
    lockfd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (lockfd < 0 || bind(lockfd, (struct sockaddr *)&lock,
        offsetof(struct sockaddr_un, sun_path) + sizeof(lockname)) < 0) {
        perror("single-instance lock (another adapter may be running)"); goto done;
    }
    struct sigaction sa = { .sa_handler = on_signal };
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGTERM, &sa, NULL) || sigaction(SIGINT, &sa, NULL) ||
        sigaction(SIGUSR1, &sa, NULL)) { perror("sigaction"); goto done; }
    int rc = snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, SND_SEQ_NONBLOCK);
    if (rc < 0) { fprintf(stderr, "sequencer open: %s\n", snd_strerror(rc)); goto done; }
    if (snd_seq_set_client_name(seq, CLIENT_NAME) < 0 ||
        snd_seq_set_client_pool_input(seq, 1024) < 0) goto alsa_error;
    snd_seq_addr_t mini=absent_source, full=absent_source;
    int sink = snd_seq_create_simple_port(seq, "private ingress",
        /* Owner-created incoming subscriptions do not require SUBS_WRITE.
           Omitting it blocks another sender from auto-subscribing itself;
           NO_EXPORT alone only blocks connections made by a third client. */
        SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_NO_EXPORT,
        SND_SEQ_PORT_TYPE_APPLICATION);
    if (sink < 0) goto alsa_error;
    int source = snd_seq_create_simple_port(seq, SOURCE_NAME,
        SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ,
        SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
    if (source < 0) goto alsa_error;
    snd_seq_addr_t ingress = { .client = snd_seq_client_id(seq), .port = sink };
    if (snd_seq_connect_from(seq, sink, SND_SEQ_CLIENT_SYSTEM, SND_SEQ_PORT_SYSTEM_ANNOUNCE) < 0 ||
        !refresh_sources(seq,sink,&mini,&full)) goto alsa_error;
    int count = snd_seq_poll_descriptors_count(seq, POLLIN);
    if (count <= 0 || count > 16) goto alsa_error;
    fds = calloc((size_t)count, sizeof(*fds));
    if (!fds || snd_seq_poll_descriptors(seq, fds, (unsigned)count, POLLIN) != count) goto alsa_error;
    printf("DISARMED pid=%ld source=%d:%d name=%s mini=%u:%u full=%u:%u\n",
        (long)getpid(), snd_seq_client_id(seq), source, SOURCE_NAME,
        mini.client, mini.port, full.client, full.port);
    fflush(stdout);
    int armed = 0;
    unsigned long forwarded = 0;
    while (!stopping) {
        /* Re-query on every bounded iteration, including silence and before arming. */
        if (!refresh_sources(seq,sink,&mini,&full)) {
            fprintf(stderr, "Ambiguous/unreadable controller or subscription error; exiting\n"); goto done;
        }
        if (arm_requested) {
            arm_requested = 0;
            if (!armed) {
                /* Drain rather than blindly discard: a disconnect notification
                   must still fail closed when USB addresses get reused. */
                int drained;
                for (drained = 0; drained < BURST_LIMIT; drained++) {
                    snd_seq_event_t *old;
                    rc = snd_seq_event_input(seq, &old);
                    if (rc == -EAGAIN) break;
                    if (rc < 0) goto alsa_error;
                    if(source_lost(old,mini,full,ingress)){if(!refresh_sources(seq,sink,&mini,&full))goto alsa_error;break;}
                }
                if (drained == BURST_LIMIT || !refresh_sources(seq,sink,&mini,&full)) goto alsa_error;
                armed = 1;
                printf("ARMED; pre-arm queue discarded\n"); fflush(stdout);
            }
        }
        int ready = poll(fds, (nfds_t)count, 100);
        if (ready < 0) { if (errno == EINTR) continue; perror("poll"); goto done; }
        for (int i = 0; i < count; i++)
            if (fds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) goto alsa_error;
        if (!ready) continue;
        int n;
        for (n = 0; n < BURST_LIMIT && !stopping && !arm_requested; n++) {
            snd_seq_event_t *event;
            rc = snd_seq_event_input(seq, &event);
            if (rc == -EAGAIN) break;
            if (rc < 0) { fprintf(stderr, "input failure/overrun: %s\n", snd_strerror(rc)); goto done; }
            if (source_lost(event, mini, full, ingress)) {
                /* Re-query next iteration; do not forward queued events from an old
                   source address or replay them after reconnect. */
                if(snd_seq_drop_input(seq)<0)goto alsa_error;
                stop_down=0;
                if(present(mini))snd_seq_disconnect_from(seq,sink,mini.client,mini.port);
                if(present(full))snd_seq_disconnect_from(seq,sink,full.client,full.port);
                mini=full=absent_source;break;
            }
            snd_seq_event_t out;
            if (armed && translate(event, mini, full, &out)) {
                int matched=0; snd_seq_addr_t relay=absent_source;
                if(stop_bridge_exe&&(out.type==SND_SEQ_EVENT_NOTEON||out.type==SND_SEQ_EVENT_NOTEOFF)&&out.data.note.note==93){
                    unsigned down=out.type==SND_SEQ_EVENT_NOTEON&&out.data.note.velocity;
                    if(down&&stop_down)continue;
                    stop_down=down;int relay_pid=0;
                    matched=stop_find_port(seq,"mpclearn-mirror-motors",STOP_RELAY_PORT,stop_bridge_exe,SND_SEQ_PORT_CAP_WRITE,SND_SEQ_PORT_CAP_READ,&relay,&relay_pid);
                    if(matched<0)fputs("Stop relay identity unavailable; ordinary Stop fallback\n",stderr);
                }
                rc=stop_deliver(seq,&out,source,matched,relay);
                if (rc < 0) { fprintf(stderr, "output failure: %s\n", snd_strerror(rc)); goto done; }
                forwarded++;
            }
        }
        if (n == BURST_LIMIT && snd_seq_event_input_pending(seq, 1) != 0) {
            fprintf(stderr, "Input burst exceeds bound; exiting without replay\n"); goto done;
        }
    }
    printf("STOPPED forwarded=%lu; no transport command sent on exit\n", forwarded);
    result = 0;
    goto done;
alsa_error:
    fprintf(stderr, "ALSA setup/poll/queue failure; exiting\n");
done:
    free(fds);
    if (seq) snd_seq_close(seq);
    if (lockfd >= 0) close(lockfd);
    return result;
}
