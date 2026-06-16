package k8smeta

import (
	"testing"

	"github.com/stretchr/testify/assert"
	app "k8s.io/api/apps/v1"
	v1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
)

func basePod() *v1.Pod {
	return &v1.Pod{
		ObjectMeta: metav1.ObjectMeta{
			Name:       "test-pod",
			Namespace:  "default",
			Generation: 1,
			Labels:     map[string]string{"app": "web"},
		},
		Status: v1.PodStatus{
			Phase:  v1.PodRunning,
			PodIP:  "10.0.0.1",
			HostIP: "192.168.1.1",
			ContainerStatuses: []v1.ContainerStatus{
				{ContainerID: "docker://abc123"},
			},
			Conditions: []v1.PodCondition{
				{Type: v1.PodReady, Status: v1.ConditionTrue},
			},
		},
	}
}

func TestPod_ConditionsOnlyChange_Filtered(t *testing.T) {
	oldPod := basePod()
	newPod := basePod()
	newPod.Status.Conditions = []v1.PodCondition{
		{Type: v1.PodReady, Status: v1.ConditionFalse},
	}
	assert.False(t, isPodSignificantChange(oldPod, newPod))
}

func TestPod_PhaseChange_Significant(t *testing.T) {
	oldPod := basePod()
	newPod := basePod()
	newPod.Status.Phase = v1.PodSucceeded
	assert.True(t, isPodSignificantChange(oldPod, newPod))
}

func TestPod_PodIPChange_Significant(t *testing.T) {
	oldPod := basePod()
	newPod := basePod()
	newPod.Status.PodIP = "10.0.0.2"
	assert.True(t, isPodSignificantChange(oldPod, newPod))
}

func TestPod_HostIPChange_Significant(t *testing.T) {
	oldPod := basePod()
	newPod := basePod()
	newPod.Status.HostIP = "192.168.1.2"
	assert.True(t, isPodSignificantChange(oldPod, newPod))
}

func TestPod_ContainerIDChange_Significant(t *testing.T) {
	oldPod := basePod()
	newPod := basePod()
	newPod.Status.ContainerStatuses = []v1.ContainerStatus{
		{ContainerID: "docker://def456"},
	}
	assert.True(t, isPodSignificantChange(oldPod, newPod))
}

func TestPod_ContainerAdded_Significant(t *testing.T) {
	oldPod := basePod()
	newPod := basePod()
	newPod.Status.ContainerStatuses = append(newPod.Status.ContainerStatuses,
		v1.ContainerStatus{ContainerID: "docker://new789"})
	assert.True(t, isPodSignificantChange(oldPod, newPod))
}

func TestPod_LabelChange_Significant(t *testing.T) {
	oldPod := basePod()
	newPod := basePod()
	newPod.Labels = map[string]string{"app": "api"}
	assert.True(t, isPodSignificantChange(oldPod, newPod))
}

func TestPod_AnnotationChange_Significant(t *testing.T) {
	oldPod := basePod()
	newPod := basePod()
	newPod.Annotations = map[string]string{"note": "updated"}
	assert.True(t, isPodSignificantChange(oldPod, newPod))
}

func TestPod_GenerationChange_Significant(t *testing.T) {
	oldPod := basePod()
	newPod := basePod()
	newPod.Generation = 2
	assert.True(t, isPodSignificantChange(oldPod, newPod))
}

func TestPod_ContainerStateOnlyChange_Filtered(t *testing.T) {
	oldPod := basePod()
	newPod := basePod()
	newPod.Status.ContainerStatuses = []v1.ContainerStatus{
		{
			ContainerID:  "docker://abc123",
			RestartCount: 5,
			Ready:        false,
		},
	}
	assert.False(t, isPodSignificantChange(oldPod, newPod))
}

func TestPod_NonPodType_Significant(t *testing.T) {
	assert.True(t, isPodSignificantChange("not-a-pod", "not-a-pod"))
}

func TestPod_Identical_Filtered(t *testing.T) {
	pod := basePod()
	assert.False(t, isPodSignificantChange(pod, pod))
}

