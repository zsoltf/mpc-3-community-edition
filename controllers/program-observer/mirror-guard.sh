#!/bin/sh
# A copy destination must be all zero before its first use: copy_channel no
# longer re-clears the text bytes it never writes (mirror-read.c). MIRROR_COPY
# and MIRROR_FIELD are the only declarations that satisfy that, because they
# give the object static storage duration, which the language zero-initialises.
# This guard fails the build if any other declaration of a CopiedMirror or
# CopiedField object appears, so the rule cannot be lost by a later edit.
# Pointers and parameters are not objects and are not matched.
set -eu
cd "$(dirname "$0")"
bad=$(grep -nE '(^|[^A-Za-z0-9_*])Copied(Mirror|Field)[[:space:]]+[A-Za-z_][A-Za-z0-9_]*[[:space:]]*(=[[:space:]]*\{[^}]*\})?[;,)]' \
        -- *.c *.h *.inc 2>/dev/null \
      | grep -v '^mirror-read\.c:[0-9]*:typedef ' \
      | grep -v 'Copied\(Mirror\|Field\) \*' \
      | grep -v '^mirror-read\.c:[0-9]*:#define MIRROR_\(COPY\|FIELD\)(name)' \
      || true)
if [ -n "$bad" ]; then
  echo 'mirror-guard: a copy destination must be declared with MIRROR_COPY() or MIRROR_FIELD();' >&2
  echo 'a plain declaration leaves it uninitialised, and the copy no longer clears what it never writes:' >&2
  echo "$bad" >&2
  exit 1
fi
# A heap destination is the same hazard by another route: malloc leaves it
# uninitialised. calloc satisfies the same precondition the language gives a
# static object, so it is the only accepted allocator for one.
heap=$(grep -nE 'Copied(Mirror|Field)[^;]*=[^;]*\bmalloc[[:space:]]*\(' -- *.c *.h *.inc 2>/dev/null || true)
if [ -n "$heap" ]; then
  echo 'mirror-guard: a heap copy destination must come from calloc, not malloc:' >&2
  echo "$heap" >&2
  exit 1
fi
echo 'mirror-guard: every CopiedMirror/CopiedField object is declared through MIRROR_COPY/MIRROR_FIELD'
