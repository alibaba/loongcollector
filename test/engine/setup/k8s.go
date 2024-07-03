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
package setup

import (
	"bytes"
	"crypto/rand"
	"fmt"
	"math/big"

	corev1 "k8s.io/api/core/v1"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/kubernetes/scheme"
	"k8s.io/client-go/rest"
	"k8s.io/client-go/tools/clientcmd"
	"k8s.io/client-go/tools/remotecommand"

	"github.com/alibaba/ilogtail/test/config"
	"github.com/alibaba/ilogtail/test/engine/setup/controller"
)

type K8sEnv struct {
	deployType           string
	config               *rest.Config
	k8sClient            *kubernetes.Clientset
	deploymentController *controller.DeploymentController
	daemonsetController  *controller.DaemonSetController
}

func NewDaemonSetEnv() *K8sEnv {
	env := &K8sEnv{
		deployType: "daemonset",
	}
	env.init()
	return env
}

func (k *K8sEnv) GetType() string {
	return k.deployType
}

func (k *K8sEnv) ExecOnLogtail(command string) error {
	if k.k8sClient == nil {
		return fmt.Errorf("k8s client init failed")
	}
	pods, err := k.daemonsetController.GetDaemonSetPods("logtail-ds", "kube-system")
	if err != nil {
		return err
	}
	for _, pod := range pods.Items {
		if err := k.execInPod(k.config, pod.Namespace, pod.Name, pod.Spec.Containers[0].Name, []string{"bash", "-c", command}); err != nil {
			return err
		}
	}
	return nil
}

func (k *K8sEnv) ExecOnSource(command string) error {
	if k.k8sClient == nil {
		return fmt.Errorf("k8s client init failed")
	}
	pods, err := k.deploymentController.GetDeploymentPods("e2e-generator", "default")
	if err != nil {
		return err
	}
	randomIndex, err := rand.Int(rand.Reader, big.NewInt(int64(len(pods.Items))))
	if err != nil {
		return err
	}
	pod := pods.Items[randomIndex.Int64()]
	if err := k.execInPod(k.config, pod.Namespace, pod.Name, pod.Spec.Containers[0].Name, []string{"sh", "-c", command}); err != nil {
		return err
	}
	return nil
}

func (k *K8sEnv) AddFilter(filter controller.ContainerFilter) error {
	return k.deploymentController.AddFilter("e2e-generator", filter)
}

func (k *K8sEnv) RemoveFilter(filter controller.ContainerFilter) error {
	return k.deploymentController.RemoveFilter("e2e-generator", filter)
}

func (k *K8sEnv) init() {
	var c *rest.Config
	var err error
	if len(config.TestConfig.KubeConfigPath) == 0 {
		c, err = rest.InClusterConfig()
	} else {
		c, err = clientcmd.BuildConfigFromFlags("", config.TestConfig.KubeConfigPath)
	}
	if err != nil {
		panic(err)
	}
	k8sClient, err := kubernetes.NewForConfig(c)
	if err != nil {
		panic(err)
	}
	k.config = c
	k.k8sClient = k8sClient
	k.deploymentController = controller.NewDeploymentController(k.k8sClient)
	k.daemonsetController = controller.NewDaemonSetController(k.k8sClient)
}

func (k *K8sEnv) execInPod(config *rest.Config, namespace, podName, containerName string, command []string) error {
	req := k.k8sClient.CoreV1().RESTClient().
		Post().
		Resource("pods").
		Name(podName).
		Namespace(namespace).
		SubResource("exec").
		VersionedParams(&corev1.PodExecOptions{
			Container: containerName,
			Command:   command,
			Stdin:     false,
			Stdout:    true,
			Stderr:    true,
			TTY:       false,
		}, scheme.ParameterCodec)
	executor, err := remotecommand.NewSPDYExecutor(config, "POST", req.URL())
	if err != nil {
		return err
	}
	var stdout, stderr bytes.Buffer
	err = executor.Stream(remotecommand.StreamOptions{
		Stdin:  nil,
		Stdout: &stdout,
		Stderr: &stderr,
	})
	if err != nil {
		return err
	}
	return nil
}
