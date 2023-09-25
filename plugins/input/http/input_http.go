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

package http

import (
	"errors"
	"fmt"
	"io"
	"net"
	"net/http"
	"net/url"
	"regexp"
	"strconv"
	"strings"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/pipeline"
	"github.com/alibaba/ilogtail/pkg/util"
)

// Response sends HTTP request and collects HTTP response metrics.
type Response struct {
	Addresses               []string
	AddressPath             string
	FlushAddressIntervalSec int
	Body                    string
	Method                  string
	ResponseTimeoutMs       int
	PerAddressSleepMs       int
	Headers                 map[string]string
	FollowRedirects         bool
	ResponseStringMatch     string
	IncludeBody             bool

	// Path to CA file
	SSLCA string
	// Path to host cert file
	SSLCert string
	// Path to cert key file
	SSLKey string
	// Use SSL but skip chain & host verification
	InsecureSkipVerify bool

	compiledStringMatch *regexp.Regexp
	client              *http.Client
	context             pipeline.Context
	tags                map[string]string
	lastLoadAddressTime time.Time
}

func (h *Response) loadAddresses() error {
	if h.Addresses == nil {
		h.Addresses = make([]string, 0, 1)
	}
	if len(h.AddressPath) > 0 {
		addresses, err := util.ReadLines(h.AddressPath)
		h.lastLoadAddressTime = time.Now()
		if err != nil {
			return err
		}
		if addresses == nil {
			return fmt.Errorf("no url in this file: %s", h.AddressPath)
		}
		h.Addresses = addresses
	}
	if len(h.Addresses) == 0 {
		h.Addresses = append(h.Addresses, "http://localhost")
	}
	for _, address := range h.Addresses {
		addr, err := url.Parse(address)
		if err != nil {
			return err
		}
		if addr.Scheme != "http" && addr.Scheme != "https" {
			return errors.New("Only http and https are supported")
		}
	}

	return nil
}

func (h *Response) Init(context pipeline.Context) (int, error) {
	h.context = context

	// Set default values
	if h.ResponseTimeoutMs < 1 {
		h.ResponseTimeoutMs = 5000
	}

	if h.ResponseTimeoutMs > 30000 {
		logger.Info(h.context.GetRuntimeContext(), "can't config ResponseTimeoutMs for more than 30s, reset value to 30000. your config is", h.ResponseTimeoutMs)
		h.ResponseTimeoutMs = 30000
	}

	if h.FlushAddressIntervalSec < 1 {
		h.FlushAddressIntervalSec = 60
	}

	if h.PerAddressSleepMs < 1 {
		h.PerAddressSleepMs = 100
	}

	// Check send and expected string
	if h.Method == "" {
		h.Method = "GET"
	}

	err := h.loadAddresses()
	if err != nil {
		return 0, err
	}

	// Prepare data
	h.tags = map[string]string{"_method_": h.Method}
	if h.client == nil {
		client, err := h.createHTTPClient()
		if err != nil {
			return 0, err
		}
		h.client = client
	}

	return 0, nil
}

// Description returns the plugin Description
func (h *Response) Description() string {
	return "HTTP/HTTPS request given an address a method and a timeout"
}

// var sampleConfig = `
//   ## Server address (default http://localhost)
//   # address = "http://localhost"

//   ## Set response_timeout (default 5 seconds)
//   # response_timeout = "5s"

//   ## HTTP Request Method
//   # method = "GET"

//   ## Whether to follow redirects from the server (defaults to false)
//   # follow_redirects = false

//   ## Optional HTTP Request Body
//   # body = '''
//   # {'fake':'data'}
//   # '''

//   ## Optional substring or regex match in body of the response
//   # response_string_match = "\"service_status\": \"up\""
//   # response_string_match = "ok"
//   # response_string_match = "\".*_status\".?:.?\"up\""

//   ## Optional SSL Config
//   # ssl_ca = "/etc/telegraf/ca.pem"
//   # ssl_cert = "/etc/telegraf/cert.pem"
//   # ssl_key = "/etc/telegraf/key.pem"
//   ## Use SSL but skip chain & host verification
//   # insecure_skip_verify = false

//   ## HTTP Request Headers (all values must be strings)
//   # [inputs.http_response.headers]
//   #   Host = "github.com"
// `

// ErrRedirectAttempted indicates that a redirect occurred
var ErrRedirectAttempted = errors.New("redirect")

