// Copyright 2024 iLogtail Authors
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

package prometheus

import (
	"errors"
	"time"
)

const (
	defaultTimeout             = 1 * time.Minute
	defaultSeriesLimit         = 1000
	defaultConcurrency         = 2
	defaultMaxConnsPerHost     = 50
	defaultMaxIdleConnsPerHost = 50
	defaultIdleConnTimeout     = 90 * time.Second
	defaultWriteBufferSize     = 64 * 1024
	defaultQueueCapacity       = 1024
)

const (
	headerKeyUserAgent = "User-Agent"
	headerValUserAgent = "oneagent prometheus remote write flusher"

	headerKeyContentType = "Content-Type"
	headerValContentType = "application/x-protobuf"

	headerKeyContentEncoding = "Content-Encoding"
	headerValContentEncoding = "snappy"

	headerKeyPromRemoteWriteVersion = "X-Prometheus-Remote-Write-Version"
	headerValPromRemoteWriteVersion = "0.1.0"
)

var errNoHTTPFlusher = errors.New("no http flusher instance in prometheus flusher instance")
