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
package control

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"net/http"
	"os"
	"strings"

	global_config "github.com/alibaba/ilogtail/pkg/config"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/test/config"
	"github.com/alibaba/ilogtail/test/engine/setup"
	"github.com/alibaba/ilogtail/test/engine/setup/dockercompose"
	"github.com/alibaba/ilogtail/test/engine/setup/subscriber"
)

const iLogtailLocalConfigDir = "/etc/ilogtail/config/local"
const lotailpluginHTTPAddress = "ilogtailC:18689"
const E2EProjectName = "e2e-test-project"
const E2ELogstoreName = "e2e-test-logstore"

func AddLocalConfig(ctx context.Context, configName, c string) (context.Context, error) {
	c = completeConfigWithFlusher(c)
	if setup.Env.GetType() == "docker-compose" {
		// write local file
		if _, err := os.Stat(config.ConfigDir); os.IsNotExist(err) {
			if err = os.MkdirAll(config.ConfigDir, 0750); err != nil {
				return ctx, err
			}
		} else if err != nil {
			return ctx, err
		}
		filePath := fmt.Sprintf("%s/%s.yaml", config.ConfigDir, configName)
		err := os.WriteFile(filePath, []byte(c), 0600)
		if err != nil {
			return ctx, err
		}
	} else {
		command := fmt.Sprintf(`cd %s && cat << 'EOF' > %s.yaml
%s`, iLogtailLocalConfigDir, configName, c)
		if err := setup.Env.ExecOnLogtail(command); err != nil {
			return ctx, err
		}
	}
	return ctx, nil
}

func RemoveAllLocalConfig(ctx context.Context) (context.Context, error) {
	command := fmt.Sprintf("cd %s && rm -rf *.yaml", iLogtailLocalConfigDir)
	if err := setup.Env.ExecOnLogtail(command); err != nil {
		return ctx, err
	}
	return ctx, nil
}

func AddHTTPConfig(ctx context.Context, configName, c string) (context.Context, error) {
	if setup.Env.GetType() == "docker-compose" {
		address := dockercompose.GetPhysicalAddress(lotailpluginHTTPAddress)
		if address == "" {
			return ctx, errors.New("ilogtail export virtual address should be " + lotailpluginHTTPAddress)
		}
		endpointPrefix := "http://" + address
		var loadConfigs []*global_config.LoadedConfig
		loadConfigs = append(loadConfigs, &global_config.LoadedConfig{
			Project:     E2EProjectName,
			Logstore:    E2ELogstoreName,
			ConfigName:  configName,
			LogstoreKey: 1,
			JSONStr:     c,
		})
		cfg, _ := json.Marshal(loadConfigs)
		// load test case configuration
		resp, err := http.Post(endpointPrefix+global_config.EnpointLoadconfig, "application/json", bytes.NewReader(cfg))
		if err != nil {
			return ctx, fmt.Errorf("error when posting the new logtail plugin configuration: %v", err)
		}
		_ = resp.Body.Close()
		if resp.StatusCode != http.StatusOK {
			return ctx, fmt.Errorf("failed load ilogtail configuration, the response code is %d", resp.StatusCode)
		}
	} else {
		return ctx, fmt.Errorf("env is not docker-compose")
	}
	return ctx, nil
}

func RemoveHTTPConfig(ctx context.Context, configName string) (context.Context, error) {
	if setup.Env.GetType() == "docker-compose" {
		address := dockercompose.GetPhysicalAddress(lotailpluginHTTPAddress)
		if address == "" {
			return ctx, errors.New("ilogtail export virtual address should be " + lotailpluginHTTPAddress)
		}
		endpointPrefix := "http://" + address
		resp, err := http.Get(endpointPrefix + "/holdon")
		if err != nil {
			logger.Error(context.Background(), "HOLDON_LOGTAILPLUGIN_ALARM", "err", err)
			return ctx, err
		}
		_ = resp.Body.Close()
		if resp.StatusCode != http.StatusOK {
			logger.Error(context.Background(), "HOLDON_LOGTAILPLUGIN_ALARM", "statusCode", resp.StatusCode)
			return ctx, fmt.Errorf("failed to hold on logtail plugin, the response code is %d", resp.StatusCode)
		}
	} else {
		return ctx, fmt.Errorf("env is not docker-compose")
	}
	return ctx, nil
}

func completeConfigWithFlusher(c string) string {
	if strings.Contains(c, "flushers") {
		return c
	}
	return c + subscriber.TestSubscriber.FlusherConfig()
}
