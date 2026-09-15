/* Original finite direct-binding diagnostic, MIT; see LICENSE. ARM32 only. */
#ifndef PROGRAM_OBSERVER_H
#define PROGRAM_OBSERVER_H
#include <stdint.h>
#include <stdatomic.h>
#if defined(MODEL_CAPTURE) || defined(VOLUME_MIRROR)
#include "model.h"
#ifdef VOLUME_MIRROR
#undef PATCH_COUNT
#if defined(MIRROR_COMMAND)
#define PATCH_COUNT 290u
#elif defined(UI_WITNESS)
#define PATCH_COUNT 31u
#else
#define PATCH_COUNT 59u
#endif
#include "mirror-state.h"
#endif
#else
#define PATCH_COUNT 8u
#endif
#ifdef MODEL_CAPTURE
#define BINDING_COUNT (MODEL_TRACKS*7u)
#else
#define BINDING_COUNT 64u
#endif
#define RECORD_COUNT 24576u
#include "config.h"
#ifdef MIRROR_COMMAND
_Static_assert(WINDOW_SECONDS==0 || (WINDOW_SECONDS>=60 && WINDOW_SECONDS<=900),"manual or finite window");
#else
_Static_assert(WINDOW_SECONDS>=60 && WINDOW_SECONDS<=900,"finite window");
#endif
_Static_assert(ATOMIC_INT_LOCK_FREE==2,"native lock-free words");
#define CONSUMER_STACK 262144u
#ifdef MODEL_CAPTURE
#define STREAM_VERSION 3u
#define STREAM_MAGIC 0x314c444du /* MDL1: finite rooted model samples, never motor input. */
#else
#define STREAM_VERSION 3u
#define STREAM_MAGIC 0x334f5250u /* PRO3 adds absolute-clock heartbeats. */
#endif
#define NAME_BYTES 96u
enum {P_OWNER,P_BINDER,P_INITIAL,P_CHANGED,P_DISCONNECT,P_PROJECT_FALLBACK,P_PROJECT_LOAD,P_METADATA};
enum {OWNER=1,OWNER_CONTEXT,BINDER,INITIAL,CHANGED,DISCONNECT,PROJECT,ARMED,REPLACED,UNEXPECTED_BINDER,METADATA,METADATA_NAME,METADATA_END,HEARTBEAT};
enum {OK=0,CONTENTION=1,OVERFLOW,IDENTITY,ORDER,VALUE,IO,CLOSING_WRITER,SETUP,CLOCK_ERROR,ADMISSION_BUSY,RESERVATION_LOSS,METADATA_INVALID};
enum {SETUP_MMAP=1,SETUP_ATTR,SETUP_PROTECT,SETUP_STACK,SETUP_ATFORK,SETUP_THREADS,SETUP_PATCHES,SETUP_CREATE};
typedef struct {uint64_t d[32];uint32_t apsr,fpscr,r[13],lr;} Context;
_Static_assert(sizeof(Context)==320,"saved ARM context");
/* Names retain existing consumer/footer layout; kind defines wire operands. */
typedef struct {uint32_t seq,kind,epoch,generation,pair,opaque,value,tick;} Record;
_Static_assert(sizeof(Record)==32,"copied wire row");
typedef struct {_Atomic uint32_t busy;unsigned char padding[2048-sizeof(_Atomic uint32_t)];} AdmissionStorage;
_Static_assert(sizeof(AdmissionStorage)==2048,"full exclusive granule isolation");
extern AdmissionStorage observer_admission;
#define observer_busy (observer_admission.busy)
extern _Atomic uint32_t observer_running,observer_pair,observer_filter[BINDING_COUNT],observer_wrappers[BINDING_COUNT];
extern uint32_t observer_image_bias;
extern unsigned char gate_begin[],gate_end[],gate_handler[];
extern unsigned char fast_begin[],fast_end[],fast_running[],fast_filter[],fast_skip[],fast_target[];
void observer_hook(Context *,unsigned);
#endif
