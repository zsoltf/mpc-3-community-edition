#!/bin/sh
# Fixed package entrypoint; all lifecycle and adapter admission share session.lock.
set -eu
stage=/data/mpclearn-model.manual1
operation=${1:-status}
[ "$#" -le 1 ] || exit 2
case "$operation" in start|status|stop) ;; *) echo 'mcu start|status|stop' >&2;exit 2;; esac
exec "$stage/mcu-session.sh" "$operation"
