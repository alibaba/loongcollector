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

package requestbreaker

import (
	"fmt"
	"net/http"
	"time"

	"github.com/streadway/handy/breaker"

	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/pipeline/extensions"
)

// ensure ExtensionRequestBreaker implements the extensions.RequestInterceptor interface
var _ extensions.RequestInterceptor = (*ExtensionRequestBreaker)(nil)

type ExtensionRequestBreaker struct {
	FailureRatio    float64
	WindowInSeconds int

	breaker breaker.Breaker
}

func (b *ExtensionRequestBreaker) Description() string {
	return "http client request breaker extension that enable fail-fast on unavailable endpoints"
}

func (b *ExtensionRequestBreaker) Init(context pipeline.Context) error {
	var ops []breaker.BreakerOption
	ops = append(ops, breaker.WithFailureRatio(b.FailureRatio))
	if b.WindowInSeconds > 0 {
		ops = append(ops, breaker.WithWindow(time.Duration(b.WindowInSeconds)*time.Second))
	}
	b.breaker = breaker.NewBreaker(ops...)
	return nil
}

func (b *ExtensionRequestBreaker) Stop() error {
	return nil
}

func (b *ExtensionRequestBreaker) RoundTripper(base http.RoundTripper) (http.RoundTripper, error) {
	if b.breaker == nil {
		return nil, fmt.Errorf("extension not initialized")
	}
	transport := breaker.Transport(b.breaker, breaker.DefaultResponseValidator, base)
	return transport, nil
}

func init() {
	pipeline.AddExtensionCreator("ext_request_breaker", func() pipeline.Extension {
		return &ExtensionRequestBreaker{
			FailureRatio:    0.10,
			WindowInSeconds: 10,
		}
	})
}
