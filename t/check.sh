#!/bin/sh
# Copyright 2016 Gauthier Voron <gauthier.voron@lip6.fr>
# This file is part of pin.
#
# Pin is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# Pin is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with pin.  If not, see <http://www.gnu.org/licenses/>.

LIB="$1"
if [ "x$2" != "x" ] ; then
    BIN="$2"
else
    BIN=./
fi


compare()
{
    got="$1" ; shift
    exp="$1" ; shift

    lines=`cat "$got" | wc -l`
    if [ $lines -ne `cat "$exp" | wc -l` ] ; then
	return 1
    fi

    for line in `seq 1 $lines` ; do
	lgot=`head -n $line "$got" | tail -n 1`
	lexp=`head -n $line "$exp" | tail -n 1`

	if [ $(( 0x$lgot & ~0x$lexp )) -ne 0 ] ; then
	    return 1
	fi
    done

    return 0
}

check_config()
{
    out=`mktemp`
    cor=`mktemp`
    name="$1"
    args="$2"
    rr="$3"
    map="$4"
    exp="$5"

    set -m
    (
	if [ "x$rr" != "x" ] ; then
	    export PIN_RR="$rr"
	fi
	if [ "x$map" != "x" ] ; then
	    export PIN_MAP="$map"
	fi
	export LD_PRELOAD="$LIB"

	"$BIN/pthread" $args  > "$out"
    ) &
    pid=$!
    set +m

    sleep 11
    if ps $pid >/dev/null ; then
	kill -TERM -$pid
	echo "timeout config $name" >&2
    fi

    for elm in $exp ; do
	echo $elm
    done > "$cor"

    if ! compare "$out" "$cor" ; then
	echo "failed config $name"
	echo "-- expected"
	cat "$cor"
	echo "-- but got"
	cat "$out"
	echo "--"
    fi >&2

    rm "$out" "$cor"
}


#            Test name        args         PIN_RR    PIN_MAP    expected
check_config "main 0"         0            0         ""         1
check_config "main 1"         0            1         ""         2
check_config "multi single"   "0 0 0 0"    0         ""         "1 1 1 1"
check_config "multi multi"    "0 0 0 0"    "0 1"     ""         "1 2 1 2"
check_config "multi choice"   "0 0 0 0 0"  "0,1 2,3" ""         "3 c 3 c 3"
check_config "multi range"    "0 0 0 0 0"  "0-2 3"   ""         "7 8 7 8 7"
check_config "nomap"          "1 2"        ""        ""         "1 2"
check_config "map"            "1 2"        ""        "0=2 1=3"  "1 2"
check_config "pinmap"         "0 0"        "2 3"     "0=2 1=3"  "1 2"
