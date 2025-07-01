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
	"context"
	"crypto/rand"
	"fmt"
	"math/big"
	"sort"
	"strings"
	"time"

	corev1 "k8s.io/api/core/v1"
	k8sErrors "k8s.io/apimachinery/pkg/api/errors"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	intstr "k8s.io/apimachinery/pkg/util/intstr"
	"k8s.io/apimachinery/pkg/watch"
	"k8s.io/client-go/dynamic"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/kubernetes/scheme"
	"k8s.io/client-go/rest"
	"k8s.io/client-go/tools/clientcmd"
	"k8s.io/client-go/tools/remotecommand"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/test/config"
	"github.com/alibaba/ilogtail/test/engine/setup/controller"
)

type K8sEnv struct {
	deployType           string
	config               *rest.Config
	k8sClient            *kubernetes.Clientset
	deploymentController *controller.DeploymentController
	daemonsetController  *controller.DaemonSetController
	dynamicController    *controller.DynamicController
}

func NewDaemonSetEnv() *K8sEnv {
	env := &K8sEnv{
		deployType: "daemonset",
	}
	env.init()
	return env
}

func NewDeploymentEnv() *K8sEnv {
	env := &K8sEnv{
		deployType: "deployment",
	}
	env.init()
	return env
}

func (k *K8sEnv) GetType() string {
	return k.deployType
}

func (k *K8sEnv) ExecOnLoongCollector(command string) (string, error) {
	if k.k8sClient == nil {
		return "", fmt.Errorf("k8s client init failed")
	}
	var pods *corev1.PodList
	var err error
	if k.deployType == "daemonset" {
		pods, err = k.daemonsetController.GetDaemonSetPods("loongcollector-ds", "kube-system")
		if err != nil {
			return "", err
		}
	} else if k.deployType == "deployment" {
		pods, err = k.deploymentController.GetRunningDeploymentPods("loongcollector-cluster", "kube-system")
		if err != nil {
			return "", err
		}
	}
	results := make([]string, 0)
	sort.Slice(pods.Items, func(i, j int) bool {
		return pods.Items[i].Name < pods.Items[j].Name
	})
	for _, pod := range pods.Items {
		result, err := k.execInPod(k.config, pod.Namespace, pod.Name, pod.Spec.Containers[0].Name, []string{"bash", "-c", command})
		if err != nil {
			return "", err
		}
		results = append(results, result)
	}
	return strings.Join(results, "\n"), nil
}

func (k *K8sEnv) ExecOnSource(ctx context.Context, command string) (string, error) {
	if k.k8sClient == nil {
		return "", fmt.Errorf("k8s client init failed")
	}
	deploymentName := "e2e-generator"
	if ctx.Value(config.CurrentWorkingDeploymentKey) != nil {
		deploymentName = ctx.Value(config.CurrentWorkingDeploymentKey).(string)
	}
	pods, err := k.deploymentController.GetRunningDeploymentPods(deploymentName, "default")
	if err != nil {
		return "", err
	}
	randomIndex, err := rand.Int(rand.Reader, big.NewInt(int64(len(pods.Items))))
	if err != nil {
		return "", err
	}
	pod := pods.Items[randomIndex.Int64()]
	fmt.Println("exec on pod: ", pod.Name, "command: ", command)
	return k.execInPod(k.config, pod.Namespace, pod.Name, pod.Spec.Containers[0].Name, []string{"sh", "-c", command})
}

func (k *K8sEnv) AddFilter(deploymentName string, filter controller.ContainerFilter) error {
	return k.deploymentController.AddFilter(deploymentName, filter)
}

func (k *K8sEnv) RemoveFilter(deploymentName string, filter controller.ContainerFilter) error {
	return k.deploymentController.RemoveFilter(deploymentName, filter)
}

func (k *K8sEnv) Scale(deploymentName string, namespace string, replicas int) error {
	return k.deploymentController.Scale(deploymentName, namespace, replicas)
}

func (k *K8sEnv) Apply(filePath string) error {
	return k.dynamicController.Apply(filePath)
}

func (k *K8sEnv) Delete(filePath string) error {
	return k.dynamicController.Delete(filePath)
}

func SwitchCurrentWorkingDeployment(ctx context.Context, deploymentName string) (context.Context, error) {
	return context.WithValue(ctx, config.CurrentWorkingDeploymentKey, deploymentName), nil
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

	dynamicClient, err := dynamic.NewForConfig(c)
	if err != nil {
		panic(err)
	}
	k.dynamicController = controller.NewDynamicController(dynamicClient, k.k8sClient.Discovery())
}

