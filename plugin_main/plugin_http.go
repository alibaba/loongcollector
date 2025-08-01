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
	"io"
	"net/http"
	_ "net/http/pprof" //nolint
	"os"
	"runtime"
	"runtime/debug"
	"runtime/pprof"
	"runtime/trace"
	"sync"
	"time"

	"github.com/alibaba/ilogtail/pkg/config"
	"github.com/alibaba/ilogtail/pkg/flags"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pluginmanager"
)

var (
	handlers    = make(map[string]*handler) // contains the whole export http.HandlerFunc.
	controlLock sync.Mutex                  // the http logtail plugin control handlers should be invoked in order.
	once        sync.Once
)

// handler wraps the http.HandlerFunc with description.
type handler struct {
	handlerFunc http.HandlerFunc
	description string
}

// HandleMem dump the memory info at once.
func HandleMem(w http.ResponseWriter, req *http.Request) {
	DumpMemInfo(0)
}

// HandleTrace dump the trace info at once.
func HandleTrace(w http.ResponseWriter, req *http.Request) {
	DumpTraceInfo()
}

func HandleStack(w http.ResponseWriter, req *http.Request) {
	DumpAllGoroutineStacks()
}

// HandleCPU dump the CPU info in 30 seconds.
func HandleCPU(w http.ResponseWriter, req *http.Request) {
	DumpCPUInfo(30)
}

// HandleCPU10 dump the CPU info in 10 seconds.
func HandleCPU10(w http.ResponseWriter, req *http.Request) {
	DumpCPUInfo(10)
}

// HandleCPU180 dump the CPU info in 180 seconds.
func HandleCPU180(w http.ResponseWriter, req *http.Request) {
	DumpCPUInfo(180)
}

// HandleForceGC trigger force GC.
func HandleForceGC(w http.ResponseWriter, req *http.Request) {
	logger.Info(context.Background(), "GC begin")
	runtime.GC()
	logger.Info(context.Background(), "GC done")
	debug.FreeOSMemory()
	logger.Info(context.Background(), "Free os memory done")
}

// HelpServer show the help messages of the whole exported handlers.
func HelpServer(w http.ResponseWriter, req *http.Request) {
	_, _ = io.WriteString(w, `
		/mem      to dump mem info
		/cpu      to dump cpu info, default 30 seconds
		/cpu10    to dump cpu info, 10 seconds
		/cpu180   to dump cpu info, 180 seconds
		/debug/pprof/goroutine?debug=1   
		/debug/pprof/heap?debug=1
		/debug/pprof/threadcreate?debug=1
		/forcegc
		`)
}

// HandleLoadConfig load the new ilogtail config.
func HandleLoadConfig(w http.ResponseWriter, r *http.Request) {
	controlLock.Lock()
	defer controlLock.Unlock()
	bytes, err := io.ReadAll(r.Body)
	if err != nil {
		logger.Error(context.Background(), "LOAD_CONFIG_ALARM", "stage", "read", "err", err)
		w.WriteHeader(500)
		_, _ = w.Write([]byte("read body error"))
		return
	}
	logger.Infof(context.Background(), "%s", string(bytes))
	loadConfigs, err := config.DeserializeLoadedConfig(bytes)
	if err != nil {
		logger.Error(context.Background(), "LOAD_CONFIG_ALARM", "stage", "parse", "err", err)
		w.WriteHeader(500)
		_, _ = w.Write([]byte("parse body error"))
		return
	}
	for _, cfg := range loadConfigs {
		Stop(cfg.ConfigName, 0)
		LoadPipeline(cfg.Project, cfg.Logstore, cfg.ConfigName, cfg.LogstoreKey, cfg.JSONStr)
		Start(cfg.ConfigName)
	}
}

// HandleHoldOn hold on the ilogtail process.
func HandleHoldOn(w http.ResponseWriter, r *http.Request) {
	controlLock.Lock()
	defer controlLock.Unlock()
	StopAllPipelines(1)
	StopAllPipelines(0)
	w.WriteHeader(http.StatusOK)
}

