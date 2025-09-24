#!/bin/python3
#%%
import os
from subprocess import PIPE, Popen
import concurrent.futures
from rich.progress import Progress, BarColumn, TextColumn, TimeElapsedColumn
import threading


# Path to the directory containing the sat instances
directorySat = "/home/robin/OneDrive/University/Library/SAT.CNF/sat/"
# Path to the directory containing the unsat instances
directoryUnSat = "/home/robin/OneDrive/University/Library/SAT.CNF/unsat/"
# Path to the executable
SAT_exec = "/home/robin/programs/NapSAT-dev/build/NapSAT"

SAT_options = ["", "-rscb", "-lscb", "-gb", "-gb", "-gb -lcm"]
additional_options = "-c -sw -cp"

N_THREADS = 20

def search_pattern(filename: str, pattern: str):
    '''
    Runs the SAT solver on the given file and searches for the given pattern in the output.
    If the pattern is found, the filename is printed.
    If some text is printed on stderr, the filename is printed.
    '''
    for option in SAT_options:
        args = [SAT_exec, filename] + option.split() + additional_options.split()
        output = Popen(args, shell=False, stdout=PIPE, stderr=PIPE)
        out, error = output.communicate()
        out = out.decode("utf-8")
        err = error.decode("utf-8")
        if out.find(pattern) != -1:
            print("\n" + filename + " : " + option + " : Pattern found: " + pattern)
            print(out)
        if err != "":
            print("\n" + filename + " : " + option + " : " + "ERROR")
            print("\n".join(err.split("\n")[:3]))

def search_output(root_directory, pattern):
    bench_files = {}
    for root, dirs, files in os.walk(root_directory):
        for file in files:
            if file.endswith(".cnf"):
                bench_files.setdefault(root, []).append(file)
    dir = list(bench_files.keys())
    dir.sort(key=lambda x: int(x.split("uf")[1].split("-")[0]) if "uf" in x else 0)

    lock = threading.Lock()
    tasks = {}
    started = set()

    def wrapped_search_pattern(filename, pattern, directory, folder_name):
        with lock:
            if directory not in started:
                task_id = progress.add_task(
                    "Processing", total=len(bench_files[directory]), folder=folder_name
                )
                tasks[directory] = task_id
                started.add(directory)
        search_pattern(filename, pattern)

    with Progress(
        TextColumn("[bold blue]{task.fields[folder]}", justify="right"),
        BarColumn(),
        TextColumn("[green]{task.completed}/{task.total}"),
        "[progress.percentage]{task.percentage:>3.0f}%",
        TimeElapsedColumn(),
        refresh_per_second=2,
    ) as progress, concurrent.futures.ThreadPoolExecutor(max_workers=N_THREADS) as executor:
        futures = []
        for directory in dir:
            bench_files[directory].sort()
            folder_name = os.path.basename(directory)
            for filename in bench_files[directory]:
                full_path = os.path.join(directory, filename)
                future = executor.submit(wrapped_search_pattern, full_path, pattern, directory, folder_name)
                future.directory = directory
                futures.append(future)
        for future in concurrent.futures.as_completed(futures):
            directory = future.directory
            if directory in tasks:
                progress.update(tasks[directory], advance=1)

#%%

# the first argument is the directory to search in
# the second argument is binary to execute
if __name__ == "__main__":
    # check if we have the arguments
    import sys
    directory = ""
    if len(sys.argv) > 1:
        directory = sys.argv[1]
    if len(sys.argv) > 2:
        SAT_exec = sys.argv[2]
    additional_options_bis = []
    while len(sys.argv) > 3 + len(additional_options):
        additional_options_bis.append(sys.argv[3 + len(additional_options)])
    if len(additional_options_bis) > 0:
        additional_options = " ".join(additional_options_bis)

    if directory == "":
        search_output(directoryUnSat, "s SATISFIABLE")
        search_output(directorySat, "s UNSATISFIABLE")
    else:
        search_output(directory, "s UNSATISFIABLE")
