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

package eventrecorder

import (
	"context"

	corev1 "k8s.io/api/core/v1"
	v1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/runtime"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/kubernetes/scheme"
	typedcorev1 "k8s.io/client-go/kubernetes/typed/core/v1"
	restclient "k8s.io/client-go/rest"
	"k8s.io/client-go/tools/record"
	ref "k8s.io/client-go/tools/reference"

	"github.com/alibaba/ilogtail/pkg/logger"
)

type EventRecorder struct {
	recorder record.EventRecorder
	define   EventDefine
}

var eventRecorder = &EventRecorder{}
var nodeIP string
var nodeName string
var podName string
var podNamespace string

func SetEventRecorder(kubeclientset *kubernetes.Clientset, module string) {
	logger.Info(context.Background(), "Creating event broadcaster")
	eventBroadcaster := record.NewBroadcaster()
	eventBroadcaster.StartRecordingToSink(&typedcorev1.EventSinkImpl{Interface: kubeclientset.CoreV1().Events("")})
	recorder := eventBroadcaster.NewRecorder(scheme.Scheme, corev1.EventSource{Component: Logtail})

	eventRecorder.recorder = recorder
	eventRecorder.define = *NewEventDefine(module)
}

func Init(nodeIPStr, nodeNameStr, podNameStr, podNamespaceStr string) {
	var cfg *restclient.Config

	cfg, err := restclient.InClusterConfig()
	logger.Info(context.Background(), "init event_revorder", "")

	nodeIP = nodeIPStr
	nodeName = nodeNameStr
	podName = podNameStr
	podNamespace = podNamespaceStr
	if err != nil {
		logger.Error(context.Background(), "INIT_ALARM", "Error create EventRecorder: %s", err.Error())
		return
	}
	kubeClient, err := kubernetes.NewForConfig(cfg)
	if err != nil {
		logger.Error(context.Background(), "INIT_ALARM", "Error create EventRecorder: %s", err.Error())
		return
	}
	SetEventRecorder(kubeClient, "logtail")
}

func GetEventRecorder() *EventRecorder {
	if eventRecorder.recorder != nil {
		return eventRecorder
	}
	return nil
}

func (e *EventRecorder) SendNormalEvent(object runtime.Object, action Action, message string) {
	if message == "" {
		message = "success"
	}
	e.recorder.Event(object, corev1.EventTypeNormal, e.define.getInfoAction(action), message)
}

func (e *EventRecorder) SendErrorEvent(object runtime.Object, action Action, alarm Alarm, message string) {
	if message == "" {
		message = "failed"
	}
	if alarm == "" {
		alarm = "Fail"
	}
	e.recorder.Event(object, corev1.EventTypeWarning, e.define.getErrorAction(action, alarm), message)
}

func (e *EventRecorder) SendNormalEventWithAnnotation(object runtime.Object, annotations map[string]string, action Action, message string) {
	if message == "" {
		message = "success"
	}
	if len(nodeIP) > 0 {
		annotations["nodeIP"] = nodeIP
	}
	if len(nodeName) > 0 {
		annotations["nodeName"] = nodeName
	}
	e.recorder.AnnotatedEventf(object, annotations, corev1.EventTypeNormal, e.define.getInfoAction(action), message)
}

func (e *EventRecorder) SendErrorEventWithAnnotation(object runtime.Object, annotations map[string]string, action Action, alarm Alarm, message string) {
	if message == "" {
		message = "failed"
	}
	if alarm == "" {
		alarm = "Fail"
	}
	if len(nodeIP) > 0 {
		annotations["nodeIP"] = nodeIP
	}
	if len(nodeName) > 0 {
		annotations["nodeName"] = nodeName
	}
	if alarm == "" {
		alarm = CommonAlarm
	}
	e.recorder.AnnotatedEventf(object, annotations, corev1.EventTypeWarning, e.define.getErrorAction(action, alarm), message)
}

func (e *EventRecorder) GetObject() runtime.Object {
	currPodName := "logtail-ds"
	if len(podName) > 0 {
		currPodName = podName
	} else if len(nodeIP) > 0 {
		currPodName = currPodName + "-" + nodeIP
	}

	currPodNamespace := "kube-system"
	if len(podNamespace) > 0 {
		currPodNamespace = podNamespace
	}

	podInfo := &v1.Pod{
		ObjectMeta: metav1.ObjectMeta{
			Name:      currPodName,
			Namespace: currPodNamespace,
		},
	}

	ref, err := ref.GetReference(scheme.Scheme, podInfo)
	if err == nil {
		return ref
	}
	return nil
}
