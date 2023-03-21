// Copyright 2023 iLogtail Authors
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
	"testing"
	"time"

	_ "github.com/alibaba/ilogtail/pkg/logger/test"
)

const testLog = `2022-09-13 17:06:42 CST | JMX | INFO | Connection | Connecting to: service:jmx:rmi:///jndi/rmi://127.0.0.1:1617/jmxrmi
2022-09-13 17:06:42 CST | JMX | INFO | App | Completed instance initialization...
2022-09-13 17:06:42 CST | JMX | WARN | App | Could not initialize instance: ljp_test_bjshaopxuan_metrics__1_0__ljp_test_bj_custom_plugin_config_yfutcsvm-127.0.0.1-1617: 
java.util.concurrent.ExecutionException: java.io.IOException: Failed to retrieve RMIServer stub: javax.naming.ServiceUnavailableException [Root exception is java.rmi.ConnectException: Connection refused to host: 127.0.0.1; nested exception is: 
        java.net.ConnectException: Connection refused (Connection refused)]
        at java.util.concurrent.FutureTask.report(FutureTask.java:122)
        at java.util.concurrent.FutureTask.get(FutureTask.java:192)
        at org.datadog.jmxfetch.App.processRecoveryResults(App.java:1001)
        at org.datadog.jmxfetch.App$6.invoke(App.java:977)
        at org.datadog.jmxfetch.tasks.TaskProcessor.processTasks(TaskProcessor.java:63)
        at org.datadog.jmxfetch.App.init(App.java:969)
        at org.datadog.jmxfetch.App.start(App.java:463)
        at org.datadog.jmxfetch.App.run(App.java:211)
        at org.datadog.jmxfetch.App.main(App.java:155)
Caused by: java.io.IOException: Failed to retrieve RMIServer stub: javax.naming.ServiceUnavailableException [Root exception is java.rmi.ConnectException: Connection refused to host: 127.0.0.1; nested exception is: 
        java.net.ConnectException: Connection refused (Connection refused)]
        at javax.management.remote.rmi.RMIConnector.connect(RMIConnector.java:369)
        at javax.management.remote.JMXConnectorFactory.connect(JMXConnectorFactory.java:270)
        at org.datadog.jmxfetch.Connection.createConnection(Connection.java:64)
2022-09-13 17:06:42 CST | JMX | INFO | App | Completed instance initialization...
`

func TestProcessor_Process(t *testing.T) {
	manager = createManager("test")
	NewProcessor().Process([]byte(testLog), time.Second)
}
