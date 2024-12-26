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
package cleanup

import (
	"context"
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"

	"github.com/alibaba/ilogtail/test/engine/setup"
)

type ChaosStatus struct {
	Code    int                 `json:"code"`
	Success bool                `json:"success"`
	Result  []map[string]string `json:"result"`
}

func DestoryAllChaos(ctx context.Context) (context.Context, error) {
	switch setup.Env.GetType() {
	case "host":
		command := "/opt/chaosblade/blade status --type create --status Success"
		response, err := setup.Env.ExecOnLoongCollector(command)
		if err != nil {
			return ctx, err
		}
		var status ChaosStatus
		if err = json.Unmarshal([]byte(response), &status); err != nil {
			return ctx, err
		}
		for _, result := range status.Result {
			command = "/opt/chaosblade/blade destroy " + result["Uid"]
			if _, err := setup.Env.ExecOnLoongCollector(command); err != nil {
				fmt.Println("Destroy chaos failed: ", err)
			}
		}
	case "daemonset", "deployment":
		k8sEnv := setup.Env.(*setup.K8sEnv)
		chaosDir := filepath.Join("test_cases", "chaos")
		if _, err := os.Stat(chaosDir); os.IsNotExist(err) {
			return ctx, nil
		}
		err := filepath.Walk(chaosDir, func(path string, info os.FileInfo, err error) error {
			if err != nil {
				return err
			}
			if info.IsDir() {
				return nil
			}
			if filepath.Ext(path) != ".yaml" {
				return nil
			}
			return k8sEnv.Delete(path[len("test_cases/"):])
		})
		if err != nil {
			return ctx, err
		}
		// delete chaosDir
		if err = os.RemoveAll(chaosDir); err != nil {
			return ctx, err
		}
	}
	return ctx, nil
}
