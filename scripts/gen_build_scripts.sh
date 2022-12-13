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

set -ue
set -o pipefail

# Currently, there are 4 supported categories, which are plugin, core, all and e2e.
#
# plugin: Only build Linux dynamic lib with Golang part.
# core: Only build the CPP part.
# all: Do the above plugin and core steps.
# e2e: Build plugin dynamic lib with GOC and build the CPP part.
CATEGORY=$1
GENERATED_HOME=$2
VERSION=${3:-1.3.1}
REPOSITORY=${4:-aliyun/ilogtail}
OUT_DIR=${5:-output}

BUILD_LOGTAIL_UT=${BUILD_LOGTAIL_UT:-OFF}
ENABLE_COMPATIBLE_MODE=${ENABLE_COMPATIBLE_MODE:-OFF}
ENABLE_STATIC_LINK_CRT=${ENABLE_STATIC_LINK_CRT:-OFF}

BUILD_SCRIPT_FILE=$GENERATED_HOME/gen_build.sh
COPY_SCRIPT_FILE=$GENERATED_HOME/gen_copy_docker.sh


function generateBuildScript() {
  rm -rf $BUILD_SCRIPT_FILE
  cat > $BUILD_SCRIPT_FILE <<- EOF
#!/bin/bash
set -xue
set -o pipefail

function ramAvail () {
  local ramavail=\$(cat /proc/meminfo | grep -i 'MemAvailable' | grep -o '[[:digit:]]*')
  echo \$ramavail
}

nproc=\$(nproc)
ram_size=\$(ramAvail)
ram_limit_nproc=\$((ram_size / 1024 / 768))
[[ \$ram_limit_nproc -ge \$nproc ]] || nproc=\$ram_limit_nproc
[[ \$nproc -gt 0 ]] || nproc=1

EOF
  chmod 755 $BUILD_SCRIPT_FILE
  if [ $CATEGORY = "plugin" ]; then
    echo "mkdir -p core/build && cd core/build && cmake -DCMAKE_BUILD_TYPE=Release -DLOGTAIL_VERSION=${VERSION} .. && cd plugin && make -s PluginAdapter && cd ../../.. && ./scripts/plugin_build.sh mod c-shared ${OUT_DIR}" >> $BUILD_SCRIPT_FILE;
  elif [ $CATEGORY = "core" ]; then
    echo "mkdir -p core/build && cd core/build && cmake -DCMAKE_BUILD_TYPE=Release -DLOGTAIL_VERSION=${VERSION} -DBUILD_LOGTAIL_UT=${BUILD_LOGTAIL_UT} -DENABLE_COMPATIBLE_MODE=${ENABLE_COMPATIBLE_MODE} -DENABLE_STATIC_LINK_CRT=${ENABLE_STATIC_LINK_CRT} .. && make -sj\$nproc" >>  $BUILD_SCRIPT_FILE;
  elif [ $CATEGORY = "all" ]; then
    echo "mkdir -p core/build && cd core/build && cmake -DCMAKE_BUILD_TYPE=Release -DLOGTAIL_VERSION=${VERSION} -DBUILD_LOGTAIL_UT=${BUILD_LOGTAIL_UT} -DENABLE_COMPATIBLE_MODE=${ENABLE_COMPATIBLE_MODE} -DENABLE_STATIC_LINK_CRT=${ENABLE_STATIC_LINK_CRT} .. && make -sj\$nproc && cd - && ./scripts/upgrade_adapter_lib.sh && ./scripts/plugin_build.sh mod c-shared ${OUT_DIR}" >> $BUILD_SCRIPT_FILE;
  elif [ $CATEGORY = "e2e" ]; then
    echo "mkdir -p core/build && cd core/build && cmake -DLOGTAIL_VERSION=${VERSION} -DBUILD_LOGTAIL_UT=${BUILD_LOGTAIL_UT} -DENABLE_COMPATIBLE_MODE=${ENABLE_COMPATIBLE_MODE} -DENABLE_STATIC_LINK_CRT=${ENABLE_STATIC_LINK_CRT} .. && make -sj\$nproc && cd - && ./scripts/plugin_gocbuild.sh ${OUT_DIR}" >> $BUILD_SCRIPT_FILE;
  fi
}

function generateCopyScript() {
  rm -rf $COPY_SCRIPT_FILE && echo -e "#!/bin/bash\nset -ue\nset -o pipefail\n" > $COPY_SCRIPT_FILE && chmod 755 $COPY_SCRIPT_FILE
  echo 'BINDIR=$(cd $(dirname "${BASH_SOURCE[0]}")&& cd .. && pwd)/'${OUT_DIR}'/' >> $COPY_SCRIPT_FILE
  echo 'rm -rf $BINDIR && mkdir $BINDIR' >> $COPY_SCRIPT_FILE
  echo "id=\$(docker create ${REPOSITORY}:${VERSION})" >> $COPY_SCRIPT_FILE

  if [ $CATEGORY = "plugin" ]; then
    echo 'docker cp "$id":/src/'${OUT_DIR}'/libPluginBase.so $BINDIR'  >> $COPY_SCRIPT_FILE;
  elif [ $CATEGORY = "core" ]; then
    echo 'docker cp "$id":/src/core/build/ilogtail $BINDIR'  >> $COPY_SCRIPT_FILE;
    echo 'docker cp "$id":/src/core/build/plugin/libPluginAdapter.so $BINDIR'  >> $COPY_SCRIPT_FILE;
  else
    echo 'docker cp "$id":/src/'${OUT_DIR}'/libPluginBase.so $BINDIR'  >> $COPY_SCRIPT_FILE;
    echo 'docker cp "$id":/src/core/build/ilogtail $BINDIR'  >> $COPY_SCRIPT_FILE;
    echo 'docker cp "$id":/src/core/build/plugin/libPluginAdapter.so $BINDIR'  >> $COPY_SCRIPT_FILE;
  fi
  echo 'echo -e "{\n}" > $BINDIR/ilogtail_config.json' >> $COPY_SCRIPT_FILE;
  echo 'mkdir -p $BINDIR/user_yaml_config.d' >> $COPY_SCRIPT_FILE;
  echo 'docker rm -v "$id"' >> $COPY_SCRIPT_FILE;
}

mkdir -p $GENERATED_HOME
generateBuildScript
generateCopyScript
