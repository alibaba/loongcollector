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

package containercenter

import (
	"context"
	"fmt"
	"io"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"github.com/moby/moby/api/types/events"
	docker "github.com/moby/moby/client"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func newTestDockerClientAdapter(t *testing.T, handler http.Handler) *dockerClientAdapter {
	t.Helper()

	server := httptest.NewServer(handler)
	t.Cleanup(server.Close)

	client, err := docker.NewClientWithOpts(
		docker.WithHost(server.URL),
		docker.WithAPIVersion("1.54"),
	)
	require.NoError(t, err)
	t.Cleanup(func() {
		require.NoError(t, client.Close())
	})
	return &dockerClientAdapter{client: client}
}

func writeDockerAPIError(w http.ResponseWriter, status int, message string) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(status)
	_, _ = fmt.Fprintf(w, `{"message":%q}`, message)
}

func TestDockerClientAdapterContainerList(t *testing.T) {
	t.Run("success", func(t *testing.T) {
		adapter := newTestDockerClientAdapter(t, http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
			assert.Equal(t, http.MethodGet, r.Method)
			assert.Equal(t, "/v1.54/containers/json", r.URL.Path)
			assert.Equal(t, "1", r.URL.Query().Get("all"))
			w.Header().Set("Content-Type", "application/json")
			_, _ = io.WriteString(w, `[{"Id":"container-1","Names":["/demo"],"Image":"repo/demo:latest","State":"running"}]`)
		}))

		items, err := adapter.ContainerList(context.Background(), true)

		require.NoError(t, err)
		require.Len(t, items, 1)
		assert.Equal(t, "container-1", items[0].ID)
		assert.Equal(t, []string{"/demo"}, items[0].Names)
		assert.Equal(t, "repo/demo:latest", items[0].Image)
		assert.Equal(t, "running", string(items[0].State))
	})

	t.Run("daemon error", func(t *testing.T) {
		adapter := newTestDockerClientAdapter(t, http.HandlerFunc(func(w http.ResponseWriter, _ *http.Request) {
			writeDockerAPIError(w, http.StatusInternalServerError, "list failed")
		}))

		items, err := adapter.ContainerList(context.Background(), false)

		assert.ErrorContains(t, err, "list failed")
		assert.Empty(t, items)
	})
}

func TestDockerClientAdapterContainerInspect(t *testing.T) {
	t.Run("success", func(t *testing.T) {
		adapter := newTestDockerClientAdapter(t, http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
			assert.Equal(t, "/v1.54/containers/container-1/json", r.URL.Path)
			w.Header().Set("Content-Type", "application/json")
			_, _ = io.WriteString(w, `{
				"Id":"container-1",
				"Name":"/demo",
				"Image":"sha256:image-1",
				"State":{"Status":"running","Pid":321},
				"Config":{"Image":"repo/demo:latest","Env":["A=B"]}
			}`)
		}))

		info, err := adapter.ContainerInspect(context.Background(), "container-1")

		require.NoError(t, err)
		assert.Equal(t, "container-1", info.ID)
		assert.Equal(t, "/demo", info.Name)
		require.NotNil(t, info.State)
		assert.Equal(t, 321, info.State.Pid)
		require.NotNil(t, info.Config)
		assert.Equal(t, []string{"A=B"}, info.Config.Env)
	})

	t.Run("daemon error", func(t *testing.T) {
		adapter := newTestDockerClientAdapter(t, http.HandlerFunc(func(w http.ResponseWriter, _ *http.Request) {
			writeDockerAPIError(w, http.StatusNotFound, "container missing")
		}))

		info, err := adapter.ContainerInspect(context.Background(), "missing")

		assert.ErrorContains(t, err, "container missing")
		assert.Empty(t, info.ID)
	})
}

func TestDockerClientAdapterImageInspectWithRaw(t *testing.T) {
	t.Run("success", func(t *testing.T) {
		adapter := newTestDockerClientAdapter(t, http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
			assert.Equal(t, "/v1.54/images/sha256:image-1/json", r.URL.Path)
			w.Header().Set("Content-Type", "application/json")
			_, _ = io.WriteString(w, `{"Id":"sha256:image-1","RepoTags":["repo/demo:latest"],"Architecture":"amd64","Os":"linux"}`)
		}))

		info, raw, err := adapter.ImageInspectWithRaw(context.Background(), "sha256:image-1")

		require.NoError(t, err)
		assert.Equal(t, "sha256:image-1", info.ID)
		assert.Equal(t, []string{"repo/demo:latest"}, info.RepoTags)
		assert.Equal(t, "amd64", info.Architecture)
		assert.Nil(t, raw)
	})

	t.Run("empty image id", func(t *testing.T) {
		adapter := newTestDockerClientAdapter(t, http.HandlerFunc(func(http.ResponseWriter, *http.Request) {
			t.Fatal("empty image ID must fail before issuing a request")
		}))

		info, raw, err := adapter.ImageInspectWithRaw(context.Background(), "")

		assert.Error(t, err)
		assert.Empty(t, info.ID)
		assert.Nil(t, raw)
	})

	t.Run("daemon error", func(t *testing.T) {
		adapter := newTestDockerClientAdapter(t, http.HandlerFunc(func(w http.ResponseWriter, _ *http.Request) {
			writeDockerAPIError(w, http.StatusInternalServerError, "image inspect failed")
		}))

		info, raw, err := adapter.ImageInspectWithRaw(context.Background(), "sha256:missing")

		assert.ErrorContains(t, err, "image inspect failed")
		assert.Empty(t, info.ID)
		assert.Nil(t, raw)
	})
}

func TestDockerClientAdapterEvents(t *testing.T) {
	t.Run("success", func(t *testing.T) {
		adapter := newTestDockerClientAdapter(t, http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
			assert.Equal(t, "/v1.54/events", r.URL.Path)
			w.Header().Set("Content-Type", "application/x-ndjson")
			_, _ = io.WriteString(w, `{"Type":"container","Action":"start","Actor":{"ID":"container-1","Attributes":{"name":"demo"}},"time":123}`+"\n")
		}))

		ctx, cancel := context.WithTimeout(context.Background(), time.Second)
		defer cancel()
		messages, errs := adapter.Events(ctx)

		select {
		case message := <-messages:
			assert.Equal(t, events.ContainerEventType, message.Type)
			assert.Equal(t, events.ActionStart, message.Action)
			assert.Equal(t, "container-1", message.Actor.ID)
			assert.Equal(t, "demo", message.Actor.Attributes["name"])
			assert.Equal(t, int64(123), message.Time)
		case <-ctx.Done():
			t.Fatal("timed out waiting for Docker event")
		}

		select {
		case err := <-errs:
			assert.ErrorIs(t, err, io.EOF)
		case <-ctx.Done():
			t.Fatal("timed out waiting for event stream completion")
		}
	})

	t.Run("daemon error", func(t *testing.T) {
		adapter := newTestDockerClientAdapter(t, http.HandlerFunc(func(w http.ResponseWriter, _ *http.Request) {
			writeDockerAPIError(w, http.StatusServiceUnavailable, "events unavailable")
		}))

		ctx, cancel := context.WithTimeout(context.Background(), time.Second)
		defer cancel()
		_, errs := adapter.Events(ctx)

		select {
		case err := <-errs:
			assert.ErrorContains(t, err, "events unavailable")
		case <-ctx.Done():
			t.Fatal("timed out waiting for Docker events error")
		}
	})
}
