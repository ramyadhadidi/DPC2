#! /usr/bin/env python
#########################################################################################
# Author: Ramyad Hadidi (rhadidi@gatech.edu)
#########################################################################################
import sys
import os
import argparse

#########################################################################################
# sanity check
#########################################################################################
def sanity_check(args):
    if not args.exe:
        print "ERROR: Define executables with -e option"
        exit(0)
	if not args.degree:
		print "ERROR: Define degree"
		exit(0)

    return

#########################################################################################
# create an argument parser
#########################################################################################
def process_options():
    parser = argparse.ArgumentParser(description='run.py')
    parser.add_argument("--dryRun", help="Print out the generated script(s) instead of launching jobs", action="store_true", default=False)
    parser.add_argument("-o", "--outputDir", help="Root directory for result files", default="results")
    parser.add_argument("-e", "--exe", action='append', nargs='*', help="Executables to run")
    parser.add_argument("-s", "--submit",  help="Submit Jobs to qsub", action="store_true", default=False)
    parser.add_argument("-d", "--degree", help="Degree of Prefetcher, for name generation", default=0)

    return parser

#########################################################################################
# main function
#########################################################################################
def main(argv):
    #parse arguments
    parser = process_options()
    args = parser.parse_args()
    sanity_check(args)
    
    current_dir = os.getcwd()

    #compile files
    source_dir = os.path.join(current_dir, 'example_prefetchers')
    sources = os.listdir (source_dir)
    for source in sources:
        output = source.split('_')[0]
        command = (
            'gcc -Wall -o dpc2sim_' + output +
            ' example_prefetchers/'+ source + ' lib/dpc2sim.a'
            )
        print command
        os.system(command)

    #make results dir if not exist
    if not os.path.exists(args.outputDir):
        os.makedirs(args.outputDir)    

    #find traces
    trace_dir = os.path.join(current_dir, 'traces' )
    traces = os.listdir (trace_dir)

    #dpc options - currently nothing
    dpc_options = ["-hide_heartbeat"]
    dpc_options = " ".join(dpc_options)

    for dpc in args.exe[0]:
        for trace in traces:
            output_filename = "{}_{}_{}".format(dpc.split('_')[1], trace.split('_')[0], args.degree)
            command = ( 
                "zcat " + os.path.join(trace_dir, trace) + " | " + 
                os.path.join(current_dir, dpc) + " " + dpc_options + " | tee " + 
                os.path.join (current_dir, args.outputDir ,output_filename)
                )
            if args.dryRun:
                print command
            else:
                #submitting to qsub
                if args.submit:
                    exit()
                #run on current machine
                else:
                    os.system(command)


#########################################################################################
# main routine
#########################################################################################
if __name__ == '__main__':
    main(sys.argv)
