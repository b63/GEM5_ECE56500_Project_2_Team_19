import os
import re
from pprint import pprint
import subprocess
import argparse

def add_if_exists(dic, key, regex, data, group=1):
    res = re.search(regex, data)
    if res and res.group(group):
        dic[key] = res.group(group)


benchmarks = ['perlbench_s', 'gcc_s', 'bwaves_s','mcf_s', 'cactuBSSN_s', 'deepsjeng_s', 'lbm_s', 
                'omnetpp_s', 'wrf_s', 'xalancbmk_s', 'specrand_is', 'specrand_fs', 'cam4_s', 'pop2_s', 
                'imagick_s', 'nab_s', 'fotonik3d_s', 'roms_s', 'x264_s', 'leela_s', 'exchange2_s', 'xz_s', 
                'perlbench', 'bzip2', 'gcc', 'bwaves', 'gamess', 'mcf', 'milc', 'zeusmp', 'gromacs', 
                'cactusADM', 'leslie3d', 'namd', 'gobmk', 'dealII', 'soplex', 'povray', 'calculix', 
                'hmmer', 'sjeng', 'GemsFDTD', 'libquantum', 'h264ref', 'tonto', 'lbm', 'omnetpp', 
                'astar', 'wrf', 'sphinx3', 'xalancbmk', 'specrand_i' ,'specrand_f']

benchmarks_2k17 = ["bwaves_s", "cactuBSSN_s", "lbm_s", "wrf_s", "cam4_s", "pop2_s", "imagick_s", "nab_s",
                    "fotonik3d_s", "rom_s", "specrand_fs", "perlbench_s", "gcc_s", "mcf_s", "omnetpp_s",
                    "xalancbmk_s", "x264_s", "deepsjeng_s", "leela_s", "exchange2_s", "xz_s", "specrand_is"
                    "bwaves_r", "cactuBSSN_r", "lbm_r"]
data = {}

parser = argparse.ArgumentParser(
                    prog='Benchmark Runner',
                    description='Runs and compiles data from benchmarks')

parser.add_argument('-c', '--cpu_type', default='X86O3CPU', help="specify CPU type")
parser.add_argument('-b','--benchmark', help="specify which single benchmark to run")
parser.add_argument('-s2', '--l2_size', default='256kB', help="specify size of L2 cache")
parser.add_argument('-a2', '--l2_assoc', default='4',           help="specify associativity for L2 cache")
parser.add_argument('-s17', action='store_true', default=False, help="use 2k17 benchmark suite")
parser.add_argument('-v', '--verbose', action='store_true', default=False,   help="verbose, prints commands that are executed")

parser.add_argument('-sc','--shepherdcache', action='store_true', help="use Shepherd Cache for L2 cache")

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

    command = [ "./build/X86/gem5.opt", "configs/spec/spec_se.py"]
    command += ["-b", benchmark]
    command += [f"--cpu-type={args.cpu_type}", "--maxinsts=1000000"]
    command += ["--l1d_size=64kB", "--l1i_size=16kB"] 
    command += ["--caches", "--l2cache", f"--l2_size={args.l2_size}", f"--l2_assoc={args.l2_assoc}"]

    if args.shepherdcache:
        command += ["--shepherdcache"]

    if args.verbose:
        print(f"> {' '.join(command)}")

    proc = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, encoding="utf8")
    pstout, pstderr = proc.communicate()

    if proc.returncode:
        print(f"Failed to run {benchmark} return code {proc.returncode}")
        print(pstderr)

    print("Reading stats from the benchmark...")
    with open('m5out/stats.txt', 'r') as f:
        contents = f.read()

    print("Processing stats from the benchmark...")

    
    temp = {}
    add_if_exists(temp, "CPI", r'cpi *(\S+)', contents)
    add_if_exists(temp, "Total L2 Misses", r'system.l2.overallMisses::total *(\S+)', contents)
    add_if_exists(temp, "Total L2 Accesses", r'system.l2.overallAccesses::total *(\S+)', contents)
    data[benchmark] = temp


pprint(data)









