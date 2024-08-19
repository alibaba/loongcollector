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
VERSION=${3:-2.0.0}
REPOSITORY=${4:-aliyun/ilogtail}
OUT_DIR=${5:-output}
EXPORT_GO_ENVS=${6:-${DOCKER_BUILD_EXPORT_GO_ENVS:-true}}
COPY_GIT_CONFIGS=${7:-${DOCKER_BUILD_COPY_GIT_CONFIGS:-true}}
PLUGINS_CONFIG_FILE=${8:-${PLUGINS_CONFIG_FILE:-plugins.yml,external_plugins.yml}}
GO_MOD_FILE=${9:-${GO_MOD_FILE:-go.mod}}
PATH_IN_DOCKER=${10:-/src}

BUILD_TYPE=${BUILD_TYPE:-Release}
BUILD_LOGTAIL=${BUILD_LOGTAIL:-ON}
BUILD_LOGTAIL_UT=${BUILD_LOGTAIL_UT:-OFF}
ENABLE_COMPATIBLE_MODE=${ENABLE_COMPATIBLE_MODE:-OFF}
ENABLE_STATIC_LINK_CRT=${ENABLE_STATIC_LINK_CRT:-OFF}
WITHOUTGDB==${WITHOUTGDB:-OFF}
WITHSPL=${WITHSPL:-ON}
BUILD_SCRIPT_FILE=$GENERATED_HOME/gen_build.sh
COPY_SCRIPT_FILE=$GENERATED_HOME/gen_copy_docker.sh
MAKE_JOBS=${MAKE_JOBS:-$(nproc)}

