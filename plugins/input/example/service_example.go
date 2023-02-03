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

package example

import (
	"context"
	"net/http"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
)

// ServiceExample struct implement the ServiceInput interface.
// The plugin has a http server to receive the data.
// You could test the plugin with the command: curl --header "test:val123" http://127.0.0.1:19000/data
type ServiceExample struct {
	Address string
	server  *http.Server
	context pipeline.Context
}

// Init method would be triggered before working. In the example plugin, we only store the logstore config
// context reference. And we return 0 to use the default trigger interval.
func (s *ServiceExample) Init(context pipeline.Context) (int, error) {
	s.context = context
	return 0, nil
}

func (s *ServiceExample) Description() string {
	return "This is a service input example plugin, the plugin would show how to write a simple service input plugin."
}

// Start the service example plugin would run in a separate go routine, so it is blocking method.
// The ServiceInput plugin is suitable for receiving the external input data.
// In the demo, we implemented an http server to accept the specified header of requests.
func (s *ServiceExample) Start(collector pipeline.Collector) error {
	logger.Info(s.context.GetRuntimeContext(), "start the example plugin")
	mux := http.NewServeMux()
	mux.HandleFunc("/data", func(writer http.ResponseWriter, request *http.Request) {
		val := request.Header.Get("test")
		if val != "" {
			collector.AddDataArray(nil, []string{"test"}, []string{val})
		}
	})
	s.server = &http.Server{
		Addr:        s.Address,
		Handler:     mux,
		ReadTimeout: 5 * time.Second,
	}
	return s.server.ListenAndServe()
}

// Stop method would triggered when closing the plugin to graceful stop the go routine blocking with
// the Start method.
func (s *ServiceExample) Stop() error {
	logger.Info(s.context.GetRuntimeContext(), "close the example plugin")
	return s.server.Shutdown(context.Background())
}

// Register the plugin to the ServiceInputs array.
func init() {
	pipeline.ServiceInputs["service_input_example"] = func() pipeline.ServiceInput {
		return &ServiceExample{
			// here you could set default value.
			Address: ":19000",
		}
	}
}
