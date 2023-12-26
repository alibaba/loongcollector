#!/usr/bin/env bash

# Copyright 2021 iLogtail Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

function print_msg(){
  echo "============================================"
  echo "$1"
  echo "============================================"
}

function find_go_dependencies() {
  main_path=$1
  output_file=$2
  targets="$(go tool dist list)"

  for target in ${targets}; do
  	# only check platforms we build for
  	case "${target}" in
  		linux/*) ;;
  		windows/*) ;;
  		darwin/*) ;;
  		*) continue;;
  	esac
    cd  $main_path
  	GOOS=${target%%/*} GOARCH=${target##*/} go list -deps -f '{{with .Module}}{{.Path}}@@@{{.Replace}}{{end}}' . >> "${output_file}-temp"
  	cd -
  done
  LC_ALL=C sort -u "${output_file}-temp" > "${output_file}-compress"
  cat "${output_file}-compress" | while read dep
  do
    module=`echo $dep|awk -F '@@@' '{print $1}'`
    replace=`echo $dep|awk -F '@@@' '{print $2}'|awk '{print $1}'`
       if [[ "$module" == *ilogtail* || "$replace" == ./* ]]; then
           continue
       fi
       if [[ "$replace" = '<nil>' || "$replace" != *iLogtail* ]]; then
           echo "$module" >>"${output_file}"
       else
            echo "$replace fork from $module" >>"${output_file}"
       fi
  done
   rm -f "${output_file}-temp"
   rm -f "${output_file}-compress"
}


function filter_dependencies() {
  input_file=$1
  output_file=$2
#  LC_ALL=C sort -u "${input_file}" > "${input_file}"-compress
  cat "${input_file}" | while read dep
  do
  	case "${dep}" in
  		# ignore ourselves
  		github.com/alibaba/ilogtail) continue;;
  		github.com/alibaba/ilogtail/test) continue;;
      github.com/alibabacloud-go/alibabacloud*) continue;; # ignore other alibabacloud-go dependencies
      github.com/alibabacloud-go/darabonba*) continue;; # ignore other alibabacloud-go dependencies
  		./pkg) continue;;
  		../pkg) continue;;
  		../) continue;;
  	esac
    dep="${dep%%/v[0-9]}"
    dep="${dep%%/v[0-9][0-9]}"
  	echo "${dep}" >> "${output_file}"
  done

  tmpdir="$(mktemp -d)"
  mv "$output_file" "$tmpdir"/old
  uniq "$tmpdir"/old | sort > "$output_file"
  rm -rf "$tmpdir"
#  rm -rf "${input_file}"-compress
}

function find_licenses() {
  input_file=$1
  output_file_prefix=$2

  tmpdir="$(mktemp -d)"
  cat $input_file | while read line
  do
    if [[ $line == *iLogtail* ]]; then
        forkRepo=$(echo $line|awk '{print $1}')
        url="http://$forkRepo"
    else
       url="https://pkg.go.dev/$line?tab=licenses"
    fi
    wget $url -O "$tmpdir"/LICENSE
    if [ ! -s "$tmpdir"/LICENSE ]; then
       echo "NOT_SUPPORT_SEARCH: - $line"
       echo "NOT_SUPPORT_SEARCH: - $line"  >> "$output_file_prefix-NOT_FOUND"
       continue
    fi

    line=$(echo $line | sed 's/_/\\_/g')
    if [[ $line == *iLogtail* ]]; then
      license_type=`cat "$tmpdir"/LICENSE|grep "license"|sort|uniq|grep -e ".*\s\(licenses\sfound\|license\)$"| awk '{print $1}'`
      echo "- [$line]($url) based on $license_type" >> "$output_file_prefix-fork"
    else
      license_type=`cat "$tmpdir"/LICENSE|grep "#lic-0"|cut -f 2 -d ">"|cut -f 1 -d "<"`
      echo "- [$line]($url)" >> "$output_file_prefix-$license_type"
    fi
  done
}

export ROOT_DIR=$(cd $(dirname "${BASH_SOURCE[0]}") && cd .. && pwd)
export OUTPUT_DIR=$ROOT_DIR/find_licenses

MAIN_PACKAGE=$1
LICENSE_FILE=$2

rm -rf "$OUTPUT_DIR"
mkdir "$OUTPUT_DIR"

find_go_dependencies "$ROOT_DIR"/"$MAIN_PACKAGE" "$OUTPUT_DIR"/LIST
filter_dependencies "$OUTPUT_DIR"/LIST "$OUTPUT_DIR"/HEAD && rm -rf "$OUTPUT_DIR"/LIST

grep '^-' "$ROOT_DIR"/licenses/"$LICENSE_FILE"  |cut -f 2 -d "["|cut -f 1 -d "]" | sed 's/\\_/_/g' | sort | uniq  > "$OUTPUT_DIR"/LICENSE_OF_DEPENDENCIES

cat "$OUTPUT_DIR"/LICENSE_OF_DEPENDENCIES "$OUTPUT_DIR"/HEAD "$OUTPUT_DIR"/HEAD | sort | uniq -u > "$OUTPUT_DIR"/REMOVING
cat "$OUTPUT_DIR"/HEAD  "$OUTPUT_DIR"/LICENSE_OF_DEPENDENCIES "$OUTPUT_DIR"/LICENSE_OF_DEPENDENCIES | sort | uniq -u  > "$OUTPUT_DIR"/ADDING

rm -f "$OUTPUT_DIR"/HEAD

if [ -s "$OUTPUT_DIR"/REMOVING ]; then
  print_msg "FOLLOWING DEPENDENCIES IN $LICENSE_FILE SHOULD BE REMOVED "
  cat "$OUTPUT_DIR"/REMOVING
fi

if [ -s "$OUTPUT_DIR"/ADDING ]; then
  print_msg "FOLLOWING DEPENDENCIES SHOULD BE ADDED to $LICENSE_FILE "
  cat "$OUTPUT_DIR"/ADDING
  print_msg "TRY TO SEARCH LICENSE TYPE "
  find_licenses "$OUTPUT_DIR"/ADDING "$OUTPUT_DIR"/LICENSE
  echo "THE FOUNDED TYPES WERE SAVED AS $OUTPUT_DIR/LICENSE-{type} file"
fi

if [ -s "$OUTPUT_DIR"/ADDING ] || [ -s "$OUTPUT_DIR"/REMOVING ] ; then
  exit 1
else
  print_msg "DEPENDENCIES CHECKING IS PASSED"
fi
