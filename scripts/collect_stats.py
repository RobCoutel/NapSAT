import os
from subprocess import PIPE, Popen
import threading
import sys
import time
import pandas as pd
from rich.progress import Progress

# Path to the executable
SAT_exec = "./build/NapSAT"
additional_options: list[str] = [
    "-del off",
    "--restarts off"
]

N_THREADS = os.cpu_count() - 1

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

    args = [SAT_exec, filename] + option.split(" ") + additional_options + ["-stat"]
    output = Popen(args, shell=False, stdout=PIPE, stderr=PIPE)
    # get the return status

    out, error = output.communicate()
    return_code = output.returncode
    if return_code != 0:
        print(f"Error in {filename} with option {option}: Return code {return_code}")
        return
    out_dec = out.decode("utf-8")
    err = error.decode("utf-8")
    if err:
        print(f"Error in {filename}: {err.strip()}")
    stats = {}
    if out:
        stats = parse_output(out_dec)
        stats["option"] = option
        stats["file"] = filename.split("/")[-1]
    else:
        print(f"No output in {filename} with option {option}")
        return

    try:
        mutex.acquire()
        # check if the stats already exist in the dataframe
        for stat_name, value in stats.items():
            if stat_name not in df.columns:
                df[stat_name] = 0 if isinstance(value, int) else ""
        if ("Done" not in stats):
            print(f"Warning: No 'Done' stat in {filename} with option {option}")
        # the index +1 cannot be in the df yet
        assert(len(df) == 0 or df.index.min() >= 0)
        df.loc[len(df)] = stats
        # df.index = df.index + 1
        mutex.release()
    except Exception as e:
        print(f"Error in {filename} with option {option}: {e}")
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
    SAT_options = ["-ncb"] + sys.argv[3:]
    if "-gb" in SAT_options:
        SAT_options.append("-gb -lcm")
        SAT_options.append("-gb -bl")
        SAT_options.append("-gb -lcm -bl")

    opts_copy = SAT_options.copy()
    for option in opts_copy:
        SAT_options.append(option + " -pcr")
        SAT_options.append(option + " -ecr")


    print(f"Collecting stats for {directory} with options {SAT_options}")

    # check if the saving file already exists, if so, load it
    if os.path.exists(out_file):
        print(f"Loading existing stats from {out_file}")
        df = pd.read_csv(out_file)
        # get the list of already processed files and options
        processed = set((row["file"], row["option"]) for _, row in df.iterrows())
        # remove the already processed files from the list of jobs
        SAT_options = [option for option in SAT_options if (option.split(" ")[0], option) not in processed]
        print(f"Continuing with {len(SAT_options)} options")
    else:
        print(f"No existing stats found, starting from scratch")
        df = pd.DataFrame(columns=["file", "option", "Time (ms)"])  # Adjust the number of stats as needed
        processed = set()

    # a job is a tuple of (filename, option)
    jobs: list[tuple[str, str]] = []
    total_jobs = 0
    # recursively search the directory for .cnf files, create a list of command lines, one for each file, option pair
    for root, dirs, files in os.walk(directory):
        for file in files:
            if file.endswith(".cnf"):
                for option in SAT_options:
                    # only add the job if it has not been processed yet
                    total_jobs += 1
                    if (file, option) not in processed:
                        jobs.append((f"{root}/{file}", option))

    # sort the jobs by the number of variables in the file, and by file name
    jobs.sort(key=lambda x: (int(x[0].split("-")[-1].split(".")[0]), x[0]))

    thread_pool: list[threading.Thread] = []
    # create a thread for each job, but limit the number of threads to N_THREADS
    start = time.time()

    instances_solved = 0
    instances_saved = 0

    with Progress() as progress:
        task_solved = progress.add_task("Solved | 0/{total_jobs}", total=total_jobs, color="red")
        task_saved = progress.add_task("Saved | 0/{total_jobs}", total=total_jobs, color="blue")
        for i in range(len(processed)):
            progress.advance(task_solved)
            progress.advance(task_saved)
            instances_saved += 1
            instances_solved += 1
        progress.update(task_solved, description=f"Solved | {instances_solved}/{total_jobs}")
        progress.update(task_saved, description=f"Saved | {instances_saved}/{total_jobs}")

        while len(jobs) > 0 or len(thread_pool) > 0:
            for thread in thread_pool:
                if not thread.is_alive():
                    thread_pool.remove(thread)
                    instances_solved += 1
                    # progress bar
                    progress.advance(task_solved)
                    progress.update(task_solved, description=f"Solved | {instances_solved}/{total_jobs}")

            while len(thread_pool) < N_THREADS and len(jobs) > 0:
                filename, option = jobs.pop(0)
                thread = threading.Thread(target=run_one_job, args=(filename, option, df))
                thread.start()
                thread_pool.append(thread)

            mutex.acquire()
            # compute the average time of the completed jobs
            avg_time = 0
            if len(df) > 0:
                avg_time = df["Time (ms)"].mean()

            if instances_solved - instances_saved >= 50:
                # sort the dataframe by file name and option
                assert len(df) >= instances_solved
                assert len(df) < instances_solved + N_THREADS
                # save the dataframe to a csv file
                df.sort_values(by=["file", "option"]).to_csv(out_file, index=False)
                instances_saved = instances_solved
                progress.update(task_saved, completed=instances_saved)
                progress.update(task_saved, description=f"Saved | {instances_saved}/{total_jobs}")
            mutex.release()

            # sleep for avg_time / 100 (expressed in seconds, but the stat is in milliseconds)
            time.sleep(avg_time / 10**5 if avg_time > 0 else 0.00001)


    # drop the unnecessary columns from the dataframe
    df = df.drop(columns=["Variable added", "Backtracking started", "Invariants checked", "Allocated Chunk", "Purging clauses", "Binary clause simplified", "Literal removed from clause"], errors="ignore")

    # sort the dataframe by file name and option
    df = df.sort_values(by=["file", "option"])
    # save the dataframe to a csv file
    df.to_csv(out_file, index=False)

    end = time.time()
    print(f"\nFinished collecting stats in {end - start:.2f} seconds.")
    print(f"Instances solved: {instances_solved}")
