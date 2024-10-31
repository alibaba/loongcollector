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
	"context"
	"encoding/json"
	"fmt"

	"github.com/alibaba/ilogtail/test/engine/setup"
	"github.com/alibaba/ilogtail/test/engine/setup/controller"
)

func AddLabel(ctx context.Context, labelStr string) (context.Context, error) {
	var labels map[string]string
	if err := json.Unmarshal([]byte(labelStr), &labels); err != nil {
		return ctx, err
	}
	filter := controller.ContainerFilter{
		K8sLabel: labels,
	}
	if k8sEnv, ok := setup.Env.(*setup.K8sEnv); ok {
		if err := k8sEnv.AddFilter(filter); err != nil {
			return ctx, err
		}
	} else {
		return ctx, fmt.Errorf("try to add label, but env is not k8s env")
	}
	return ctx, nil
}

func RemoveLabel(ctx context.Context, labelStr string) (context.Context, error) {
	var labels map[string]string
	if err := json.Unmarshal([]byte(labelStr), &labels); err != nil {
		return ctx, err
	}
	filter := controller.ContainerFilter{
		K8sLabel: labels,
	}
	if k8sEnv, ok := setup.Env.(*setup.K8sEnv); ok {
		if err := k8sEnv.RemoveFilter(filter); err != nil {
			return ctx, nil
		}
	} else {
		return ctx, fmt.Errorf("try to remove label, but env is not k8s env")
	}
	return ctx, nil
}

func ApplyYaml(ctx context.Context, yaml string) (context.Context, error) {
	if k8sEnv, ok := setup.Env.(*setup.K8sEnv); ok {
		if err := k8sEnv.Apply(yaml); err != nil {
			return ctx, err
		}
	} else {
		return ctx, fmt.Errorf("try to apply yaml, but env is not k8s env")
	}
	return ctx, nil
}

func DeleteYaml(ctx context.Context, yaml string) (context.Context, error) {
	if k8sEnv, ok := setup.Env.(*setup.K8sEnv); ok {
		if err := k8sEnv.Delete(yaml); err != nil {
			return ctx, err
		}
	} else {
		return ctx, fmt.Errorf("try to delete yaml, but env is not k8s env")
	}
	return ctx, nil
}
