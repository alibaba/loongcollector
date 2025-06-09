package kubernetesmetav2

import (
	"testing"

	"github.com/stretchr/testify/assert"
	app "k8s.io/api/apps/v1"
	v1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"

	"github.com/alibaba/ilogtail/pkg/helper/k8smeta"
)

func TestProcessPodEntity(t *testing.T) {
	obj := &v1.Pod{
		ObjectMeta: metav1.ObjectMeta{
			Name:      "pod2",
			Namespace: "ns2",
			UID:       "uid2",
		},
		Spec: v1.PodSpec{
			Containers: []v1.Container{
				{
					Image: "k8s.gcr.io/hyperkube2_spec",
				},
				{
					Image: "k8s.gcr.io/hyperkube3_spec",
				},
			},
			InitContainers: []v1.Container{
				{
					Image: "k8s.gcr.io/initfoo_spec",
				},
			},
		},
		Status: v1.PodStatus{
			ContainerStatuses: []v1.ContainerStatus{
				{
					Name:        "container2",
					Image:       "k8s.gcr.io/hyperkube2",
					ImageID:     "docker://sha256:bbb",
					ContainerID: "docker://cd456",
				},
				{
					Name:        "container3",
					Image:       "k8s.gcr.io/hyperkube3",
					ImageID:     "docker://sha256:ccc",
					ContainerID: "docker://ef789",
				},
			},
			InitContainerStatuses: []v1.ContainerStatus{
				{
					Name:        "initContainer",
					Image:       "k8s.gcr.io/initfoo",
					ImageID:     "docker://sha256:wxyz",
					ContainerID: "docker://ef123",
				},
			},
		},
	}
	objWrapper := &k8smeta.ObjectWrapper{
		Raw: obj,
	}
	collector := &metaCollector{
		serviceK8sMeta: &ServiceK8sMeta{
			Interval: 10,
		},
	}
	log := collector.processPodEntity(objWrapper, "create")
	assert.NotNilf(t, log, "log should not be nil")
}

func TestProcessServiceEntity(t *testing.T) {
	obj := &v1.Service{
		ObjectMeta: metav1.ObjectMeta{
			Name:      "pod2",
			Namespace: "ns2",
			UID:       "uid2",
		},
		Spec: v1.ServiceSpec{
			Ports: []v1.ServicePort{
				{
					Name: "port1",
					Port: 80,
				},
				{
					Name: "port2",
					Port: 8080,
				},
			},
		},
	}
	objWrapper := &k8smeta.ObjectWrapper{
		Raw: obj,
	}
	collector := &metaCollector{
		serviceK8sMeta: &ServiceK8sMeta{
			Interval: 10,
		},
	}
	log := collector.processServiceEntity(objWrapper, "create")
	assert.NotNilf(t, log, "log should not be nil")
}

func TestProcessPodReplicasetLink(t *testing.T) {
	pod := &v1.Pod{
		ObjectMeta: metav1.ObjectMeta{
			Name:      "pod2",
			Namespace: "ns2",
			UID:       "uid2",
			OwnerReferences: []metav1.OwnerReference{
				{
					Kind: "ReplicaSet",
					Name: "rs1",
				},
			},
		},
	}
	replicaSet := &app.ReplicaSet{
		ObjectMeta: metav1.ObjectMeta{
			Name:      "rs1",
			Namespace: "ns2",
		},
	}
	objWrapper := &k8smeta.ObjectWrapper{
		Raw: &k8smeta.PodReplicaSet{
			Pod:        pod,
			ReplicaSet: replicaSet,
		},
	}
	collector := &metaCollector{
		serviceK8sMeta: &ServiceK8sMeta{
			Interval: 10,
		},
	}
	log := collector.processPodReplicaSetLink(objWrapper, "create")
	assert.NotNilf(t, log, "log should not be nil")
}

func TestProcessPodReplicasetLinkNoOwner(t *testing.T) {
	obj := &v1.Pod{
		ObjectMeta: metav1.ObjectMeta{
			Name:      "pod2",
			Namespace: "ns2",
			UID:       "uid2",
		},
	}
	objWrapper := &k8smeta.ObjectWrapper{
		Raw: obj,
	}
	collector := &metaCollector{
		serviceK8sMeta: &ServiceK8sMeta{
			Interval: 10,
		},
	}
	log := collector.processPodReplicaSetLink(objWrapper, "create")
	assert.Nilf(t, log, "log should not be nil")
}

