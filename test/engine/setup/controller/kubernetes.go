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
package controller

import (
	"context"
	"fmt"
	"os"
	"path/filepath"
	"time"

	"github.com/avast/retry-go/v4"
	corev1 "k8s.io/api/core/v1"
	"k8s.io/apimachinery/pkg/api/meta"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/apis/meta/v1/unstructured"
	"k8s.io/apimachinery/pkg/runtime/serializer/yaml"
	"k8s.io/client-go/discovery"
	"k8s.io/client-go/dynamic"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/restmapper"

	"github.com/alibaba/ilogtail/test/config"
)

type ContainerFilter struct {
	K8sLabel       map[string]string
	ContainerLabel map[string]string
	Env            map[string]string
	K8sNamespace   string
	K8sPod         string
	K8sContainer   string
}

type DeploymentController struct {
	k8sClient *kubernetes.Clientset
}

type DaemonSetController struct {
	k8sClient *kubernetes.Clientset
}

type DynamicController struct {
	discoveryClient discovery.DiscoveryInterface
	dynamicClient   dynamic.Interface
}

func NewDeploymentController(k8sClient *kubernetes.Clientset) *DeploymentController {
	return &DeploymentController{k8sClient: k8sClient}
}

func (c *DeploymentController) GetDeploymentPods(deploymentName, deploymentNamespace string) (*corev1.PodList, error) {
	deployment, err := c.k8sClient.AppsV1().Deployments(deploymentNamespace).Get(context.TODO(), deploymentName, metav1.GetOptions{})
	if err != nil {
		return nil, err
	}
	labels := map[string]string{
		"app": deployment.Spec.Template.Labels["app"],
	}
	selector := metav1.FormatLabelSelector(&metav1.LabelSelector{MatchLabels: labels})
	listOptions := metav1.ListOptions{LabelSelector: selector}

	pods, err := c.k8sClient.CoreV1().Pods(deploymentNamespace).List(context.TODO(), listOptions)
	if err != nil {
		return nil, err
	}
	return pods, nil
}

func (c *DeploymentController) GetRunningDeploymentPods(deploymentName, deploymentNamespace string) (*corev1.PodList, error) {
	pods, err := c.GetDeploymentPods(deploymentName, deploymentNamespace)
	if err != nil {
		return nil, err
	}
	runningPods := make([]corev1.Pod, 0)
	for _, pod := range pods.Items {
		if pod.DeletionTimestamp == nil {
			runningPods = append(runningPods, pod)
		}
	}
	pods.Items = runningPods
	return pods, nil
}

func (c *DeploymentController) AddFilter(deploymentName string, filter ContainerFilter) error {
	if filter.K8sNamespace == "" {
		filter.K8sNamespace = "default"
	}
	existingDeployment, err := c.k8sClient.AppsV1().Deployments(filter.K8sNamespace).Get(context.TODO(), deploymentName, metav1.GetOptions{})
	if err != nil {
		return err
	}
	// K8s label
	if filter.K8sLabel != nil {
		for k, v := range filter.K8sLabel {
			existingDeployment.Spec.Template.Labels[k] = v
		}
	}
	// env
	if filter.Env != nil {
		for k, v := range filter.Env {
			existingDeployment.Spec.Template.Spec.Containers[0].Env = append(existingDeployment.Spec.Template.Spec.Containers[0].Env, corev1.EnvVar{Name: k, Value: v})
		}
	}
	// K8s container
	if filter.K8sContainer != "" {
		existingDeployment.Spec.Template.Spec.Containers[0].Name = filter.K8sContainer
	}
	_, err = c.k8sClient.AppsV1().Deployments(filter.K8sNamespace).Update(context.TODO(), existingDeployment, metav1.UpdateOptions{})
	if err != nil {
		return err
	}
	return c.waitDeploymentAvailable(deploymentName, filter.K8sNamespace)
}

func (c *DeploymentController) RemoveFilter(deploymentName string, filter ContainerFilter) error {
	if filter.K8sNamespace == "" {
		filter.K8sNamespace = "default"
	}
	existingDeployment, err := c.k8sClient.AppsV1().Deployments(filter.K8sNamespace).Get(context.TODO(), deploymentName, metav1.GetOptions{})
	if err != nil {
		return err
	}
	// K8s label
	if filter.K8sLabel != nil {
		for k := range filter.K8sLabel {
			delete(existingDeployment.Spec.Template.Labels, k)
		}
	}
	// env
	if filter.Env != nil {
		for k := range filter.Env {
			for i, v := range existingDeployment.Spec.Template.Spec.Containers[0].Env {
				if v.Name == k {
					existingDeployment.Spec.Template.Spec.Containers[0].Env = append(existingDeployment.Spec.Template.Spec.Containers[0].Env[:i], existingDeployment.Spec.Template.Spec.Containers[0].Env[i+1:]...)
					break
				}
			}
		}
	}
	_, err = c.k8sClient.AppsV1().Deployments(filter.K8sNamespace).Update(context.TODO(), existingDeployment, metav1.UpdateOptions{})
	if err != nil {
		return err
	}
	return c.waitDeploymentAvailable(deploymentName, filter.K8sNamespace)
}