// DumpCPUInfo dump the cpu profile info with the given seconds.
func DumpCPUInfo(seconds int) {
	f, err := os.Create(*flags.Cpuprofile)
	if err != nil {
		return
	}
	if err := pprof.StartCPUProfile(f); err != nil {
		return
	}
	time.Sleep((time.Duration)(seconds) * time.Second)
	pprof.StopCPUProfile()
}

// DumpTraceInfo dump the logtail plugin process trace info.
func DumpTraceInfo() {
	f, err := os.Create("trace.out")
	if err != nil {
		return
	}
	defer func(f *os.File) {
		_ = f.Close()
	}(f)

	err = trace.Start(f)
	if err != nil {
		return
	}
	defer trace.Stop()
}

// 获取所有 Goroutine 的堆栈信息并写入到文件
func DumpAllGoroutineStacks() {
	// 创建一个文件用于存储堆栈信息
	file, err := os.Create("stack.out")
	if err != nil {
		return
	}
	defer file.Close()

	// 创建缓冲区以存储堆栈信息
	buf := make([]byte, 10<<20)           // 10MB 缓冲区(目前测试下来，4000+协程，堆栈4MB左右)
	stackSize := runtime.Stack(buf, true) // 获取 Goroutine 的堆栈信息

	// 将堆栈信息写入到文件
	_, err = file.Write(buf[:stackSize])
	if err != nil {
		return
	}
}

// DumpMemInfo dump the mem profile info after the given seconds.
func DumpMemInfo(seconds int) {
	time.Sleep((time.Duration)(seconds) * time.Second)

	f, err := os.Create(*flags.Memprofile)
	if err != nil {
		return
	}
	_ = pprof.WriteHeapProfile(f)
	_ = f.Close()
}

// InitHTTPServer chooses the exported handlers according to the flags and starts an HTTP server.
// When the HTTP prof flag is opening, the debug handlers will also work, which are exported by
// `net/http/pprof` package.
func InitHTTPServer() {
	once.Do(func() {
		if *flags.HTTPLoadFlag {
			handlers["/loadconfig"] = &handler{handlerFunc: HandleLoadConfig, description: "load new logtail plugin configuration"}
			handlers["/holdon"] = &handler{handlerFunc: HandleHoldOn, description: "hold on logtail plugin process"}
		}
		if *flags.HTTPProfFlag {
			handlers["/mem"] = &handler{handlerFunc: HandleMem, description: "dump mem info"}
			handlers["/cpu"] = &handler{handlerFunc: HandleCPU, description: "dump cpu info, default 30 seconds"}
			handlers["/cpu10"] = &handler{handlerFunc: HandleCPU10, description: "dump cpu info, 10 seconds"}
			handlers["/cpu180"] = &handler{handlerFunc: HandleCPU180, description: "dump cpu info, 180 seconds"}
			handlers["/forcegc"] = &handler{handlerFunc: HandleForceGC, description: "force gc"}
			handlers["/trace"] = &handler{handlerFunc: HandleTrace, description: "dump trace info"}
			handlers["/stack"] = &handler{handlerFunc: HandleStack, description: "dump stack info"}
			runtime.SetBlockProfileRate(1)
			if *flags.AutoProfile {
				go DumpCPUInfo(100)
				go DumpMemInfo(100)
			}
		}
		if *flags.StatefulSetFlag {
			handlers["/export/port"] = &handler{handlerFunc: pluginmanager.FindPort, description: "export ilogtail's LISTEN ports"}
		}
		if len(handlers) != 0 {
			handlers["/"] = &handler{handlerFunc: HelpServer, description: "handlers help description"}
			handlers["/help"] = &handler{handlerFunc: HelpServer, description: "handlers help description"}
			go func() {
				var mux *http.ServeMux
				if *flags.HTTPProfFlag {
					// bound the handlers exported by `net/http/pprof`
					mux = http.DefaultServeMux
				} else {
					mux = http.NewServeMux()
				}
				for path, handler := range handlers {
					mux.HandleFunc(path, handler.handlerFunc)
				}
				logger.Info(context.Background(), "#####################################")
				logger.Info(context.Background(), "start http server for logtail plugin profile or control")
				logger.Info(context.Background(), "#####################################")
				logger.Critical(context.Background(), "INIT_HTTP_SERVER_ALARM", "err", http.ListenAndServe(*flags.HTTPAddr, mux)) //nolint
			}()
		}
	})
}
