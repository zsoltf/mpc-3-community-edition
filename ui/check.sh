#!/bin/sh
set -eu
cd "$(dirname "$0")"
python3 -B tests/public-surface.py
docker run --rm --network none --memory 96m --pids-limit 32 \
  -v "$PWD/package:/package:ro" mpclearn-controls-build sh -ec '
    loader=/usr/arm-linux-gnueabihf/lib/ld-linux-armhf.so.3
    "$loader" --library-path /usr/arm-linux-gnueabihf/lib /package/ui-component
    grep -Fq "mpc_ui_screen_create" /package/libmpclearn-ui-symbols.txt
    grep -Fq "mpc_ui_label_create" /package/libmpclearn-ui-symbols.txt
    grep -Fq "mpc_ui_slider_create" /package/libmpclearn-ui-symbols.txt
    grep -Fq "mpc_ui_slider_drag_ended" /package/libmpclearn-ui-symbols.txt
    grep -Fq "mpc_ui_app_register" /package/libmpclearn-ui-symbols.txt
    grep -Fq "mpc_ui_app_request_close" /package/libmpclearn-ui-symbols.txt
    grep -Fq "mpc_ui_private_app_host_destroy" /package/libmpclearn-ui-symbols.txt
    grep -Fq "mpc_ui_private_app_host_suspend" /package/libmpclearn-ui-symbols.txt
    grep -Fq "mpc_ui_private_app_host_resume" /package/libmpclearn-ui-symbols.txt
    grep -Fq "mpc_ui_private_context_from_admitted" /package/libmpclearn-ui-symbols.txt
    grep -Fq "<mpc_ui_button_clicked>:" /package/ui-component-unwind.txt
    grep -Fq "<mpc_ui_button_paint>:" /package/ui-component-unwind.txt
    ! grep -Eq "RPATH|RUNPATH" /package/ui-component-dynamic.txt
  '
echo "PASS ARM toolkit archive, public example composition, unwind and dependency checks"