func (k *K8sEnv) execInPod(config *rest.Config, namespace, podName, containerName string, command []string) (string, error) {
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
		return "", err
	}
	var stdout, stderr bytes.Buffer
	err = executor.Stream(remotecommand.StreamOptions{
		Stdin:  nil,
		Stdout: &stdout,
		Stderr: &stderr,
	})
	if err != nil {
		return "", err
	}
	return stdout.String(), nil
}

func (k *K8sEnv) DeletePod(ctx context.Context, namespace, podName string) (context.Context, error) {
	if namespace == "" {
		return ctx, fmt.Errorf("namespace is empty")
	}
	if podName == "" {
		return ctx, fmt.Errorf("pod name is empty, skip deletion")
	}

	err := k.k8sClient.CoreV1().Pods(namespace).Delete(ctx, podName, metav1.DeleteOptions{})
	if err != nil && !k8sErrors.IsNotFound(err) {
		return ctx, err
	}

	if _, err = k.waitForPodDeleted(ctx, namespace, podName, 120*time.Second); err != nil {
		return ctx, err
	}
	logger.Debugf(ctx, "pod deleted:%s", podName)
	return ctx, nil
}

func (k *K8sEnv) waitForPodDeleted(ctx context.Context, namespace, podName string, timeoutDuration time.Duration) (context.Context, error) {
	// 创建 Context 和 Watcher
	watcher, err := k.k8sClient.CoreV1().Pods(namespace).Watch(ctx, metav1.ListOptions{
		FieldSelector: "metadata.name=" + podName,
	})
	if err != nil {
		return ctx, fmt.Errorf("failed to create watcher for pod %s: %w", podName, err)
	}
	defer watcher.Stop()

	timeout := time.After(timeoutDuration)
	for {
		select {
		case event, ok := <-watcher.ResultChan():
			if !ok {
				return ctx, fmt.Errorf("watcher channel closed unexpectedly")
			}

			if event.Type == watch.Deleted {
				logger.Debugf(ctx, "Pod %s has been deleted", podName)
				return ctx, nil
			}

		case <-timeout:
			return ctx, fmt.Errorf("timeout waiting for pod %s to be deleted after %v", podName, timeoutDuration)
		}
	}
}

func (k *K8sEnv) DeleteSingletonService(ctx context.Context) (context.Context, error) {
	if k.k8sClient == nil {
		return ctx, fmt.Errorf("k8s client init failed")
	}
	const namespace = "kube-system"
	const serviceName = "loongcollector-singleton"

	err := k.k8sClient.CoreV1().Services(namespace).Delete(ctx, serviceName, metav1.DeleteOptions{})
	if err != nil && !k8sErrors.IsNotFound(err) {
		return ctx, err
	}
	logger.Debugf(ctx, "Service deleted: %s in namespace %s", serviceName, namespace)
	return ctx, nil
}

func (k *K8sEnv) CreateSingletonService(ctx context.Context) (context.Context, error) {
	if k.k8sClient == nil {
		return ctx, fmt.Errorf("k8s client init failed")
	}
	const namespace = "kube-system"
	const serviceName = "loongcollector-singleton"
	trafficPolicy := corev1.ServiceInternalTrafficPolicyCluster
	ipFamilyPolicy := corev1.IPFamilyPolicySingleStack

	service := &corev1.Service{
		ObjectMeta: metav1.ObjectMeta{
			Name:      serviceName,
			Namespace: namespace,
		},
		Spec: corev1.ServiceSpec{
			InternalTrafficPolicy: &trafficPolicy,
			IPFamilies: []corev1.IPFamily{
				"IPv4",
			},
			IPFamilyPolicy: &ipFamilyPolicy,
			Ports: []corev1.ServicePort{
				{
					Name:       "singleton-service",
					Port:       8899,
					Protocol:   corev1.ProtocolTCP,
					TargetPort: intstr.FromInt(8899),
				},
			},
			Selector: map[string]string{
				"k8s-app": "loongcollector-singleton",
			},
			Type: corev1.ServiceTypeClusterIP,
		},
	}

	_, err := k.k8sClient.CoreV1().Services(namespace).Create(ctx, service, metav1.CreateOptions{})
	if err != nil {
		return ctx, err
	}
	logger.Debugf(ctx, "Service created: %s in namespace %s", serviceName, namespace)
	return ctx, nil
}
