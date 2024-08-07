import argparse
import subprocess
import sys
import time

def get_changed_files():
    try:
        # Run the git command to get the list of changed files
        result = subprocess.Popen('git diff --name-only -r HEAD^1 HEAD', shell=True,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        # Split the result by new line to get each file name
        out, err = result.communicate()
        changed_files = out.splitlines()
        result_files = []
        for file in changed_files:
            fileStr = file.decode('utf-8')
            if fileStr.startswith('core'):
                result_files.append(fileStr[5:])
        return result_files
    except subprocess.CalledProcessError as e:
        print(f"An error occurred while running git command: {e}")
        return []

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="A simple argparse example")
    parser.add_argument("path", type=str, help="The path of coverage file")
    args = parser.parse_args()
    changed_files = get_changed_files()
    line_cache = ""
    not_satified = []
    with open(args.path, 'r') as file:
        for line in file:
            if len(line_cache) > 0:
                line = line_cache + line
                line_cache = ""
            if '/' in line or ('%' in line and 'TOTAL' not in line):
                for changed_file in changed_files:
                    if line.startswith(changed_file):
                        units = line.split()
                        if len(units) < 4:
                            # some files with long filename will be split into 2 lines
                            line_cache = line
                            continue
                        coverage_rate = int(units[3][:-1])
                        if coverage_rate < 50:
                            not_satified.append(changed_file)
                        print(line, flush=True)
                        break
            else:
                print(line, flush=True)
    if len(not_satified) > 0:
        print(f"Coverage rate is less than 50% for the following files: {not_satified}", flush=True)
        sys.exit(1)
