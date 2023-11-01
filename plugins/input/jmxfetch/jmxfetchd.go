// Copyright 2022 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package jmxfetch

import (
	"fmt"
	"os"
	"os/exec"
	"strings"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/util"
)

const scriptsName = "jmxfetchd"

func (m *Manager) installScripts(javaPath string) error {
	var yamls []string
	for key := range m.allLoadedCfgs {
		yamls = append(yamls, key+".yaml")
	}
	scripts := fmt.Sprintf(scriptsTemplate, javaPath, strings.Join(yamls, ","), "0.0.0.0", m.port)
	err := os.WriteFile(m.jmxfetchdPath, []byte(scripts), 0755) //nolint: gosec
	if err != nil {
		return fmt.Errorf("cannot crate jmxfetchd scripts: %v", err)
	}
	return nil
}

func (m *Manager) start() {
	logger.Debug(m.managerMeta.GetContext(), "start jmxfetchd")
	_, _ = m.execJmxfetchd("start", false)
}

//nolint:unused
func (m *Manager) reload() {
	logger.Debug(m.managerMeta.GetContext(), "reload jmxfetchd")
	_, _ = m.execJmxfetchd("reload", false)
}

func (m *Manager) stop() {
	logger.Debug(m.managerMeta.GetContext(), "stop jmxfetchd")
	if exist, _ := util.PathExists(m.jmxfetchdPath); !exist {
		return
	}
	_, _ = m.execJmxfetchd("stop", false)
}

// nolint:unparam
func (m *Manager) execJmxfetchd(command string, needOutput bool) (output []byte, err error) {
	cmd := exec.Command("bash", m.jmxfetchdPath, command) //nolint:gosec
	if needOutput {
		output, err = cmd.CombinedOutput()
	} else {
		// Must call start/reload without output, because they might fork sub process,
		// which will hang when CombinedOutput is called.
		err = cmd.Run()
	}
	if err != nil && !strings.Contains(err.Error(), "no child process") {
		logger.Warningf(m.managerMeta.GetContext(), "JMXFETCH_RUNTIME_ALARM", "%v error, output: %v, error: %v", command, string(output), err)
	}
	return
}

var scriptsTemplate = `
#!/bin/bash

CURRENT_DIR=$(cd "$(dirname "$0")";pwd)
JAVA_CMD=%s
CHECKERS=%s
JAR="ilogtail_jmxfetch.jar"
REPORTER="statsd:%s:%d" 


cd "$CURRENT_DIR"

log_file="trace.log"
start_log_file="start.log"

trace_log() {
    echo "$(date) $1" >>$log_file
    filesize=$(wc -c <"$log_file")
    if [ $filesize -ge 1024000 ]; then
        mv -f $log_file "${log_file}.1"
    fi
}

do_start() {
    rm -rf start.log
    trace_log "do_start"
    touch $start_log_file
    $JAVA_CMD -jar $JAR collect -D "$CURRENT_DIR"/conf.d -c "$CHECKERS" -r $REPORTER > $start_log_file 2>&1 &
}

start() {
    c=$(ps -ef |grep $JAR |grep -v grep|grep "$CURRENT_DIR"|grep "$REPORTER"  | wc -l)
    trace_log "start $c"
    if [ $c -eq 0 ]; then
        do_start
    elif [ $c -ne 1 ]; then
        trace_log "abnormal status: $c"
    fi
}


stop() {
    sig=$1
    ppids=($(ps -ef |grep $JAR |grep -v grep|grep "$CURRENT_DIR"|grep "$REPORTER"  | awk '{print $2}'))
    trace_log "stop with $sig, ppids: $ppids"
    for ppid in ${ppids[*]}; do
        kill $sig $ppid
    done
}

force_stop() {
    trace_log "force_stop"
    stop "-9"
}

status() {
    c=$(ps -ef |grep $JAR |grep -v grep|grep "$CURRENT_DIR"|grep "$REPORTER"  | wc -l)
    if [ $c -eq 1 ]; then
        val="running"
    elif [ $c -eq 0 ]; then
        val="stopped"
    else
        val="abnormal"
        retcode=1
    fi
    trace_log "status $val $c"
    echo $val
}

reload() {
    c=$(ps -ef |grep $JAR |grep -v grep|grep "$CURRENT_DIR"|grep "$REPORTER" | wc -l)
    trace_log "reload $c"
    if [ $c -eq 0 ]; then
        do_start
    else
        stop "-1"
    fi
}

retcode=0
case "$1" in
start)
    start
    ;;
stop)
    stop
    ;;
force-stop)
    force_stop
    ;;
reload)
    reload
    ;;
status)
    status
    ;;
esac
exit $retcode
`
