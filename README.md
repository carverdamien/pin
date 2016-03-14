Pin - a tool to control thread pinning
======================================

Pin is a shared object which can intercept a target program calls to pthread
and sched functions in order to control how the threads of this target are
pinned.


How to use it
-------------

Once pin.so is built, you can modify the pinning behavior of a program `foo`
by doing : `export LD_PRELOAD=pin.so ; ./foo`
These instructions load the pin.so object, but you also need to inform pin.so
of what to do.
Here are some examples :

  * `export PIN_RR="0 3 12-17,23" ; export LD_PRELOAD=pin.so ; ./foo`

This tells pin.so to pin the new threads in a round-robin fashion on cores 0,
then 3, then one of 12 to 17 or 23.

  * `export PIN_MAP="0=12 3=17 5=17" ; export LD_PRELOAD=pin.so ; ./foo`
  
This tells pin.so to intercept `sched_setaffinity()` calls to pin threads to
core 12 instead of core 0, and to core 17 instead of core 3 or 5.
