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

package nginx

import (
	"io"
	"net"
	"net/http"
	"net/http/httptest"
	"net/url"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
)

func TestGatherURLServerFieldsInContents(t *testing.T) {
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, _ *http.Request) {
		_, _ = io.WriteString(w, "Active connections: 1\nserver accepts handled requests\n 2 3 4\nReading: 5 Writing: 6 Waiting: 7\n")
	}))
	defer server.Close()
	addr, err := url.Parse(server.URL)
	require.NoError(t, err)
	host, port, err := net.SplitHostPort(addr.Host)
	require.NoError(t, err)
	ctx := helper.NewObservePipelineContext(1)
	input := &Nginx{client: server.Client()}

	require.NoError(t, input.gatherURL(addr, ctx.Collector()))

	groups := ctx.Collector().ToArray()
	require.Len(t, groups, 1)
	require.Len(t, groups[0].Events, 1)
	log := groups[0].Events[0].(*models.Log)
	assert.Equal(t, host, log.Contents.Get("_server_"))
	assert.Equal(t, port, log.Contents.Get("_port_"))
	assert.Equal(t, "4", log.Contents.Get("requests"))
	assert.Zero(t, log.Tags.Len())
}
