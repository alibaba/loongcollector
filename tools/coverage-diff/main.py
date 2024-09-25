import argparse
import subprocess
import sys
import json
import re

ERROR_COLOR = '\033[31m'
RESET_COLOR = '\033[0m'

def get_changed_files():
    try:
        result = subprocess.check_output(['git', 'diff', '--unified=0', 'HEAD^1' ,'HEAD'], universal_newlines=True)
        return result
    except subprocess.CalledProcessError as e:
        print(f'An error occurred while running git command: {e}')
        return []

def parse_diff(diff_output):
    changes = {}

    current_file = None
    for line in diff_output.split('\n'):
        # 识别文件名
        file_match = re.match(r'^diff --git a/(.*) b/(.*)$', line)
        if file_match:
            current_file = file_match.group(2)
            changes[current_file] = []
            continue

        # 识别文件中的行变化
        hunk_match = re.match(r'^@@ -\d+(,\d+)? \+(\d+)(,(\d+))? @@', line)
        if hunk_match and current_file:
            start_line = int(hunk_match.group(2))
            line_count = int(hunk_match.group(4) if hunk_match.group(4) else 1)
            for i in range(start_line, start_line + line_count):
                changes[current_file].append(i)

    return changes

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='A simple argparse example')
    parser.add_argument('--path', type=str, help='The path of coverage file')
    parser.add_argument('--summary_path', type=str, help='The path of coverage file')
    args = parser.parse_args()
    changed_files = get_changed_files()
    changed_lines = parse_diff(changed_files)

    with open(args.summary_path, 'r') as file:
        summary = json.load(file)
    print('='*20)
    print('Total coverage rate: ', summary['line_percent'], '%')
    print('='*20)

    with open(args.path, 'r') as file:
        coverage = json.load(file)
    not_satified = {}
    not_satified_count = 0
    satified_count = 0

    for file in coverage['files']:
        if 'core/' + file['file'] in changed_lines:
            file_name = 'core/' + file['file']
            cur_satified = []
            cur_not_satified = []
            i = 0
            j = 0
            while i < len(file['lines']) and j < len(changed_lines[file_name]):
                if file['lines'][i]['line_number'] == changed_lines[file_name][j]:
                    if file['lines'][i]['count'] == 0:
                        cur_not_satified.append(file['lines'][i]['line_number'])
                    else:
                        cur_satified.append(file['lines'][i]['line_number'])
                    i += 1
                    j += 1
                elif file['lines'][i]['line_number'] < changed_lines[file_name][j]:
                    i += 1
                else:
                    j += 1
            if len(cur_satified) > 0 or len(cur_not_satified) > 0:
                print('file: ', file_name)
                if len(cur_satified) > 0:
                    print('covered lines: ', cur_satified)
                    satified_count += len(cur_satified)
                if len(cur_not_satified) > 0:
                    print(f'{ERROR_COLOR}not covered lines:{RESET_COLOR} ', cur_not_satified)
                    not_satified_count += len(cur_not_satified)
                print('')
            if len(cur_not_satified) > 0:
                not_satified[file_name] = cur_not_satified
    
    if not_satified_count + satified_count == 0:
        print('No line to cover', flush=True)
        sys.exit(0)

    coverage_rate = ((satified_count) / (not_satified_count + satified_count) ) * 100
    print('='*20)
    if coverage_rate < 50:
        print(f'{ERROR_COLOR}Diff coverage rate is less than 50%: {coverage_rate:.1f}%{RESET_COLOR}', flush=True)
        print('='*20)
        sys.exit(1)
    else:
        print(f'Diff coverage rate is {coverage_rate:.1f}%', flush=True)
        print('='*20)
        sys.exit(0)
