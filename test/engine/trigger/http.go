// Copyright 2024 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//	http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
package trigger

import (
	"context"
	"errors"
	"fmt"
	"net/http"
	"strings"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/test/engine/setup/dockercompose"
)

func HTTP(ctx context.Context, count, interval int, url, method, body string) (context.Context, error) {
	if !strings.HasPrefix(url, "http://") {
		return ctx, errors.New("the trigger URL must start with `http://`")
	}
	var virtualAddr string
	var path string
	index := strings.Index(url[7:], "/")
	if index == -1 {
		virtualAddr = url[7:]
	} else {
		virtualAddr = url[7:][:index]
		path = url[7+index:]
	}
	if !strings.Contains(virtualAddr, ":") {
		virtualAddr += ":80"
	}
	physicalAddress := dockercompose.GetPhysicalAddress(virtualAddr)
	if physicalAddress == "" {
		return ctx, fmt.Errorf("cannot find the physical address of the %s virtual address in the exported ports", virtualAddr)
	}
	realAddress := "http://" + physicalAddress + path

	request, err := http.NewRequest(method, realAddress, strings.NewReader(body))
	if err != nil {
		return ctx, fmt.Errorf("error in creating http trigger request: %v", err)
	}
	client := &http.Client{}
	for i := 0; i < count; i++ {
		logger.Infof(context.Background(), "request URL %s the %d time", request.URL, i)
		resp, err := client.Do(request)
		if err != nil {
			logger.Debugf(context.Background(), "error in triggering the http request: %s, err: %s", request.URL, err)
			continue
		}
		_ = resp.Body.Close()
		logger.Debugf(context.Background(), "request URL %s got response code: %d", request.URL, resp.StatusCode)
		if resp.StatusCode == http.StatusOK {
			logger.Debugf(context.Background(), "request URL %s success", request.URL)
		} else {
			logger.Debugf(context.Background(), "request URL %s failed", request.URL)
		}
		time.Sleep(time.Duration(interval) * time.Millisecond)
	}
	return ctx, nil
}
