import re
from pprint import pprint

print(f"Reading dictionary file...")
trace_dict = {}
key = None
with open("trace_dict.txt",'r') as f_obj:
    for line in f_obj:
        if "0x" in line:
            key = re.search(r"(0x\w+)", line).group(1)
        elif line:
            trace_dict[key]=re.findall(r"\d+", line)
print("Done")

print(f"Reading trace file...")
trace_data = []
start = None
with open("opt_trace.txt",'r') as f_obj:
    temp = []
    for line in f_obj:
        result = re.search(r"In getVictim", line)
        if result:
            start = True

        result = re.search(r"Evicting block with address 0x0", line)
        if result:
            start = False
            temp = []
            continue

        if start:
            temp.append(re.search(r"\d+: system.l2.replacement_policy: (.+)", line).group(1))
            result = re.search(r"Evicting block with", line)
            if result:
                start = False
                trace_data.append(temp)
                temp = []


print("Done")

# Analysis
for instance in trace_data:
    access_counter = None
    stored_blks = []
    trace_victim = None
    # Grab data
    for line in instance:
        result = re.search(r"Access counter: (\d+)", line)
        if result:
            access_counter = int(result.group(1))
        
        result = re.search(r"Looking at candidate with address (\w+)", line)
        if result:
            stored_blks.append(result.group(1))

        result = re.search(r"Evicting block with address (\w+)", line)
        if result:
            trace_victim = result.group(1)

    # Verify
    max_index = 0
    victim = None
    for blk in stored_blks:
        temp_dict = [eval(i) for i in trace_dict[blk]]  # List of int
        print(f"{blk}: {temp_dict}")
        print(f"{access_counter}")
        # Look for if stored block will never be used again
        if temp_dict[-1] < access_counter:
            victim = blk
            print(f"{blk} Found blk that never be used again")
            break

        for access_index in temp_dict:
            if (access_index > access_counter) and (access_index > max_index):
                max_index = access_index
                victim = blk
                break
    if victim != trace_victim:
        print(f"victim {victim} vs trace_victim {trace_victim}")
    print("-----------------------------------")
