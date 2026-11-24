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

package http

import (
	"io"
	"net/http"
	"net/http/httptest"
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/plugins/test/mock"
)

func TestResponseReadMethodContents(t *testing.T) {
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, req *http.Request) {
		_, _ = io.WriteString(w, req.Method)
	}))
	defer server.Close()
	input := &Response{
		Addresses:         []string{server.URL},
		Method:            http.MethodPost,
		PerAddressSleepMs: 1,
		IncludeBody:       true,
	}
	_, err := input.Init(mock.NewEmptyContext("project", "logstore", "http"))
	require.NoError(t, err)
	ctx := helper.NewObservePipelineContext(1)

	require.NoError(t, input.Read(ctx))

	groups := ctx.Collector().ToArray()
	require.Len(t, groups, 1)
	require.Len(t, groups[0].Events, 1)
	log := groups[0].Events[0].(*models.Log)
	assert.Equal(t, http.MethodPost, log.Contents.Get("_method_"))
	assert.Equal(t, server.URL, log.Contents.Get("_address_"))
	assert.Equal(t, http.MethodPost, string(log.GetBody()))
	assert.Zero(t, log.Tags.Len())
}
