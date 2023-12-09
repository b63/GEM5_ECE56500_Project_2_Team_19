import re
import os


dir = "512kB_16"

for filename in os.listdir(dir):
    output_array = []
    print(f"Reading {filename}...")
    with open(f"{dir}/{filename}",'r') as f_obj:
        for line in f_obj:
            result = re.search(r"system.l2: Cache responding to (0x\w+)", line)
            if result:
                output_array.append(result.group(1))
    print("Done")

    if not os.path.exists(f"simplified_{dir}"):
        os.makedirs(f"simplified_{dir}")

    print(f"Creating simplified version of {filename}...")
    with open(f"simplified_{dir}/{filename}",'w') as f_obj:
       for line in output_array:
           f_obj.write(f"{line}\n")
    print("Done")
