set -ue
set -o pipefail

function arch() {
  if uname -m | grep x86_64 &>/dev/null; then
    echo amd64
  elif uname -m | grep -E "aarch64|arm64" &>/dev/null; then
    echo arm64
  else
    echo sw64
  fi
}

ARCH=$(arch)
HOST_OS=`uname -s`
ROOT_DIR=$(cd $(dirname "${BASH_SOURCE[0]}") && cd .. && pwd)

rm -rf $ROOT_DIR/license_coverage.txt
rm -rf $ROOT_DIR/output
rm -rf $ROOT_DIR/dist
rm -rf $ROOT_DIR/behavior-test
rm -rf $ROOT_DIR/performance-test
rm -rf $ROOT_DIR/core-test
rm -rf $ROOT_DIR/e2e-engine-coverage.txt
rm -rf $ROOT_DIR/find_licenses
rm -rf $ROOT_DIR/generated_files
rm -rf $ROOT_DIR/.testCoverage.txt
rm -rf $ROOT_DIR/.coretestCoverage.txt
rm -rf $ROOT_DIR/core/build
rm -rf $ROOT_DIR/plugin_main/*.dll
rm -rf $ROOT_DIR/plugin_main/*.so
rm -rf $ROOT_DIR/plugins/all/all.go
rm -rf $ROOT_DIR/plugins/all/all_debug.go
rm -rf $ROOT_DIR/plugins/all/all_windows.go
rm -rf $ROOT_DIR/plugins/all/all_linux.go
go mod tidy -modfile "go.mod" || true

echo "===============GENERATING PLUGINS IMPORT=================="
echo "config: plugins.yml,external_plugins.yml"
echo "modfile: go.mod"
echo "root-dir: $ROOT_DIR"
echo "=========================================================="

go run -mod=mod "$ROOT_DIR/tools/builder" -root-dir="$ROOT_DIR" -config="plugins.yml,external_plugins.yml" -modfile="go.mod" && \
echo "generating plugins finished successfully"

mkdir -p generated_files

# gen gen_build.sh
cat >generated_files/gen_build.sh <<-EOF
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

envs=($(go env | grep -E 'GOPRIVATE=(".+"|'\''.+'\'')|GOPROXY=(".+"|'\''.+'\'')'))
for v in ${envs[@]}; do
    echo "go env -w $v" >> generated_files/gen_build.sh
done

globalUrlConfigs=($(git config -l --global 2>/dev/null | grep -E '^url\.'||true))
for gc in ${globalUrlConfigs[@]:-}; do
    echo "git config --global $(echo "$gc" | sed 's/=/ /')" >> generated_files/gen_build.sh
done

echo "echo 'StrictHostkeyChecking no' >> /etc/ssh/ssh_config" >> generated_files/gen_build.sh
chmod 755 generated_files/gen_build.sh
echo "mkdir -p core/build && cd core/build && cmake -DCMAKE_BUILD_TYPE=Release -DLOGTAIL_VERSION=edge -DBUILD_LOGTAIL_UT=OFF -DENABLE_COMPATIBLE_MODE=OFF -DENABLE_STATIC_LINK_CRT=OFF -DWITHOUTGDB=OFF .. && make -sj\$nproc && cd - && ./scripts/upgrade_adapter_lib.sh && ./scripts/plugin_build.sh mod c-shared output edge plugins.yml,external_plugins.yml go.mod" >> generated_files/gen_build.sh

./generated_files/gen_build.sh