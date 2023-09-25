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
	"io"
	"math/rand"
	"net/http"
	"strconv"
	"strings"
	"sync"
	"sync/atomic"
	"time"

	_ "github.com/go-sql-driver/mysql"
	"github.com/gorilla/mux"
	_ "github.com/lib/pq"
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

var (
	SuccessCount int64
	FailureCount int64
	TotalLatency int64
)

func mockHttpClient() {
	http.DefaultTransport.(*http.Transport).MaxIdleConnsPerHost = 100
	subQps := *qps / *concurrency
	subCount := *count / *concurrency / subQps
	var wg sync.WaitGroup
	for i := 0; i < *concurrency; i++ {
		wg.Add(1)
		go func(index int) {
			var httpClient = &http.Client{}
			for j := 0; j < subCount; j++ {
				nowTime := time.Now()
				for k := 0; k < subQps; k++ {
					// request
					intn := rand.Intn(10)
					var request *http.Request
					var err error
					if intn < 9 {
						request, err = http.NewRequest("GET", "http://"+*address+":"+strconv.Itoa(*port)+"/api/"+strconv.Itoa(index)+"/get", nil)
					} else {
						request, err = http.NewRequest("GET", "http://"+*address+":"+strconv.Itoa(*port)+"/api1/"+strconv.Itoa(index)+"/get", nil)
					}
					if err != nil {
						panic(err)
					}
					beforeTime := time.Now()
					// don't use keep alive connection to avoid out-of-order when concurrency.
					request.Close = true
					resp, err := httpClient.Do(request)
					afterTime := time.Now()
					if err == nil {
						atomic.AddInt64(&SuccessCount, 1)
						atomic.AddInt64(&TotalLatency, afterTime.Sub(beforeTime).Microseconds())
						io.Copy(io.Discard, resp.Body)
						resp.Body.Close()
						if *print {
							fmt.Printf("request : %s, response : %d \n", request.URL.Path, resp.StatusCode)
						}
					} else {
						atomic.AddInt64(&FailureCount, 1)
						if *print {
							fmt.Printf("request : %s, error : %s \n", request.URL.Path, err)
						}
					}
				}
				if duration := time.Since(nowTime); duration < time.Second {
					time.Sleep(time.Second - duration)
				}
			}
			wg.Done()
		}(i)

	}
	wg.Wait()
}

func main() {
	flag.Parse()
	fmt.Println("started...")
	time.Sleep(time.Duration(*delay) * time.Second)
	now := time.Now()
	switch *protocl {
	case "http":
		if *client {
			mockHttpClient()
		} else {
			mockHttpServer()
		}
	}
	after := time.Now()
	if SuccessCount > 0 && FailureCount > 0 {
		fmt.Printf("success:%d failure:%d totalLatency:%d avgLatency:%d qps:%f", SuccessCount, FailureCount, TotalLatency, TotalLatency/SuccessCount, (float64(SuccessCount))/(after.Sub(now).Seconds()))
	}
	time.Sleep(time.Hour)
}
