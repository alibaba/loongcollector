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
package chaos

import (
	"context"
	"os"
	"path/filepath"
	"strconv"
	"text/template"

	"github.com/alibaba/ilogtail/test/engine/setup"
)

const (
	// networkDelayCRD
	networkDelayCRDTmpl = `
apiVersion: chaosblade.io/v1alpha1
kind: ChaosBlade
metadata:
  name: delay-pod-network
spec:
  experiments:
    - scope: pod
      target: network
      action: delay
      desc: "delay pod network"
      matchers:
        - name: labels
          value: ["{{.PodLabel}}"]
        - name: namespace
          value: ["kube-system"]
        - name: interface
          value: ["eth0"]
        - name: destination-ip
          value: ["{{.Percent}}"]
        - name: time
          value: ["{{.Time}}"]
`

	// networkLossCRD
	networkLossCRDTmpl = `
apiVersion: chaosblade.io/v1alpha1
kind: ChaosBlade
metadata:
  name: loss-pod-network
spec:
  experiments:
    - scope: pod
      target: network
      action: loss
      desc: "loss pod network"
      matchers:
        - name: labels
          value: ["{{.PodLabel}}"]
        - name: namespace
          value: ["kube-system"]
        - name: interface
          value: ["eth0"]
        - name: percent
          value: ["{{.Percent}}"]
        - name: exclude-port
          value: ["22"]
        - name: destination-ip
          value: ["{{.Ip}}"]
`
)

func NetworkDelay(ctx context.Context, time int, ip string) (context.Context, error) {
	switch setup.Env.GetType() {
	case "host":
		command := "/opt/chaosblade/blade create network delay --time " + strconv.FormatInt(int64(time), 10) + " --exclude-port 22 --interface eth0 --destination-ip " + ip
		_, err := setup.Env.ExecOnLogtail(command)
		if err != nil {
			return ctx, err
		}
	case "daemonset", "deployment":
		dir := filepath.Join("test_cases", "chaos")
		filename := "loss-pod-network.yaml"
		_ = os.Mkdir(dir, 0750)
		file, err := os.OpenFile(filepath.Join(dir, filename), os.O_WRONLY|os.O_CREATE|os.O_TRUNC, 0644) //nolint:gosec
		if err != nil {
			return ctx, err
		}
		defer file.Close() //nolint:gosec

		networkDelayCRD, _ := template.New("networkDelay").Parse(networkDelayCRDTmpl)
		if err = networkDelayCRD.Execute(file, map[string]string{
			"PodLabel": getLoongCollectorPodLabel(),
			"Time":     strconv.FormatInt(int64(time), 10),
			"Ip":       ip,
		}); err != nil {
			return ctx, err
		}
		k8sEnv := setup.Env.(*setup.K8sEnv)
		if err := k8sEnv.Apply(filepath.Join("chaos", filename)); err != nil {
			return ctx, err
		}
	}
	return ctx, nil
}

func NetworkLoss(ctx context.Context, percentage int, ip string) (context.Context, error) {
	switch setup.Env.GetType() {
	case "host":
		command := "/opt/chaosblade/blade create network loss --percent " + strconv.FormatInt(int64(percentage), 10) + " --exclude-port 22 --interface eth0 --destination-ip " + ip
		_, err := setup.Env.ExecOnLogtail(command)
		if err != nil {
			return ctx, err
		}
	case "daemonset", "deployment":
		dir := filepath.Join("test_cases", "chaos")
		filename := "loss-pod-network.yaml"
		_ = os.Mkdir(dir, 0750)
		file, err := os.OpenFile(filepath.Join(dir, filename), os.O_WRONLY|os.O_CREATE|os.O_TRUNC, 0644) //nolint:gosec
		if err != nil {
			return ctx, err
		}
		defer file.Close() //nolint:gosec

		networkLossCRD, _ := template.New("networkLoss").Parse(networkLossCRDTmpl)
		if err = networkLossCRD.Execute(file, map[string]string{
			"PodLabel": getLoongCollectorPodLabel(),
			"Percent":  strconv.FormatInt(int64(percentage), 10),
			"Ip":       ip,
		}); err != nil {
			return ctx, err
		}
		k8sEnv := setup.Env.(*setup.K8sEnv)
		if err := k8sEnv.Apply(filepath.Join("chaos", filename)); err != nil {
			return ctx, err
		}
	}
	return ctx, nil
}

func getLoongCollectorPodLabel() string {
	var PodLabel string
	if setup.Env.GetType() == "daemonset" {
		PodLabel = "k8s-app=logtail-ds"
	} else if setup.Env.GetType() == "deployment" {
		PodLabel = "k8s-app=loongcollector-cluster"
	}
	return PodLabel
}