// createHTTPClient creates an http client which will timeout at the specified
// timeout period and can follow redirects if specified
func (h *Response) createHTTPClient() (*http.Client, error) {
	tlsCfg, err := util.GetTLSConfig(
		h.SSLCert, h.SSLKey, h.SSLCA, h.InsecureSkipVerify)
	if tlsCfg != nil {
		logger.Debug(h.context.GetRuntimeContext(), "init ssl cfg", tlsCfg)
	}
	if err != nil {
		return nil, err
	}
	client := &http.Client{
		Transport: &http.Transport{
			DisableKeepAlives: true,
			TLSClientConfig:   tlsCfg,
		},
		Timeout: time.Duration(h.ResponseTimeoutMs) * time.Millisecond,
	}

	if !h.FollowRedirects {
		client.CheckRedirect = func(req *http.Request, via []*http.Request) error {
			return ErrRedirectAttempted
		}
	}
	return client, nil
}

// httpGather gathers all fields and returns any errors it encounters.
func (h *Response) httpGather(address string) (map[string]string, error) {
	// Prepare fields
	fields := map[string]string{"_address_": address}

	var body io.Reader
	if h.Body != "" {
		body = strings.NewReader(h.Body)
	}
	request, err := http.NewRequest(h.Method, address, body)
	if err != nil {
		return nil, err
	}

	for key, val := range h.Headers {
		if key == "__Host__" {
			request.Host = val
		} else {
			request.Header.Add(key, val)
		}
	}

	// Start Timer
	start := time.Now()
	resp, err := h.client.Do(request)

	if err != nil {
		if netErr, ok := err.(net.Error); ok && netErr.Timeout() {
			fields["_result_"] = "timeout"
			return fields, nil
		}
		fields["_result_"] = "connection_failed"
		if h.FollowRedirects {
			return fields, nil
		}
		if urlError, ok := err.(*url.Error); !ok || urlError.Err != ErrRedirectAttempted {
			return fields, nil
		}
	}
	defer func() {
		_, _ = io.Copy(io.Discard, resp.Body)
		_ = resp.Body.Close()
	}()

	fields["_response_time_ms_"] = strconv.FormatFloat(float64(time.Since(start).Nanoseconds())/1000000., 'f', 3, 32)
	fields["_http_response_code_"] = strconv.Itoa(resp.StatusCode)

	bodyBytes, err := io.ReadAll(resp.Body)
	if err != nil {
		logger.Error(h.context.GetRuntimeContext(), "HTTP_PARSE_ALARM", "Read body of HTTP response failed", err)
		fields["_result_"] = "invalid_body"
		fields["_response_match_"] = "no"
		return fields, nil
	}

	// Check the response for a regex match.
	if h.ResponseStringMatch != "" {

		// Compile once and reuse
		if h.compiledStringMatch == nil {
			h.compiledStringMatch, err = regexp.Compile(h.ResponseStringMatch)
			if err != nil {
				logger.Error(h.context.GetRuntimeContext(), "HTTP_INIT_ALARM", "Compile regular expression faild", h.ResponseStringMatch, "error", err)
				fields["_result_"] = "match_regex_invalid"
				return fields, nil
			}
		}

		if h.compiledStringMatch.Match(bodyBytes) {
			fields["_result_"] = "success"
			fields["_response_match_"] = "yes"
		} else {
			fields["_result_"] = "mismatch"
			fields["_response_match_"] = "no"
		}
	} else {
		fields["_result_"] = "success"
	}

	if h.IncludeBody {
		fields["content"] = string(bodyBytes)
	}

	return fields, nil
}

// Collect gets all metric fields and tags and returns any errors it encounters
func (h *Response) Collect(collector pipeline.Collector) error {
	// should not occur
	if h.tags == nil || h.client == nil {
		return nil
	}

	if len(h.AddressPath) > 0 {
		curTime := time.Now()
		if curTime.Sub(h.lastLoadAddressTime).Seconds() > float64(h.FlushAddressIntervalSec) {
			err := h.loadAddresses()
			if err != nil {
				logger.Warning(h.context.GetRuntimeContext(), "HTTP_LOAD_ADDRESS_ALARM", "load address error, file", h.AddressPath, "error", err)
			}
		}
	}
	logger.Debug(h.context.GetRuntimeContext(), "collect addresses", h.Addresses)
	// Gather data
	for _, address := range h.Addresses {
		fields, err := h.httpGather(address)
		if err != nil {
			logger.Warning(h.context.GetRuntimeContext(), "HTTP_COLLECT_ALARM", "collect error, address", address, "error", err)
		}
		if len(fields) > 0 {
			// Add metrics
			collector.AddData(h.tags, fields)
		}
	}

	return nil
}

func init() {
	pipeline.MetricInputs["metric_http"] = func() pipeline.MetricInput {
		return &Response{}
	}
}
