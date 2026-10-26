package loki

import (
	"net/url"
	"reflect"
	"testing"
	"time"

	"github.com/grafana/loki-client-go/pkg/backoff"
	"github.com/grafana/loki-client-go/pkg/urlutil"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"gopkg.in/yaml.v2"
)

var clientConfig = Config{}

var clientDefaultConfig = (`
url: http://localhost:3100/loki/api/v1/push
`)

var clientCustomConfig = `
url: http://localhost:3100/loki/api/v1/push
backoff_config:
  max_retries: 20
  min_period: 5s
  max_period: 1m
batchwait: 5s
batchsize: 204800
timeout: 5s
`

func Test_Config(t *testing.T) {
	u, err := url.Parse("http://localhost:3100/loki/api/v1/push")
	require.NoError(t, err)
	tests := []struct {
		configValues   string
		expectedConfig Config
	}{
		{
			clientDefaultConfig,
			Config{
				URL: urlutil.URLValue{
					URL: u,
				},
				BackoffConfig: backoff.BackoffConfig{
					MaxBackoff: MaxBackoff,
					MaxRetries: MaxRetries,
					MinBackoff: MinBackoff,
				},
				BatchSize: BatchSize,
				BatchWait: BatchWait,
				Timeout:   Timeout,
			},
		},
		{
			clientCustomConfig,
			Config{
				URL: urlutil.URLValue{
					URL: u,
				},
				BackoffConfig: backoff.BackoffConfig{
					MaxBackoff: 1 * time.Minute,
					MaxRetries: 20,
					MinBackoff: 5 * time.Second,
				},
				BatchSize: 100 * 2048,
				BatchWait: 5 * time.Second,
				Timeout:   5 * time.Second,
			},
		},
	}
	for _, tc := range tests {
		err := yaml.Unmarshal([]byte(tc.configValues), &clientConfig)
		require.NoError(t, err)

		if !reflect.DeepEqual(tc.expectedConfig, clientConfig) {
			t.Errorf("Configs does not match, expected: %v, received: %v", tc.expectedConfig, clientConfig)
		}
	}
}

func TestDefaultConfig(t *testing.T) {
	cfg, err := NewDefaultConfig("http://loki.com")
	assert.Nil(t, err)
	assert.Equal(t, cfg.BatchSize, BatchSize)
	assert.Equal(t, cfg.BatchWait, BatchWait)
	assert.Equal(t, cfg.Timeout, Timeout)
	assert.Equal(t, cfg.BackoffConfig.MaxBackoff, MaxBackoff)
	assert.Equal(t, cfg.BackoffConfig.MinBackoff, MinBackoff)
	assert.Equal(t, cfg.BackoffConfig.MaxRetries, MaxRetries)
	assert.Equal(t, cfg.URL.URL.String(), "http://loki.com")
}
