import os
from subprocess import PIPE, Popen
import threading
import sys
import time
import pandas as pd

# Path to the executable
SAT_exec = "./build/NapSAT"
additional_options = "-stat"

N_THREADS = 14

def parse_output(output:str) -> dict[str, int|str]:
    """
    Parses the output of the SAT solver and returns a dictionary with statistics.

    Parameters:
        output (str): The output of the SAT solver

    Returns:
        dict[str, int]: A dictionary with statistics
    """
    stats: dict[str, int|str] = {}
    lines = output.split("\n")
    for line in lines:
        line = line.replace("Stat : ", "")
        if line.startswith("c  - "):
            stat_name = line[4:].split(":")[0].strip()
            value = line.split(":")[1].strip().replace(",", "")
            if value.isdigit():
                stats[stat_name] = int(value)
            else:
                stats[stat_name] = value
    return stats

mutex = threading.Lock()

def run_one_job(filename : str, option: str, df: pd.DataFrame):
    '''
    Runs the SAT solver on the given file and searches for the given pattern in the output.
    If the pattern is found, the filename is printed.
    If some text is printed on stderr, the filename is printed.

    Parameters:
        filename (str): The name of the file to run the SAT solver on
        pattern (str): The pattern to search for in the output

    Returns:
        None
    '''
    args = [SAT_exec, filename] + [option] + ["-stat"]
    output = Popen(args, shell=False, stdout=PIPE, stderr=PIPE)
    out, error = output.communicate()
    out_dec = out.decode("utf-8")
    err = error.decode("utf-8")
    if err:
        print(f"Error in {filename}: {err.strip()}")
    stats = {}
    if out:
        stats = parse_output(out_dec)
        stats["option"] = option
        stats["file"] = filename.split("/")[-1]
    # run the problem a second time, without the -stat option, to get the time
    args = [SAT_exec, filename] + [option]
    output = Popen(args, shell=False, stdout=PIPE, stderr=PIPE)
    out, error = output.communicate()
    out_dec = out.decode("utf-8")
    err = error.decode("utf-8")
    if err:
        print(f"Error in {filename} (without -stat): {err.strip()}")
    if out:
        # parse the time from the output
        for line in out_dec.split("\n"):
            if line.startswith("c Time:"):
                time = line.split(":")[1].strip()
                stats["time"] = time
                break

    mutex.acquire()
    # check if the stats already exist in the dataframe
    for stat_name, value in stats.items():
        if stat_name not in df.columns:
            df[stat_name] = 0 if isinstance(value, int) else ""
    df.loc[-1] = stats
    df.index = df.index + 1
    mutex.release()

if __name__ == "__main__":
    # get the arguments from the command line
    # the first argument is the directory to search in
    # the second argument is the file where the stats will be saved
    if len(sys.argv) < 4:
        print("Usage: python collect_stats.py <directory> <out-file> [option1] [option2], ...")
        sys.exit(1)
    directory = sys.argv[1]
    out_file = sys.argv[2]
    SAT_options = [""] + sys.argv[3:]
    print(f"Collecting stats for {directory} with options {SAT_options}")

    # a job is a tuple of (filename, option)
    jobs: list[tuple[str, str]] = []
    # recursively search the directory for .cnf files, create a list of command lines, one for each file, option pair
    for root, dirs, files in os.walk(directory):
        for file in files:
            if file.endswith(".cnf"):
                for option in SAT_options:
                    jobs.append((f"{root}/{file}", option))

    # sort the jobs by the number of variables in the file, and by file name
    jobs.sort(key=lambda x: (int(x[0].split("uf")[1].split("-")[0]), x[0]))

    df = pd.DataFrame(columns=["file", "option", "Time"])  # Adjust the number of stats as needed

    thread_pool: list[threading.Thread] = []
    # create a thread for each job, but limit the number of threads to N_THREADS
    n_completed = 0
    start = time.time()
    while len(jobs) > 0:
        if len(thread_pool) < N_THREADS:
            filename, option = jobs.pop(0)
            thread = threading.Thread(target=run_one_job, args=(filename, option, df))
            thread.start()
            thread_pool.append(thread)
        for thread in thread_pool:
            if not thread.is_alive():
                thread_pool.remove(thread)
                n_completed += 1
        print(f"Progress: {n_completed} + ({len(thread_pool)})/{len(jobs) + n_completed + len(thread_pool)}", end="\r")
    # wait for all threads to finish
    for thread in thread_pool:
        thread.join()

    # save the dataframe to a csv file
    df.to_csv(out_file, index=False)

    end = time.time()
    print(f"\nFinished collecting stats in {end - start:.2f} seconds.")