function generateBuildScript() {
  rm -rf $BUILD_SCRIPT_FILE
  cat >$BUILD_SCRIPT_FILE <<-EOF
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

  if [ $EXPORT_GO_ENVS = "true" ]; then
    envs=($(go env | grep -E 'GOPRIVATE=(".+"|'\''.+'\'')|GOPROXY=(".+"|'\''.+'\'')'))
    for v in ${envs[@]}; do
      echo "go env -w $v" >> $BUILD_SCRIPT_FILE
    done
  fi

  if [ $COPY_GIT_CONFIGS = "true" ]; then
    globalUrlConfigs=($(git config -l --global 2>/dev/null | grep -E '^url\.'||true))
    for gc in ${globalUrlConfigs[@]:-}; do
      echo "git config --global $(echo "$gc" | sed 's/=/ /')" >> $BUILD_SCRIPT_FILE
    done
  fi

  echo "echo 'StrictHostkeyChecking no' >> /etc/ssh/ssh_config" >> $BUILD_SCRIPT_FILE

  chmod 755 $BUILD_SCRIPT_FILE
  if [ $CATEGORY = "plugin" ]; then
    echo "mkdir -p core/build && cd core/build && cmake -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DLOGTAIL_VERSION=${VERSION} .. && cd plugin && make -s PluginAdapter && cd ../../.. && ./scripts/upgrade_adapter_lib.sh && ./scripts/plugin_build.sh mod c-shared ${OUT_DIR} ${VERSION} ${PLUGINS_CONFIG_FILE} ${GO_MOD_FILE}" >>$BUILD_SCRIPT_FILE
  elif [ $CATEGORY = "core" ]; then
    echo "mkdir -p core/build && cd core/build && cmake -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DLOGTAIL_VERSION=${VERSION} -DBUILD_LOGTAIL=${BUILD_LOGTAIL} -DBUILD_LOGTAIL_UT=${BUILD_LOGTAIL_UT} -DENABLE_COMPATIBLE_MODE=${ENABLE_COMPATIBLE_MODE} -DENABLE_STATIC_LINK_CRT=${ENABLE_STATIC_LINK_CRT} -DWITHOUTGDB=${WITHOUTGDB} -DWITHSPL=${WITHSPL} .. && make -sj${MAKE_JOBS}" >>$BUILD_SCRIPT_FILE
  elif [ $CATEGORY = "all" ]; then
    echo "mkdir -p core/build && cd core/build && cmake -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DLOGTAIL_VERSION=${VERSION} -DBUILD_LOGTAIL_UT=${BUILD_LOGTAIL_UT} -DENABLE_COMPATIBLE_MODE=${ENABLE_COMPATIBLE_MODE} -DENABLE_STATIC_LINK_CRT=${ENABLE_STATIC_LINK_CRT} -DWITHOUTGDB=${WITHOUTGDB} .. && make -sj\$nproc && cd - && ./scripts/upgrade_adapter_lib.sh && ./scripts/plugin_build.sh mod c-shared ${OUT_DIR} ${VERSION} ${PLUGINS_CONFIG_FILE} ${GO_MOD_FILE}" >>$BUILD_SCRIPT_FILE
  elif [ $CATEGORY = "e2e" ]; then
    echo "mkdir -p core/build && cd core/build && cmake -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DLOGTAIL_VERSION=${VERSION} -DBUILD_LOGTAIL_UT=${BUILD_LOGTAIL_UT} -DENABLE_COMPATIBLE_MODE=${ENABLE_COMPATIBLE_MODE} -DENABLE_STATIC_LINK_CRT=${ENABLE_STATIC_LINK_CRT} -DWITHOUTGDB=${WITHOUTGDB} .. && make -sj\$nproc && cd - && ./scripts/plugin_gocbuild.sh ${OUT_DIR} ${PLUGINS_CONFIG_FILE} ${GO_MOD_FILE}" >>$BUILD_SCRIPT_FILE
  fi
}

function generateCopyScript() {
  rm -rf $COPY_SCRIPT_FILE && echo -e "#!/bin/bash\nset -ue\nset -o pipefail\n" >$COPY_SCRIPT_FILE && chmod 755 $COPY_SCRIPT_FILE
  echo 'BINDIR=$(cd $(dirname "${BASH_SOURCE[0]}")&& cd .. && pwd)/'${OUT_DIR}'/' >>$COPY_SCRIPT_FILE
  echo 'rm -rf $BINDIR && mkdir $BINDIR' >>$COPY_SCRIPT_FILE
  echo "id=\$(docker create ${REPOSITORY}:${VERSION})" >>$COPY_SCRIPT_FILE

  if [ $CATEGORY = "plugin" ]; then
    echo 'docker cp "$id":'${PATH_IN_DOCKER}'/'${OUT_DIR}'/libPluginBase.so $BINDIR' >>$COPY_SCRIPT_FILE
  elif [ $CATEGORY = "core" ]; then
    if [ $BUILD_LOGTAIL = "ON" ]; then
      echo 'docker cp "$id":'${PATH_IN_DOCKER}'/core/build/ilogtail $BINDIR' >>$COPY_SCRIPT_FILE
      echo 'docker cp "$id":'${PATH_IN_DOCKER}'/core/build/go_pipeline/libPluginAdapter.so $BINDIR' >>$COPY_SCRIPT_FILE
    fi
    if [ $BUILD_LOGTAIL_UT = "ON" ]; then
      echo 'docker cp "$id":'${PATH_IN_DOCKER}'/core/build core/build' >>$COPY_SCRIPT_FILE
      echo 'rm -rf core/log_pb && docker cp "$id":'${PATH_IN_DOCKER}'/core/log_pb core/log_pb' >>$COPY_SCRIPT_FILE
    fi
  else
    echo 'docker cp "$id":'${PATH_IN_DOCKER}'/'${OUT_DIR}'/libPluginBase.so $BINDIR' >>$COPY_SCRIPT_FILE
    echo 'docker cp "$id":'${PATH_IN_DOCKER}'/core/build/ilogtail $BINDIR' >>$COPY_SCRIPT_FILE
    echo 'docker cp "$id":'${PATH_IN_DOCKER}'/core/build/go_pipeline/libPluginAdapter.so $BINDIR' >>$COPY_SCRIPT_FILE
    if [ $BUILD_LOGTAIL_UT = "ON" ]; then
      echo 'docker cp "$id":'${PATH_IN_DOCKER}'/core/build core/build' >>$COPY_SCRIPT_FILE
      echo 'rm -rf core/log_pb && docker cp "$id":'${PATH_IN_DOCKER}'/core/log_pb core/log_pb' >>$COPY_SCRIPT_FILE
    fi
  fi
  echo 'echo -e "{\n}" > $BINDIR/ilogtail_config.json' >>$COPY_SCRIPT_FILE
  echo 'mkdir -p $BINDIR/config/local' >>$COPY_SCRIPT_FILE
  echo 'docker rm -v "$id"' >>$COPY_SCRIPT_FILE
}

mkdir -p $GENERATED_HOME
generateBuildScript
generateCopyScript
