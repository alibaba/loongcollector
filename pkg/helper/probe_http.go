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

package helper

import (
	"net/http"
	"os"
	"strconv"
)

func init() {
	checkAlivePort := os.Getenv("HTTP_PROBE_PORT")
	if len(checkAlivePort) == 0 {
		return
	}
	checkPort, err := strconv.Atoi(checkAlivePort)
	if err != nil {
		return
	}
	server := &http.Server{ //nolint:gosec
		Addr: ":" + strconv.Itoa(checkPort),
	}
	mux := http.NewServeMux()
	mux.Handle("/", &probeHandler{})
	server.Handler = mux
	go func() {
		_ = server.ListenAndServe()
	}()
}

type probeHandler struct{}

func (*probeHandler) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	_, _ = w.Write([]byte("probe called"))
}
