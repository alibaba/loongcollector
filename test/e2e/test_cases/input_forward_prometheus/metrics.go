// Copyright 2026 iLogtail Authors
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
	"fmt"
	"io"
	"log"
	"net/http"
	"time"

	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
	"google.golang.org/grpc/metadata"
	"google.golang.org/protobuf/types/known/emptypb"
	"google.golang.org/protobuf/types/known/wrapperspb"

	"github.com/alibaba/ilogtail/pkg/protocol"
)

func main() {
	go func() {
		if err := sendForwardMetric(); err != nil {
			log.Fatal(err)
		}
	}()

	mux := http.NewServeMux()
	mux.HandleFunc("/metrics", func(w http.ResponseWriter, _ *http.Request) {
		w.Header().Set("Content-Type", "text/plain; version=0.0.4")
		_, _ = io.WriteString(w, "# TYPE e2e_scraped_metric gauge\ne2e_scraped_metric{source=\"scrape\"} 17\n")
	})
	server := &http.Server{Addr: ":18080", Handler: mux, ReadHeaderTimeout: 5 * time.Second}
	log.Fatal(server.ListenAndServe())
}

func sendForwardMetric() error {
	client, err := grpc.NewClient("loongcollectorC:7899", grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		return fmt.Errorf("create forward client: %w", err)
	}
	defer client.Close()

	ctx, cancel := context.WithTimeout(context.Background(), 90*time.Second)
	defer cancel()
	ctx = metadata.AppendToOutgoingContext(ctx, "x-loongsuite-apm-configname", "forward-prometheus")

	group := &protocol.PipelineEventGroup{
		PipelineEvents: &protocol.PipelineEventGroup_Metrics{
			Metrics: &protocol.PipelineEventGroup_MetricEvents{
				Events: []*protocol.MetricEvent{{
					Timestamp: uint64(time.Now().UnixNano()),
					Name:      []byte("e2e_forward_metric"),
					Tags:      map[string][]byte{"source": []byte("forward")},
					Value: &protocol.MetricEvent_UntypedSingleValue{
						UntypedSingleValue: &protocol.UntypedSingleValue{Value: 42},
					},
				}},
			},
		},
	}
	payload, err := group.Marshal()
	if err != nil {
		return fmt.Errorf("marshal pipeline event group: %w", err)
	}

	// LoongSuiteForwardRequest.data is repeated bytes field 1. BytesValue
	// serializes the same field for a request with a single event group.
	if err := client.Invoke(ctx, "/logtail.LoongSuiteForwardService/Forward",
		wrapperspb.Bytes(payload), &emptypb.Empty{}, grpc.WaitForReady(true)); err != nil {
		return fmt.Errorf("forward metric: %w", err)
	}
	return nil
}