func TestProcessPodServiceLink(t *testing.T) {
	obj1 := &v1.Pod{
		ObjectMeta: metav1.ObjectMeta{
			Name:      "pod2",
			Namespace: "ns2",
			UID:       "uid2",
			OwnerReferences: []metav1.OwnerReference{
				{
					Kind: "Service",
					Name: "svc1",
				},
			},
			Labels: map[string]string{
				"app": "nginx",
			},
		},
	}
	obj2 := &v1.Service{
		ObjectMeta: metav1.ObjectMeta{
			Name:      "svc1",
			Namespace: "ns2",
			UID:       "uid1",
		},
		Spec: v1.ServiceSpec{
			Ports: []v1.ServicePort{
				{
					Name: "port1",
					Port: 80,
				},
			},
			Selector: map[string]string{
				"app": "nginx",
			},
		},
	}
	objWrapper := &k8smeta.ObjectWrapper{
		Raw: &k8smeta.PodService{
			Pod:     obj1,
			Service: obj2,
		},
	}
	collector := &metaCollector{
		serviceK8sMeta: &ServiceK8sMeta{
			Interval: 10,
		},
	}
	log := collector.processPodServiceLink(objWrapper, "create")
	assert.NotNilf(t, log, "log should not be nil")
}

func TestProcessEmptyPod(t *testing.T) {
	data := k8smeta.ObjectWrapper{
		Raw: &v1.Pod{},
	}
	collector := &metaCollector{
		serviceK8sMeta: &ServiceK8sMeta{
			Interval: 10,
		},
	}
	events := collector.processPodEntity(&data, "Update")
	assert.NotNil(t, events)
	assert.Len(t, events, 1)
}

func TestProcessEmptyService(t *testing.T) {
	data := k8smeta.ObjectWrapper{
		Raw: &v1.Service{},
	}
	collector := &metaCollector{
		serviceK8sMeta: &ServiceK8sMeta{
			Interval: 10,
		},
	}
	events := collector.processServiceEntity(&data, "Update")
	assert.NotNil(t, events)
	assert.Len(t, events, 1)
}

func TestProcessEmptyNode(t *testing.T) {
	data := k8smeta.ObjectWrapper{
		Raw: &v1.Node{},
	}
	collector := &metaCollector{
		serviceK8sMeta: &ServiceK8sMeta{
			Interval: 10,
		},
	}
	events := collector.processNodeEntity(&data, "Update")
	assert.NotNil(t, events)
	assert.Len(t, events, 1)
}

func TestProcessEmptyConfigMap(t *testing.T) {
	data := k8smeta.ObjectWrapper{
		Raw: &v1.ConfigMap{},
	}
	collector := &metaCollector{
		serviceK8sMeta: &ServiceK8sMeta{
			Interval: 10,
		},
	}
	events := collector.processConfigMapEntity(&data, "Update")
	assert.NotNil(t, events)
	assert.Len(t, events, 1)
}

func TestProcessEmptyNamespace(t *testing.T) {
	data := k8smeta.ObjectWrapper{
		Raw: &v1.Namespace{},
	}
	collector := &metaCollector{
		serviceK8sMeta: &ServiceK8sMeta{
			Interval: 10,
		},
	}
	events := collector.processNamespaceEntity(&data, "Update")
	assert.NotNil(t, events)
	assert.Len(t, events, 1)
}

func TestProcessEmptyPersistentVolume(t *testing.T) {
	data := k8smeta.ObjectWrapper{
		Raw: &v1.PersistentVolume{},
	}
	collector := &metaCollector{
		serviceK8sMeta: &ServiceK8sMeta{
			Interval: 10,
		},
	}
	events := collector.processPersistentVolumeEntity(&data, "Update")
	assert.NotNil(t, events)
	assert.Len(t, events, 1)
}

func TestProcessEmptyPersistentVolumeClaim(t *testing.T) {
	data := k8smeta.ObjectWrapper{
		Raw: &v1.PersistentVolumeClaim{},
	}
	collector := &metaCollector{
		serviceK8sMeta: &ServiceK8sMeta{
			Interval: 10,
		},
	}
	events := collector.processPersistentVolumeClaimEntity(&data, "Update")
	assert.NotNil(t, events)
	assert.Len(t, events, 1)
}
