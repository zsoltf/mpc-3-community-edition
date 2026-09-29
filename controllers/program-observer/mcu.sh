#!/bin/sh
# Fixed package entrypoint; all lifecycle and adapter admission share session.lock.
# The package runs where it is installed; mcu-session.sh admits only the image
# location and the single development override.
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
operation=${1:-status}
case "$operation" in start|status|stop|list-midi|configure) ;; *) echo 'mcu start|status|stop|list-midi|configure [controller options]' >&2;exit 2;; esac
[ "$#" -ne 0 ] || set -- "$operation"
exec "$here/mcu-session.sh" "$@"
