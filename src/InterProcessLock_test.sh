#!/bin/bash

set -u

# Stress test for interprocess lock
testprog=$1

numprocs=1000
outdir=InterProcessLock_test.dir

mkdir $outdir || exit 1
trap "rm -Rf $outdir" EXIT

# Launch a bunch of processes as fast as possible
for ((i=0;i<$numprocs;++i)) ; do
    $testprog $outdir/$i &
done

wait

num_locks_acquired=$(ls $outdir/*.acquired | wc -l)
num_locks_blocked=$(ls $outdir/*.blocked | wc -l)

echo "Acquired locks: $num_locks_acquired"
echo "Blocked locks: $num_locks_blocked"

if [[ $num_locks_acquired != 1 ]] ; then
    echo "Exactly one process should have got the lock"
    exit 1
fi

if [[ $num_locks_blocked != $(($numprocs-1)) ]] ; then
    echo "Some jobs which should have failed didn't"
    exit 1
fi


# Note - the above doeesn't seem like a reliable test on windows, but it's
# unclear at this stage whether this is just bash being flaky or an actual bug
# in the lock implementation :-(  Doing it using the following .bat command
# seems to work.
#
# for /l %%i in (1, 1, 1000) do start /b src\InterProcessLock_test a\%%i
