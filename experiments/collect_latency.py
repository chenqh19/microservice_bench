import subprocess
import time


def run_one_rps(RPS, duration=30, file_name="tail.txt"):
    cmd = f"taskset -c 32-63 ../wrk2/wrk -D fixed -t 200 -c 200 -d {duration} -L -s ../wrk_scripts/scripts/hotel-reservation/mixed-workload_type_1.lua http://localhost:50050 -R {RPS}"
     
    # Create subprocesses to execute each command using subprocess.Popen
    process = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = process.communicate()  # Wait for process to finish and capture its output
    if stderr:
        print("Standard Error:")
        print(stderr.decode('utf-8'))

    # Wait for all subprocesses to complete and capture their output
    tail_lat = {}
    stdout, stderr = process.communicate()  # Communicate() waits for the process to finish and returns output
    stdout_lines = stdout.decode().splitlines()
    for line in stdout_lines:
        line = line.strip()
        for tail in ["0.500000", "0.900000", "0.990625"]:
            if tail in line:
                tail_lat[tail] = line.split()[0]
    if stderr:
        print(f"Error: {stderr.decode().strip()}")  # Print the standard error if any
    # for tail in ["0.500000", "0.950000", "0.990625"]:
    #     assert tail in tail_lat[path_name]
    
    time.sleep(5)
    with open(file_name, "a") as f:
        f.write(f"RPS: {RPS}, tail_lat: {tail_lat}\n")
    return tail_lat

def main():
    time.sleep(5)
    file_name = "tail_c0s1.txt"
    with open(file_name, "a") as f:
        f.write("start experiment\n")
    for rps in range(60000, 40000, -10000): # warm up
        run_one_rps(rps, 30)
        time.sleep(5)
    time.sleep(10)
    for rps in range(40000, 63000, 500): # collect latency
        if rps < 55000 and rps % 2000 != 0:
            continue
        run_one_rps(rps, 30, file_name)
        time.sleep(5)
    with open(file_name, "a") as f:
        f.write("end experiment\n")

if __name__ == "__main__":
    main()