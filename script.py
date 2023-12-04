import time
import re
from pprint import pprint
import subprocess
import argparse

start = time.time()
benchmarks = ['perlbench_s', 'gcc_s', 'bwaves_s','mcf_s', 'cactuBSSN_s', 'deepsjeng_s', 'lbm_s', 
              'omnetpp_s', 'xalancbmk_s', 'specrand_is', 'specrand_fs', 'cam4_s', 'pop2_s', 
              'imagick_s', 'nab_s', 'fotonik3d_s', 'roms_s', 'exchange2_s', 'xz_s', 
              'perlbench', 'bzip2', 'gcc', 'bwaves', 'gamess', 'mcf', 'milc', 'zeusmp', 'gromacs', 
              'cactusADM', 'leslie3d', 'namd', 'gobmk', 'dealII', 'soplex', 'povray', 'calculix', 
              'hmmer', 'sjeng', 'GemsFDTD', 'libquantum', 'h264ref', 'tonto', 'lbm', 'omnetpp', 
              'astar', 'wrf', 'sphinx3', 'xalancbmk', 'specrand_i' ,'specrand_f']

benchmarks_2k17 = ["bwaves_s", "cactuBSSN_s", "lbm_s", "cam4_s", "pop2_s", "imagick_s", "nab_s",
                   "fotonik3d_s", "specrand_fs", "perlbench_s", "gcc_s", "mcf_s", "omnetpp_s",
                   "xalancbmk_s", "deepsjeng_s", "exchange2_s", "xz_s", "specrand_is"]
data = {}

parser = argparse.ArgumentParser(
                    prog='Benchmark Runner',
                    description='Runs and compiles data from benchmarks')

parser.add_argument('-c', '--cpu_type', default='X86O3CPU')
parser.add_argument('-b','--benchmark')
parser.add_argument('-s2', '--l2_size', default='256kB')
parser.add_argument('-a2', '--l2_assoc', default='4')
parser.add_argument('-s17', action='store_true', default=False)
args = parser.parse_args()

if args.benchmark:
    if args.benchmark in benchmarks:
        benchmarks = [args.benchmark]
    else:
        print(f"{args.benchmark} is not one of the benchmarks available to run.")
        exit(1)

if args.s17:
    benchmarks = benchmarks_2k17

for benchmark in benchmarks:
    print(f"Running {benchmark}...")
    with open(f"{args.l2_size}_{args.l2_assoc}/{benchmark}_trace.txt",'w') as f_obj:
        proc = subprocess.Popen(["./build/ECE565-X86/gem5.opt", "--debug-flags=MemoryAddr", 
                                 "configs/spec/spec_se.py", "-b", benchmark,  f"--cpu-type={args.cpu_type}", "--maxinsts=5000000", 
                                 "--l1d_size=16kB", "--l1i_size=16kB", "--l1d_assoc=2","--l1i_assoc=2",
                                 "--caches", "--l2cache", f"--l2_size={args.l2_size}", f"--l2_assoc={args.l2_assoc}"],
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        proc.communicate()

        if proc.returncode:
            print(f"Failed to run {benchmark}")

    print("Reading stats from the benchmark...")
    with open('m5out/stats.txt', 'r') as f:
        contents = f.read()

    print("Processing stats from the benchmark...")
    temp = {}
    temp["CPI"] = re.search(r'cpi *(\S+)', contents).group(1)
    data[benchmark] = temp


pprint(data)
end = time.time()
print(f"Total runtime: {end - start}")
