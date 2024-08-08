// Copyright 2024 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//	http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
package pluginmanager

import (
	"runtime"
	"strconv"
)

func GetMetrics() []map[string]string {
	metrics := make([]map[string]string, 0)
	for _, config := range LogtailConfig {
		metrics = append(metrics, config.Context.ExportMetricRecords()...)
	}
	metrics = append(metrics, GetProcessStat())
	return metrics
}

func GetProcessStat() map[string]string {
	recrods := map[string]string{}
	recrods["metric-level"] = "process"
	// cpu
	// {
	// 	recrods["global_cpu_go_used_cores"] = "0"
	// }
	// mem
	{
		memStat := runtime.MemStats{}
		runtime.ReadMemStats(&memStat)
		recrods["global_memory_go_used_mb"] = strconv.Itoa(int(memStat.Sys / 1024.0 / 1024.0))
	}
	return recrods
}
