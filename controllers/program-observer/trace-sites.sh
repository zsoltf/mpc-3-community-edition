#!/bin/sh
# Regenerate the trace-fault site map: __COUNTER__ value -> source file and line.
# The observer packs that value into admission_detail when it latches a trace
# fault, so a captured admission_detail names the exact call site.
# Usage: trace-sites.sh > trace-sites.txt   (run after any edit that adds,
# removes or reorders a trace_fail call, because __COUNTER__ is positional).
set -eu
cd "$(dirname "$0")"
[ -f package/command/config.h ] || { echo 'build the package first: config.h missing' >&2; exit 1; }
docker run --rm --network none -v "$PWD/..:/controllers:ro" -v "$PWD:/src:ro" \
  -v "$PWD/../stop-route.h:/stop-route.h:ro" mpclearn-controls-build sh -ec '
arm-linux-gnueabihf-gcc -std=c11 -E -DVOLUME_MIRROR -DMIRROR_COMMAND -fPIC \
  -I/src/package/command /src/command-capture.c
' | python3 -c "
import sys,re
f='?';ln=0;out=[]
lm=re.compile(r'^# (\d+) \"([^\"]+)\"')
cm=re.compile(r'trace_fail_at\(\s*\(?\s*(\w+)\s*\)?\s*,\s*(\d+)u?\s*\)')
for line in sys.stdin:
    m=lm.match(line)
    if m: ln=int(m.group(1)); f=m.group(2); continue
    for c in cm.finditer(line):
        out.append((int(c.group(2)), f.split('/')[-1], ln, c.group(1)))
    ln+=1
seen=set()
for site,f,l,why in sorted(out):
    if site in seen: continue
    seen.add(site)
    print('%-4d %s:%d  %s' % (site,f,l,why))
print('# %d sites' % len(seen), file=sys.stderr)
"
