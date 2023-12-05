import os
import re
from pprint import pprint
import subprocess
import argparse

def add_if_exists(dic, key, regex, data, group=1):
    res = re.search(regex, data)
    if res and res.group(group):
        dic[key] = res.group(group)
        return dic[key]
    return None

def convert_size_to_bytes(size_str):
    units = {
        "B": 1,
        "KB": 1024,
        "MB": 1024**2,
        "GB": 1024**3,
        "TB": 1024**4
    }

    res = re.search(r'(\d+)([A-Za-z]+)', size_str)
    number = int(res.group(1))
    unit = res.group(2)

    return number * units[unit.upper()]

def convert_size_to_mb(bytes, unit = "KB"):
    units = {
        "B": 1,
        "KB": 1024,
        "MB": 1024**2,
        "GB": 1024**3,
        "TB": 1024**4
    }

    x = bytes/units[unit]
    return f"{x:0.1f}{unit}"

def write_to_file(data, name):
    benchmarks = set()
    headers    = set()
    for k, v in data.items():
        benchmarks.add(k)
        headers.update(v.keys())

    f = open(name, 'w')
    
    hindex = dict()
    hrow = ['Benchmark']
    for i, h in enumerate(headers):
        hindex[h] = i
        hrow.append(h)
    
    print(",".join(hrow),file=f)

    for k, v in data.items():
        row = ["" for _ in range(len(headers))]

        for k2,v2 in v.items():
            row[hindex[k2]] = v2
        s = ','.join(row)

        print(f"{k}, {s}", file=f)

    f.close()


def run(run_args=None):
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

    parser = argparse.ArgumentParser(
                        prog='Benchmark Runner',
                        description='Runs and compiles data from benchmarks')

    parser.add_argument('-c', '--cpu_type', default='X86O3CPU', help="specify CPU type")
    parser.add_argument('-b','--benchmark', help="specify which comma-separated benchmark(s) to run")
    parser.add_argument('-s2', '--l2_size', default='256kB', help="specify size of L2 cache")
    parser.add_argument('-a2', '--l2_assoc', default='4',    help="specify associativity for L2 cache")
    parser.add_argument('-asc2', '--l2_sc_assoc', default='2', help="specify associativity L2 Shepherd Cache")
    parser.add_argument('-s17', action='store_true', default=False, help="use 2k17 benchmark suite")
    parser.add_argument('-v', '--verbose', action='store_true', default=False,   help="verbose, prints commands that are executed")
    parser.add_argument('-o', '--output', default='', help="write CSV formatted data to this file")

    parser.add_argument('-sc','--shepherdcache', action='store_true', help="use Shepherd Cache for L2 cache")

    if run_args:
        args = parser.parse_args(args = run_args)
    else:
        args = parser.parse_args()

    if args.benchmark:
        _benchmarks = args.benchmark.split(',')
        for b in _benchmarks:
            if b not in benchmarks:
                print(f"{b} is not one of the benchmarks available to run.")
                exit(1)
        benchmarks = _benchmarks

    if args.s17:
        benchmarks = benchmarks_2k17

    data = {}
    for benchmark in benchmarks:
        print(f"Running {benchmark}...")    

        command = [ "./build/ECE565-X86/gem5.opt", "configs/spec/spec_se.py"]
        command += ["-b", benchmark]
        command += [f"--cpu-type={args.cpu_type}", "--maxinsts=1000000"]
        command += ["--l1d_size=64kB", "--l1i_size=16kB"] 

        entry_size = 64
        cache_size = convert_size_to_bytes(args.l2_size)
        num_blocks = cache_size / entry_size
        ways = int(args.l2_assoc)

        if num_blocks % ways != 0:
            cache_size_req = ((num_blocks + ways-1)//ways) * ways * entry_size
            inc_f = (cache_size_req - cache_size)/cache_size * 100
            print(f"NOTE: increasing cache size from {convert_size_to_mb(cache_size)} to {convert_size_to_mb(cache_size_req)}, +{inc_f:0.2f}% increase")
            cache_size = cache_size_req

        command += ["--caches", "--l2cache", f"--l2_size={cache_size:.0f}", f"--l2_assoc={ways}"]

        if args.shepherdcache:
            command += ["--shepherdcache", f"--l2_sc_assoc={args.l2_sc_assoc}"]

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
        add_if_exists(temp, "L2 Misses", r'system.l2.overallMisses::total *(\S+)', contents)
        add_if_exists(temp, "L2 Accesses", r'system.l2.overallAccesses::total *(\S+)', contents)
        add_if_exists(temp, "L2 Fallback replacements", r'system.l2.tags.fallbackReplRefs  *(\S+)', contents)
        add_if_exists(temp, "L2 OPT replacements", r'system.l2.tags.optReplRefs  *(\S+)', contents)
        add_if_exists(temp, "L2 Empty replacements", r'system.l2.tags.emptyReplRefs  *(\S+)', contents)
        add_if_exists(temp, "L2 replacements", r'system.l2.tags.victimReplRefs *(\S+)', contents)
        add_if_exists(temp, "L2 valid replacements", r'system.l2.replacements *(\S+)', contents)
        data[benchmark] = temp
        pprint({f'{benchmark}': temp})

    if args.output:
        write_to_file(data, args.output)
    else:
        pprint(data)
    
    return data

def _main_():
    data = run()


if __name__ == "__main__":
    _main_()







