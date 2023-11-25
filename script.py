import os
import re
from pprint import pprint
import subprocess
import argparse


benchmarks = ['perlbench_s', 'gcc_s', 'bwaves_s','mcf_s', 'cactuBSSN_s', 'deepsjeng_s', 'lbm_s', 
'omnetpp_s', 'wrf_s', 'xalancbmk_s', 'specrand_is', 'specrand_fs', 'cam4_s', 'pop2_s', 
'imagick_s', 'nab_s', 'fotonik3d_s', 'roms_s', 'x264_s', 'leela_s', 'exchange2_s', 'xz_s', 
'perlbench', 'bzip2', 'gcc', 'bwaves', 'gamess', 'mcf', 'milc', 'zeusmp', 'gromacs', 
'cactusADM', 'leslie3d', 'namd', 'gobmk', 'dealII', 'soplex', 'povray', 'calculix', 
'hmmer', 'sjeng', 'GemsFDTD', 'libquantum', 'h264ref', 'tonto', 'lbm', 'omnetpp', 
'astar', 'wrf', 'sphinx3', 'xalancbmk', 'specrand_i' ,'specrand_f']
data = {}

parser = argparse.ArgumentParser(
                    prog='Benchmark Runner',
                    description='Runs and compiles data from benchmarks')

parser.add_argument('-c', '--cpu_type', default='X86O3CPU')
parser.add_argument('-b','--benchmark')
parser.add_argument('-s2', '--l2_size', default='256kB')
parser.add_argument('-a2', '--l2_assoc', default='4')
args = parser.parse_args()

if args.benchmark:
    if args.benchmark in benchmarks:
        benchmarks = [args.benchmark]
    else:
        print(f"{args.benchmark} is not one of the benchmarks available to run.")
        exit(1)

for benchmark in benchmarks:
    print(f"Running {benchmark}...")    
    proc = subprocess.Popen(["./build/ECE565-ARM/gem5.opt", "configs/spec/spec_se.py", "-b", benchmark, 
    	   f"--cpu-type={args.cpu_type}", "--maxinsts=1000000", "--l1d_size=64kB", 
           "--l1i_size=16kB", "--caches", "--l2cache",
           f"--l2_size={args.l2_size}", f"--l2_assoc={args.l2_assoc}"],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    proc.communicate()

    print("Reading stats from the benchmark...")
    with open('m5out/stats.txt', 'r') as f:
        contents = f.read()

    print("Processing stats from the benchmark...")
    temp = {}
    temp["CPI"] = re.search(r'cpi *(\S+)', contents).group(1)
    data[benchmark] = temp


pprint(data)









