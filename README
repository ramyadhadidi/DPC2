
Welcome to the 2nd Data Prefetching Championship!

If you have not already done so, plase subscribe to the prefetching 
competition mailing list by sending an empty email to:

dpc-2-subscribe AT yahoogroups.com

You do not need to have a Yahoo email account - you only need to 
subscribe to the mailing list. When prompted, type in a dummy 
message to the moderator. 

This mailing list will be used for any announcements regarding the 
competition, any simulation infrastructure updates, and participants 
may post questions or report problems.

You must provide a single .c file for your prefetcher.  There are several
example prefetchers in the example_prefetchers directory.  Refer to them
to learn how to interface with the DPC2 Simulator.

*
* How to compile:
*

Compile your prefetcher .c file against lib/dpc2sim.a, like this:

gcc -Wall -o dpc2sim example_prefetchers/stream_prefetcher.c lib/dpc2sim.a

*
* How to run:
*

The DPC2 Simulator reads in an instruction trace from stdin in the form
of binary data, so you must use the cat command and pipe it into the 
input of the simulator, like this:

cat trace.dpc | ./dpc2sim

The included traces have been zipped.  You can use zcat to feed these
traces into the simulator without unzipping them beforehand:

zcat trace.dpc.gz | ./dpc2sim

There are several command line switches that you can use to configure the
DPC2 Simulator.

-small_llc
This changes the size of the Last Level Cache to 256 KB.  The default
size of the LLC is 1 MB.

-low_bandwidth
This changes the DRAM bandwidth of the system to 3.2 GB/s.  The default
DRAM bandwidth is 12.8 GB/s.

-scramble_loads
This randomizes the order in which loads lookup the L1.  Note that this
randomization only occurs among loads which are ready to issue at that
moment, so the degree of randomization the L2 sees is usually small.
Default is to NOT use scrambled loads.

-warmup_instructions <number>
Use this to specify the length of the warmup period.  After the warmup
period is over, the IPC statistics are reset, and the final reported
IPC metric will be calculated starting at this point.
Default value is 10,000,000.

-simulation_instructions <number>
Use this to specify how many instructions you want to execute after the
warmup period is over.  After the simulation period is over, the simulator
will exit and IPC since the warmup period will be printed.
Default value is 100,000,000.

-hide_heartbeat
Normally, a heartbeat message is printed every 100,000 instructions, which
shows the IPC since the last heartbeat message, as well as the cummulative
IPC of the program so far.  The cummulative IPC displayed by the heartbeat
function is NOT affected by the reset after the warmup period.

For the championship, your prefetcher's performance will be measured in 
four configurations:

1. No command line switches set
2. -small_llc
3. -low_bandwidth
4. -scramble_loads

You can combine the switches for your testing purposes, but the championship
will only look at the four configurations mentioned above.

*
* How to create traces:
*

We have included only 8 sample traces, taken from SPEC CPU 2006. These 
traces are short, and do not necessarily cover the range of behaviors your 
prefetcher will likely see in the full competition trace list (not
included).  We STRONGLY recommend creating your own traces, covering
a wide variety of program types and behaviors.

The included Pin Tool dpc2_tracer.so can be used to generate new traces.
It was created using Pin 2.13, and may require installing libdwarf.so, 
libelf.so, or other libraries, if you do not already have them.  Please
refer to Pin documentation for working with Pin 2.13.

Use the Pin tool like this:

pin -t pintool/dpc2_tracer.so -- <your program here>

The tracer has three options you can set:

-o
Specify the output file for your trace.  The default is default_trace.dpc

-s <number>
Specify the number of instructions to skip in the program before tracing
begins.  The default value is 0.

-t <number>
The number of instructions to trace, after -s instructions have been 
skipped.  The default value is 1,000,000.

For example, you could trace 200,000 instructions of the program ls, after
skipping the first 100,000 instructions, with this command:

pin -t pintool/dpc2_tracer.so -o traces/ls_trace.dpc -s 100000 -t 200000 -- ls

Traces created with the dpc2_tracer Pin Tool are 48 bytes per instruction,
but they generally compress down to 2-10 bytes per instruction using gzip.

We generated traces from SPEC CPU 2006 by using the submit feature, which 
controls the conditions under which the benchmark program is run.  See
SPEC documentation for more details on the submit feature.
