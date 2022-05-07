// Copyright 2021 iLogtail Authors
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

package all

import (
	_ "github.com/alibaba/ilogtail/plugins/aggregator/defaultone"
	_ "github.com/alibaba/ilogtail/plugins/aggregator/logstorerouter"
	_ "github.com/alibaba/ilogtail/plugins/aggregator/shardhash"
	_ "github.com/alibaba/ilogtail/plugins/aggregator/skywalking"
	_ "github.com/alibaba/ilogtail/plugins/flusher/checker"
	_ "github.com/alibaba/ilogtail/plugins/flusher/grpc"
	_ "github.com/alibaba/ilogtail/plugins/flusher/kafka"
	_ "github.com/alibaba/ilogtail/plugins/flusher/sleep"
	_ "github.com/alibaba/ilogtail/plugins/flusher/statistics"
	_ "github.com/alibaba/ilogtail/plugins/flusher/stdout"
	_ "github.com/alibaba/ilogtail/plugins/input/canal"
	_ "github.com/alibaba/ilogtail/plugins/input/docker/event"
	_ "github.com/alibaba/ilogtail/plugins/input/docker/rawstdout"
	_ "github.com/alibaba/ilogtail/plugins/input/docker/stdout"
	_ "github.com/alibaba/ilogtail/plugins/input/example"
	_ "github.com/alibaba/ilogtail/plugins/input/hostmeta"
	_ "github.com/alibaba/ilogtail/plugins/input/http"
	_ "github.com/alibaba/ilogtail/plugins/input/httpserver"
	_ "github.com/alibaba/ilogtail/plugins/input/kafka"
	_ "github.com/alibaba/ilogtail/plugins/input/kubernetesmeta"
	_ "github.com/alibaba/ilogtail/plugins/input/lumberjack"
	_ "github.com/alibaba/ilogtail/plugins/input/mock"
	_ "github.com/alibaba/ilogtail/plugins/input/mockd"
	_ "github.com/alibaba/ilogtail/plugins/input/mqtt"
	_ "github.com/alibaba/ilogtail/plugins/input/mysql"
	_ "github.com/alibaba/ilogtail/plugins/input/mysqlbinlog"
	_ "github.com/alibaba/ilogtail/plugins/input/netping"
	_ "github.com/alibaba/ilogtail/plugins/input/nginx"
	_ "github.com/alibaba/ilogtail/plugins/input/process"
	_ "github.com/alibaba/ilogtail/plugins/input/prometheus"
	_ "github.com/alibaba/ilogtail/plugins/input/redis"
	_ "github.com/alibaba/ilogtail/plugins/input/skywalkingv2"
	_ "github.com/alibaba/ilogtail/plugins/input/skywalkingv3"
	_ "github.com/alibaba/ilogtail/plugins/input/syslog"
	_ "github.com/alibaba/ilogtail/plugins/input/system"
	_ "github.com/alibaba/ilogtail/plugins/input/systemv2"
	_ "github.com/alibaba/ilogtail/plugins/processor/addfields"
	_ "github.com/alibaba/ilogtail/plugins/processor/anchor"
	_ "github.com/alibaba/ilogtail/plugins/processor/appender"
	_ "github.com/alibaba/ilogtail/plugins/processor/base64/decoding"
	_ "github.com/alibaba/ilogtail/plugins/processor/base64/encoding"
	_ "github.com/alibaba/ilogtail/plugins/processor/defaultone"
	_ "github.com/alibaba/ilogtail/plugins/processor/dictmap"
	_ "github.com/alibaba/ilogtail/plugins/processor/drop"
	_ "github.com/alibaba/ilogtail/plugins/processor/droplastkey"
	_ "github.com/alibaba/ilogtail/plugins/processor/encrypt"
	_ "github.com/alibaba/ilogtail/plugins/processor/fieldswithcondition"
	_ "github.com/alibaba/ilogtail/plugins/processor/filter/keyregex"
	_ "github.com/alibaba/ilogtail/plugins/processor/filter/regex"
	_ "github.com/alibaba/ilogtail/plugins/processor/geoip"
	_ "github.com/alibaba/ilogtail/plugins/processor/gotime"
	_ "github.com/alibaba/ilogtail/plugins/processor/json"
	_ "github.com/alibaba/ilogtail/plugins/processor/md5"
	_ "github.com/alibaba/ilogtail/plugins/processor/packjson"
	_ "github.com/alibaba/ilogtail/plugins/processor/pickkey"
	_ "github.com/alibaba/ilogtail/plugins/processor/regex"
	_ "github.com/alibaba/ilogtail/plugins/processor/rename"
	_ "github.com/alibaba/ilogtail/plugins/processor/split/char"
	_ "github.com/alibaba/ilogtail/plugins/processor/split/keyvalue"
	_ "github.com/alibaba/ilogtail/plugins/processor/split/logregex"
	_ "github.com/alibaba/ilogtail/plugins/processor/split/logstring"
	_ "github.com/alibaba/ilogtail/plugins/processor/split/string"
	_ "github.com/alibaba/ilogtail/plugins/processor/strptime"
)