func (c *DeploymentController) waitDeploymentAvailable(deploymentName, deploymentNamespace string) error {
	timeoutCtx, cancel := context.WithTimeout(context.TODO(), config.TestConfig.RetryTimeout)
	defer cancel()
	return retry.Do(
		func() error {
			deployment, err := c.k8sClient.AppsV1().Deployments(deploymentNamespace).Get(context.TODO(), deploymentName, metav1.GetOptions{})
			if err != nil {
				return err
			}
			pods, err := c.GetDeploymentPods(deploymentName, deploymentNamespace)
			if err != nil {
				return err
			}
			if !(deployment.Status.AvailableReplicas == *deployment.Spec.Replicas && pods != nil && len(pods.Items) == int(*deployment.Spec.Replicas)) {
				return fmt.Errorf("deployment %s/%s not available yet", deploymentNamespace, deploymentName)
			}
			for _, pod := range pods.Items {
				deployment.Spec.Template.Labels["pod-template-hash"] = pod.Labels["pod-template-hash"]
				fmt.Println(pod.Name, pod.Labels, deployment.Spec.Template.Labels)
				if len(deployment.Spec.Template.Labels) != len(pod.Labels) {
					return fmt.Errorf("pod %s/%s not match labels", pod.Namespace, pod.Name)
				}
				if pod.Status.Phase != corev1.PodRunning {
					return fmt.Errorf("pod %s/%s not running yet", pod.Namespace, pod.Name)
				}
				for label, value := range deployment.Spec.Template.Labels {
					if v, ok := pod.Labels[label]; !ok || v != value {
						return fmt.Errorf("pod %s/%s not match label %s=%s", pod.Namespace, pod.Name, label, value)
					}
				}
			}
			return nil
		},
		retry.Context(timeoutCtx),
		retry.Delay(5*time.Second),
		retry.DelayType(retry.FixedDelay),
	)
}

func (c *DeploymentController) DeleteDeployment(deploymentName, deploymentNamespace string) error {
	return c.k8sClient.AppsV1().Deployments(deploymentNamespace).Delete(context.TODO(), deploymentName, metav1.DeleteOptions{})
}

func NewDaemonSetController(k8sClient *kubernetes.Clientset) *DaemonSetController {
	return &DaemonSetController{k8sClient: k8sClient}
}

func (c *DaemonSetController) GetDaemonSetPods(dsName, dsNamespace string) (*corev1.PodList, error) {
	daemonSet, err := c.k8sClient.AppsV1().DaemonSets(dsNamespace).Get(context.TODO(), dsName, metav1.GetOptions{})
	if err != nil {
		return nil, err
	}
	selector := metav1.FormatLabelSelector(daemonSet.Spec.Selector)
	listOptions := metav1.ListOptions{LabelSelector: selector}

	pods, err := c.k8sClient.CoreV1().Pods(dsNamespace).List(context.TODO(), listOptions)
	if err != nil {
		return nil, err
	}
	return pods, nil
}

func NewDynamicController(dynamicClient dynamic.Interface, discoveryClient discovery.DiscoveryInterface) *DynamicController {
	return &DynamicController{dynamicClient: dynamicClient, discoveryClient: discoveryClient}
}

func (c *DynamicController) Apply(filePath string) error {
	// Parse the object from the YAML file
	mapping, obj, err := c.parseObjFromYaml(filePath)
	if err != nil {
		return err
	}

	// Apply the object to the Kubernetes cluster
	resourceInterface := c.dynamicClient.Resource(mapping.Resource)
	if oldObj, err := resourceInterface.Get(context.TODO(), obj.GetName(), metav1.GetOptions{}); err != nil {
		if !meta.IsNoMatchError(err) {
			return err
		}
		// Object does not exist, create it
		if _, err := resourceInterface.Create(context.TODO(), obj, metav1.CreateOptions{}); err != nil {
			return err
		}
	} else {
		// Object exists, update it
		obj.SetResourceVersion(oldObj.GetResourceVersion())
		if _, err := resourceInterface.Update(context.TODO(), obj, metav1.UpdateOptions{}); err != nil {
			return err
		}
	}
	return nil
}

func (c *DynamicController) Delete(filePath string) error {
	// Parse the object from the YAML file
	mapping, obj, err := c.parseObjFromYaml(filePath)
	if err != nil {
		return err
	}

	// Delete the object from the Kubernetes cluster
	resourceInterface := c.dynamicClient.Resource(mapping.Resource)
	if err := resourceInterface.Delete(context.TODO(), obj.GetName(), metav1.DeleteOptions{}); err != nil {
		return err
	}
	return nil
}

func (c *DynamicController) parseObjFromYaml(filePath string) (*meta.RESTMapping, *unstructured.Unstructured, error) {
	// Read the YAML file
	basePath := "test_cases"
	yamlFile, err := os.ReadFile(filepath.Join(basePath, filePath)) // #nosec G304
	if err != nil {
		return nil, nil, err
	}

	// Decode the YAML file into an unstructured.Unstructured object
	decUnstructured := yaml.NewDecodingSerializer(unstructured.UnstructuredJSONScheme)
	obj := &unstructured.Unstructured{}
	_, gvk, err := decUnstructured.Decode(yamlFile, nil, obj)
	if err != nil {
		return nil, nil, err
	}

	apiGroupResources, err := restmapper.GetAPIGroupResources(c.discoveryClient)
	if err != nil {
		return nil, nil, err
	}
	restMapper := restmapper.NewDiscoveryRESTMapper(apiGroupResources)
	mapping, err := restMapper.RESTMapping(gvk.GroupKind(), gvk.Version)
	if err != nil {
		return nil, nil, err
	}

	return mapping, obj, nil
}
