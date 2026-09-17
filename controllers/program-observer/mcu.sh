#!/bin/sh
# Fixed package entrypoint; all lifecycle and adapter admission share session.lock.
# The package runs where it is installed; mcu-session.sh admits only the image
# location and the single development override.
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
operation=${1:-status}
[ "$#" -le 1 ] || exit 2
case "$operation" in start|status|stop) ;; *) echo 'mcu start|status|stop' >&2;exit 2;; esac
exec "$here/mcu-session.sh" "$operation"
