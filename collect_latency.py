import subprocess
import time


def run_one_rps(RPS, duration=30):
    cmd = f"taskset -c 32-63 ./wrk2/wrk -D fixed -t 100 -c 100 -d {duration} -L -s ./wrk_scripts/scripts/hotel-reservation/mixed-workload_type_1.lua http://localhost:50050 -R {RPS}"
     
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
    with open("tails.txt", "a") as file:
        for line in stdout_lines:
            line = line.strip()
            for tail in ["0.500000", "0.900000", "0.950000"]:
                if tail in line:
                    file.write(line + "\n")
                    tail_lat[tail] = line.split()[0]
    if stderr:
        print(f"Error: {stderr.decode().strip()}")  # Print the standard error if any
    # for tail in ["0.500000", "0.950000", "0.990625"]:
    #     assert tail in tail_lat[path_name]
    
    time.sleep(5)
    print("RPS:", RPS, "tail_lat:", tail_lat)
    # TODO: collect the performance metrics, return key metrics
    return tail_lat

def main():
    for rps in range(500, 1500, 100):
        run_one_rps(rps, 200)

if __name__ == "__main__":
    main()