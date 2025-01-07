#!/bin/bash

# Combine the statistic and records files from each benchmark run into a single file

# Define the input files and the output file
input_statistic_files=($(find test/benchmark/report -type f -name '*ilogtail_statistic.json'))
output_statistic_file="test/benchmark/report/ilogtail_statistic_all.json"

# Start the output file with an opening square bracket
rm -f "$output_statistic_file"
touch "$output_statistic_file"
echo '[' > "$output_statistic_file"

# Loop through each input file
for i in "${!input_statistic_files[@]}"; do
  # Read the file, remove the first and last line (the square brackets), and append to the output file
  cat "${input_statistic_files[$i]}" | sed '1d;$d' >> "$output_statistic_file"
  
  # If this is not the last file, append a comma to separate the arrays
  if [ $i -lt $((${#input_statistic_files[@]} - 1)) ]; then
    echo ',' >> "$output_statistic_file"
  fi
done

# Finish the output file with a closing square bracket
echo ']' >> "$output_statistic_file"

# Define the input files and the output file
input_records_files=($(find test/benchmark/report -type f -name '*records.json'))
output_records_file="test/benchmark/report/records_all.json"

# Start the output file with an opening square bracket
rm -f "$output_records_file"
touch "$output_records_file"
echo '[' > "$output_records_file"

# Loop through each input file
for i in "${!input_records_files[@]}"; do
  # Read the file, remove the first and last line (the square brackets), and append to the output file
  cat "${input_records_files[$i]}" | sed '1d;$d' >> "$output_records_file"
  
  # If this is not the last file, append a comma to separate the arrays
  if [ $i -lt $((${#input_records_files[@]} - 1)) ]; then
    echo ',' >> "$output_records_file"
  fi
done

# Finish the output file with a closing square bracket
echo ']' >> "$output_records_file"
