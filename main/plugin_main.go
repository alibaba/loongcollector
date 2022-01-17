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

package main

import (
	"context"
	"flag"
	"fmt"
	"runtime"

	"github.com/alibaba/ilogtail"
	_ "github.com/alibaba/ilogtail/helper/envconfig"
	"github.com/alibaba/ilogtail/main/flags"
	_ "github.com/alibaba/ilogtail/main/wrapmemcpy"
	"github.com/alibaba/ilogtail/pkg/doc"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/signals"
	"github.com/alibaba/ilogtail/pkg/util"
	_ "github.com/alibaba/ilogtail/plugins/all"
)

// main export http control method in pure GO.
func main() {
	flag.Parse()
	defer logger.Flush()
	if *flags.Doc {
		generatePluginDoc()
		return
	}
	cpu := runtime.NumCPU()
	procs := runtime.GOMAXPROCS(0)
	fmt.Println("cpu num:", cpu, " GOMAXPROCS:", procs)
	fmt.Println("hostname : ", util.GetHostName())
	fmt.Println("hostIP : ", util.GetIPAddress())
	fmt.Printf("load config %s %s %s\n", *flags.GlobalConfig, *flags.PluginConfig, *flags.FlusherConfig)
	pluginCfg, globalCfg := flags.LoadConfig()
	handlers["/loadconfig"] = &handler{handlerFunc: HandleLoadConfig, description: "load new logtail plugin configuration"}
	handlers["/holdon"] = &handler{handlerFunc: HandleHoldOn, description: "hold on logtail plugin process"}
	if InitPluginBaseV2(globalCfg) != 0 || LoadConfig("PluginProject", "PluginLogstore",
		"1.0#PluginProject##MockConfig2", 123, pluginCfg) != 0 {
		return
	}
	Resume()
	// handle the first shutdown signal gracefully
	<-signals.SetupSignalHandler()
	logger.Info(context.Background(), "########################## exit process begin ##########################")
	HoldOn(1)
	logger.Info(context.Background(), "########################## exit process done ##########################")
}

func generatePluginDoc() {
	for name, creator := range ilogtail.ServiceInputs {
		doc.Register("service_input", name, creator())
	}
	for name, creator := range ilogtail.MetricInputs {
		doc.Register("metric_input", name, creator())
	}
	for name, creator := range ilogtail.Processors {
		doc.Register("processor", name, creator())
	}
	for name, creator := range ilogtail.Aggregators {
		doc.Register("aggregator", name, creator())
	}
	for name, creator := range ilogtail.Flushers {
		doc.Register("flusher", name, creator())
	}
	doc.Generate(*flags.DocPath)
}
