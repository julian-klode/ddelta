#!/bin/sh
$(dirname $0)/build-all-deltas.py "$@" | (sum=0; while read _ old new delta; do test -e $delta || continue; (echo $new | cut -f1 -d_ | sed s#.*/##  ; wc -c < $new; wc -c < $delta ) | xargs; done)