func baseService() *v1.Service {
	return &v1.Service{
		ObjectMeta: metav1.ObjectMeta{
			Name:       "test-svc",
			Namespace:  "default",
			Generation: 1,
			Labels:     map[string]string{"app": "web"},
		},
	}
}

func TestMetadataOnly_GenerationChange_Significant(t *testing.T) {
	oldSvc := baseService()
	newSvc := baseService()
	newSvc.Generation = 2
	assert.True(t, isMetadataOnlySignificantChange(oldSvc, newSvc))
}

func TestMetadataOnly_LabelChange_Significant(t *testing.T) {
	oldSvc := baseService()
	newSvc := baseService()
	newSvc.Labels = map[string]string{"app": "api"}
	assert.True(t, isMetadataOnlySignificantChange(oldSvc, newSvc))
}

func TestMetadataOnly_AnnotationChange_Significant(t *testing.T) {
	oldSvc := baseService()
	newSvc := baseService()
	newSvc.Annotations = map[string]string{"note": "updated"}
	assert.True(t, isMetadataOnlySignificantChange(oldSvc, newSvc))
}

func TestMetadataOnly_StatusOnlyChange_Filtered(t *testing.T) {
	oldSvc := baseService()
	newSvc := baseService()
	newSvc.Status = v1.ServiceStatus{
		LoadBalancer: v1.LoadBalancerStatus{
			Ingress: []v1.LoadBalancerIngress{{IP: "1.2.3.4"}},
		},
	}
	assert.False(t, isMetadataOnlySignificantChange(oldSvc, newSvc))
}

func TestMetadataOnly_Identical_Filtered(t *testing.T) {
	svc := baseService()
	assert.False(t, isMetadataOnlySignificantChange(svc, svc))
}

func TestIsSignificantUpdate_Pod(t *testing.T) {
	oldPod := basePod()
	newPod := basePod()
	newPod.Status.Conditions = []v1.PodCondition{}
	assert.False(t, isSignificantUpdate(POD, oldPod, newPod))

	newPod2 := basePod()
	newPod2.Status.Phase = v1.PodFailed
	assert.True(t, isSignificantUpdate(POD, oldPod, newPod2))
}

func TestIsSignificantUpdate_Service(t *testing.T) {
	oldSvc := baseService()
	newSvc := baseService()
	assert.False(t, isSignificantUpdate(SERVICE, oldSvc, newSvc))

	newSvc2 := baseService()
	newSvc2.Generation = 2
	assert.True(t, isSignificantUpdate(SERVICE, oldSvc, newSvc2))
}

func TestIsSignificantUpdate_NonStatusResources(t *testing.T) {
	types := []string{DAEMONSET, STATEFULSET, REPLICASET, CONFIGMAP, NAMESPACE, CRONJOB, INGRESS, STORAGECLASS}
	for _, rt := range types {
		oldSvc := baseService()
		newSvc := baseService()
		assert.False(t, isSignificantUpdate(rt, oldSvc, newSvc), "expected filtered for %s", rt)
	}
}

func TestIsSignificantUpdate_StatusResources_AlwaysSignificant(t *testing.T) {
	types := []string{NODE, DEPLOYMENT, JOB, PERSISTENTVOLUME, PERSISTENTVOLUMECLAIM}
	svc := baseService()
	for _, rt := range types {
		assert.True(t, isSignificantUpdate(rt, svc, svc), "expected significant for %s", rt)
	}
}

func TestSameContainerIDs_Empty(t *testing.T) {
	assert.True(t, sameContainerIDs(nil, nil))
	assert.True(t, sameContainerIDs([]v1.ContainerStatus{}, []v1.ContainerStatus{}))
	assert.False(t, sameContainerIDs(nil, []v1.ContainerStatus{{ContainerID: "a"}}))
}

func TestMetadataOnly_GenerationChange_DaemonSet(t *testing.T) {
	oldDS := &app.DaemonSet{
		ObjectMeta: metav1.ObjectMeta{Generation: 1, Labels: map[string]string{"a": "b"}},
	}
	newDS := &app.DaemonSet{
		ObjectMeta: metav1.ObjectMeta{Generation: 2, Labels: map[string]string{"a": "b"}},
	}
	assert.True(t, isMetadataOnlySignificantChange(oldDS, newDS))
}
