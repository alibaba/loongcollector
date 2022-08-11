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

package main

import (
	"flag"
	"fmt"
	_ "github.com/go-sql-driver/mysql"
	"github.com/gorilla/mux"
	_ "github.com/lib/pq"
	"net/http"
	"strconv"
	"strings"
	"time"
)

var protocl = flag.String("protocol", "http", "protocl")
var client = flag.Bool("client", true, "client or server, true as client, false as server")
var qps = flag.Int("qps", 100, "qps")
var latency = flag.Int("latency", 10, "latency ms")
var count = flag.Int("count", 1000, "total count")
var concurrency = flag.Int("concurrency", 10, "concurrency")
var payload = flag.Int("payload", 100, "payload size")
var port = flag.Int("port", 8000, "port")
var address = flag.String("address", "127.0.0.1", "address")
var print = flag.Bool("print", true, "print logs")
var delay = flag.Int("delay", 1, "delay start")

var payloadString strings.Builder

func generatePayload(size int) {
	baseStr := "1234567890!@#$%^&*()_+{}[];':,.<>./>?abcdef"
	for payloadString.Len() < size {
		payloadString.WriteString(baseStr)
	}
}

func indexHandler(w http.ResponseWriter, r *http.Request) {
	if *latency > 0 {
		time.Sleep(time.Millisecond * time.Duration(*latency))
	}
	w.Write([]byte(payloadString.String()))
}

func mockHttpServer() {
	generatePayload(*payload)
	rtr := mux.NewRouter()
	rtr.HandleFunc("/api/{name:[0-9]+}/get", indexHandler).Methods("GET")
	http.Handle("/", rtr)
	http.ListenAndServe(*address+":"+strconv.Itoa(*port), nil)
}

func main() {
	flag.Parse()
	fmt.Println("started...")
	time.Sleep(time.Duration(*delay) * time.Second)
	now := time.Now()
	switch *protocl {

	case "dns":
		if *client {
			mockDnsClient()
		} else {
			mocDnsServer()
		}
	}
	after := time.Now()
	if SuccessCount > 0 && FailureCount > 0 {
		fmt.Printf("success:%d failure:%d totalLatency:%d avgLatency:%d qps:%f", SuccessCount, FailureCount, TotalLatency, TotalLatency/SuccessCount, (float64(SuccessCount))/(after.Sub(now).Seconds()))
	}
	time.Sleep(time.Hour)
}
