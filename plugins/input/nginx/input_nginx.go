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

package nginx

import (
	"bufio"
	"fmt"
	"net"
	"net/http"
	"net/url"
	"strings"
	"sync"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/util"
)

type Nginx struct {
	// List of status URLs
	Urls []string
	// Path to CA file
	SSLCA string `toml:"ssl_ca"`
	// Path to client cert file
	SSLCert string `toml:"ssl_cert"`
	// Path to cert key file
	SSLKey string `toml:"ssl_key"`
	// Use SSL but skip chain & host verification
	SkipInsecureVerify bool
	// Response timeout
	ResponseTimeoutMs int32

	// HTTP client
	client  *http.Client
	context pipeline.Context
}

func (n *Nginx) Init(context pipeline.Context) (int, error) {
	n.context = context
	if n.ResponseTimeoutMs <= 100 {
		n.ResponseTimeoutMs = 5000
	}
	logger.Debug(n.context.GetRuntimeContext(), "nginx load config", n.context.GetConfigName())
	return 0, nil
}

func (n *Nginx) Description() string {
	return "Read Nginx's basic status information (ngx_http_stub_status_module)"
}

func (n *Nginx) Collect(collector pipeline.Collector) error {
	var wg sync.WaitGroup
	logger.Debug(n.context.GetRuntimeContext(), "start collect nginx info", *n)
	// Create an HTTP client that is re-used for each
	// collection interval
	if n.client == nil {
		client, err := n.createHTTPClient()
		if err != nil {
			return err
		}
		n.client = client
	}

	for _, u := range n.Urls {
		addr, err := url.Parse(u)
		if err != nil {
			logger.Error(n.context.GetRuntimeContext(), "NGINX_STATUS_INIT_ALARM", "Unable to parse address", u, "error", err)
		}

		wg.Add(1)
		go func(addr *url.URL) {
			defer wg.Done()
			err := n.gatherURL(addr, collector)
			if err != nil {
				logger.Error(n.context.GetRuntimeContext(), "NGINX_STATUS_COLLECT_ALARM", "url", addr.Host, "error", err)
			}
		}(addr)
	}

	wg.Wait()
	return nil
}

func (n *Nginx) createHTTPClient() (*http.Client, error) {
	tlsCfg, err := util.GetTLSConfig(
		n.SSLCert, n.SSLKey, n.SSLCA, n.SkipInsecureVerify)
	if err != nil {
		return nil, err
	}

	client := &http.Client{
		Transport: &http.Transport{
			TLSClientConfig: tlsCfg,
		},
		Timeout: time.Duration(n.ResponseTimeoutMs) * time.Millisecond,
	}

	return client, nil
}

func (n *Nginx) gatherURL(addr *url.URL, collector pipeline.Collector) error {
	resp, err := n.client.Get(addr.String())
	if err != nil {
		return fmt.Errorf("error making HTTP request to %s: %s", addr.String(), err)
	}
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusOK {
		return fmt.Errorf("%s returned HTTP status %s", addr.String(), resp.Status)
	}
	r := bufio.NewReader(resp.Body)

	// Active connections
	_, err = r.ReadString(':')
	if err != nil {
		return err
	}
	line, err := r.ReadString('\n')
	if err != nil {
		return err
	}
	active := strings.TrimSpace(line)

	// Server accepts handled requests
	_, err = r.ReadString('\n')
	if err != nil {
		return err
	}
	line, err = r.ReadString('\n')
	if err != nil {
		return err
	}
	data := strings.Fields(line)
	accepts := data[0]
	handled := data[1]
	requests := data[2]

	// Reading/Writing/Waiting
	line, err = r.ReadString('\n')
	if err != nil {
		return err
	}
	data = strings.Fields(line)
	reading := data[1]
	writing := data[3]
	waiting := data[5]

	tags := getTags(addr)
	fields := map[string]string{
		"active":   active,
		"accepts":  accepts,
		"handled":  handled,
		"requests": requests,
		"reading":  reading,
		"writing":  writing,
		"waiting":  waiting,
	}
	collector.AddData(tags, fields)
	return nil
}

// Get tag(s) for the nginx plugin
func getTags(addr *url.URL) map[string]string {
	h := addr.Host
	host, port, err := net.SplitHostPort(h)
	if err != nil {
		host = addr.Host
		switch {
		case addr.Scheme == "http":
			port = "80"
		case addr.Scheme == "https":
			port = "443"
		default:
			port = ""
		}
	}
	return map[string]string{"_server_": host, "_port_": port}
}

func init() {
	pipeline.MetricInputs["metric_nginx_status"] = func() pipeline.MetricInput {
		return &Nginx{}
	}
}
