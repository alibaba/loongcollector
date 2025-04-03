package k8smeta

import (
	"testing"

	"github.com/stretchr/testify/assert"
	app "k8s.io/api/apps/v1"
	batch "k8s.io/api/batch/v1"
	corev1 "k8s.io/api/core/v1"
	networking "k8s.io/api/networking/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
)

func TestGetPodNodeLink(t *testing.T) {
	podCache := newK8sMetaCache(make(chan struct{}), POD)
	nodeCache := newK8sMetaCache(make(chan struct{}), NODE)
	nodeCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object: &ObjectWrapper{
			Raw: &corev1.Node{
				ObjectMeta: metav1.ObjectMeta{
					Name: "node1",
				},
			},
		},
	})
	nodeCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object: &ObjectWrapper{
			Raw: &corev1.Node{
				ObjectMeta: metav1.ObjectMeta{
					Name: "node2",
				},
			},
		},
	})
	pod1 := generateMockPod("1")
	pod1.Raw.(*corev1.Pod).Spec.NodeName = "node1"
	pod2 := generateMockPod("2")
	pod2.Raw.(*corev1.Pod).Spec.NodeName = "node2"
	podCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    pod1,
	})
	podCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    pod2,
	})
	linkGenerator := NewK8sMetaLinkGenerator(map[string]MetaCache{
		POD:  podCache,
		NODE: nodeCache,
	})
	podList := []*K8sMetaEvent{
		{
			EventType: "update",
			Object:    podCache.metaStore.Items["default/pod1"],
		},
		{
			EventType: "update",
			Object:    podCache.metaStore.Items["default/pod2"],
		},
	}
	results := linkGenerator.getPodNodeLink(podList)
	assert.Equal(t, 2, len(results))
	assert.Equal(t, "node1", results[0].Object.Raw.(*PodNode).Node.Name)
	assert.Equal(t, "node2", results[1].Object.Raw.(*PodNode).Node.Name)
}

func TestGetPodDeploymentLink(t *testing.T) {
	podCache := newK8sMetaCache(make(chan struct{}), POD)
	replicasetCache := newK8sMetaCache(make(chan struct{}), REPLICASET)
	deploymentCache := newK8sMetaCache(make(chan struct{}), DEPLOYMENT)
	deploymentCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object: &ObjectWrapper{
			Raw: &app.Deployment{
				ObjectMeta: metav1.ObjectMeta{
					Name:      "deployment1",
					Namespace: "default",
				},
			},
		},
	})
	deploymentCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object: &ObjectWrapper{
			Raw: &app.Deployment{
				ObjectMeta: metav1.ObjectMeta{
					Name:      "deployment2",
					Namespace: "default",
				},
			},
		},
	})
	replicasetCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object: &ObjectWrapper{
			Raw: &app.ReplicaSet{
				ObjectMeta: metav1.ObjectMeta{
					Name:      "replicaset1",
					Namespace: "default",
					OwnerReferences: []metav1.OwnerReference{
						{
							Kind: "Deployment",
							Name: "deployment1",
						},
					},
				},
				Spec: app.ReplicaSetSpec{
					Selector: &metav1.LabelSelector{
						MatchLabels: map[string]string{
							"app": "test",
						},
					},
				},
			},
		},
	})
	replicasetCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object: &ObjectWrapper{
			Raw: &app.ReplicaSet{
				ObjectMeta: metav1.ObjectMeta{
					Name:      "replicaset2",
					Namespace: "default",
					OwnerReferences: []metav1.OwnerReference{
						{
							Kind: "Deployment",
							Name: "deployment2",
						},
					},
				},
				Spec: app.ReplicaSetSpec{
					Selector: &metav1.LabelSelector{
						MatchLabels: map[string]string{
							"app": "test2",
						},
					},
				},
			},
		},
	})
	pod1 := generateMockPod("1")
	pod1.Raw.(*corev1.Pod).OwnerReferences = []metav1.OwnerReference{
		{
			Kind: "ReplicaSet",
			Name: "replicaset1",
		},
	}
	pod2 := generateMockPod("2")
	pod2.Raw.(*corev1.Pod).OwnerReferences = []metav1.OwnerReference{
		{
			Kind: "ReplicaSet",
			Name: "replicaset2",
		},
	}
	podCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    pod1,
	})
	podCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    pod2,
	})
	linkGenerator := NewK8sMetaLinkGenerator(map[string]MetaCache{
		POD:        podCache,
		REPLICASET: replicasetCache,
		DEPLOYMENT: deploymentCache,
	})
	podList := []*K8sMetaEvent{
		{
			EventType: "update",
			Object:    podCache.metaStore.Items["default/pod1"],
		},
		{
			EventType: "update",
			Object:    podCache.metaStore.Items["default/pod2"],
		},
	}
	results := linkGenerator.getPodDeploymentLink(podList)
	assert.Equal(t, 2, len(results))
	assert.Equal(t, "deployment1", results[0].Object.Raw.(*PodDeployment).Deployment.Name)
	assert.Equal(t, "deployment2", results[1].Object.Raw.(*PodDeployment).Deployment.Name)
}

func TestGetReplicaSetDeploymentLink(t *testing.T) {
	replicasetCache := newK8sMetaCache(make(chan struct{}), REPLICASET)
	deploymentCache := newK8sMetaCache(make(chan struct{}), DEPLOYMENT)
	deploymentCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object: &ObjectWrapper{
			Raw: &app.Deployment{
				ObjectMeta: metav1.ObjectMeta{
					Name:      "deployment1",
					Namespace: "default",
				},
			},
		},
	})
	deploymentCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object: &ObjectWrapper{
			Raw: &app.Deployment{
				ObjectMeta: metav1.ObjectMeta{
					Name:      "deployment2",
					Namespace: "default",
				},
			},
		},
	})
	replicasetCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object: &ObjectWrapper{
			Raw: &app.ReplicaSet{
				ObjectMeta: metav1.ObjectMeta{
					Name:      "replicaset1",
					Namespace: "default",
					OwnerReferences: []metav1.OwnerReference{
						{
							Kind: "Deployment",
							Name: "deployment1",
						},
					},
				},
				Spec: app.ReplicaSetSpec{
					Selector: &metav1.LabelSelector{
						MatchLabels: map[string]string{
							"app": "test",
						},
					},
				},
			},
		},
	})
	replicasetCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object: &ObjectWrapper{
			Raw: &app.ReplicaSet{
				ObjectMeta: metav1.ObjectMeta{
					Name:      "replicaset2",
					Namespace: "default",
					OwnerReferences: []metav1.OwnerReference{
						{
							Kind: "Deployment",
							Name: "deployment2",
						},
					},
				},
				Spec: app.ReplicaSetSpec{
					Selector: &metav1.LabelSelector{
						MatchLabels: map[string]string{
							"app": "test2",
						},
					},
				},
			},
		},
	})
	linkGenerator := NewK8sMetaLinkGenerator(map[string]MetaCache{
		REPLICASET: replicasetCache,
		DEPLOYMENT: deploymentCache,
	})
	replicasetList := []*K8sMetaEvent{
		{
			EventType: "update",
			Object:    replicasetCache.metaStore.Items["default/replicaset1"],
		},
		{
			EventType: "update",
			Object:    replicasetCache.metaStore.Items["default/replicaset2"],
		},
	}
	results := linkGenerator.getReplicaSetDeploymentLink(replicasetList)
	assert.Equal(t, 2, len(results))
	assert.Equal(t, "deployment1", results[0].Object.Raw.(*ReplicaSetDeployment).Deployment.Name)
	assert.Equal(t, "deployment2", results[1].Object.Raw.(*ReplicaSetDeployment).Deployment.Name)
}

func TestGetPodReplicaSetLink(t *testing.T) {
	podCache := newK8sMetaCache(make(chan struct{}), POD)
	replicasetCache := newK8sMetaCache(make(chan struct{}), REPLICASET)
	replicasetCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object: &ObjectWrapper{
			Raw: &app.ReplicaSet{
				ObjectMeta: metav1.ObjectMeta{
					Name:      "replicaset1",
					Namespace: "default",
				},
				Spec: app.ReplicaSetSpec{
					Selector: &metav1.LabelSelector{
						MatchLabels: map[string]string{
							"app": "test",
						},
					},
				},
			},
		},
	})
	replicasetCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object: &ObjectWrapper{
			Raw: &app.ReplicaSet{
				ObjectMeta: metav1.ObjectMeta{
					Name:      "replicaset2",
					Namespace: "default",
				},
				Spec: app.ReplicaSetSpec{
					Selector: &metav1.LabelSelector{
						MatchLabels: map[string]string{
							"app": "test2",
						},
					},
				},
			},
		},
	})
	pod1 := generateMockPod("1")
	pod1.Raw.(*corev1.Pod).OwnerReferences = []metav1.OwnerReference{
		{
			Kind: "ReplicaSet",
			Name: "replicaset1",
		},
	}
	pod2 := generateMockPod("2")
	pod2.Raw.(*corev1.Pod).OwnerReferences = []metav1.OwnerReference{
		{
			Kind: "ReplicaSet",
			Name: "replicaset2",
		},
	}
	podCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    pod1,
	})
	podCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    pod2,
	})
	linkGenerator := NewK8sMetaLinkGenerator(map[string]MetaCache{
		POD:        podCache,
		REPLICASET: replicasetCache,
	})
	podList := []*K8sMetaEvent{
		{
			EventType: "update",
			Object:    podCache.metaStore.Items["default/pod1"],
		},
		{
			EventType: "update",
			Object:    podCache.metaStore.Items["default/pod2"],
		},
	}
	results := linkGenerator.getPodReplicaSetLink(podList)
	assert.Equal(t, 2, len(results))
	assert.Equal(t, "replicaset1", results[0].Object.Raw.(*PodReplicaSet).ReplicaSet.Name)
	assert.Equal(t, "replicaset2", results[1].Object.Raw.(*PodReplicaSet).ReplicaSet.Name)
}

func TestGetPodDaemonSetLink(t *testing.T) {
	podCache := newK8sMetaCache(make(chan struct{}), POD)
	daemonsetCache := newK8sMetaCache(make(chan struct{}), DAEMONSET)
	daemonsetCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object: &ObjectWrapper{
			Raw: &app.DaemonSet{
				ObjectMeta: metav1.ObjectMeta{
					Name:      "daemonset1",
					Namespace: "default",
				},
			},
		},
	})
	daemonsetCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object: &ObjectWrapper{
			Raw: &app.DaemonSet{
				ObjectMeta: metav1.ObjectMeta{
					Name:      "daemonset2",
					Namespace: "default",
				},
			},
		},
	})
	pod1 := generateMockPod("1")
	pod1.Raw.(*corev1.Pod).OwnerReferences = []metav1.OwnerReference{
		{
			Kind: "DaemonSet",
			Name: "daemonset1",
		},
	}
	pod2 := generateMockPod("2")
	pod2.Raw.(*corev1.Pod).OwnerReferences = []metav1.OwnerReference{
		{
			Kind: "DaemonSet",
			Name: "daemonset2",
		},
	}
	podCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    pod1,
	})
	podCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    pod2,
	})
	linkGenerator := NewK8sMetaLinkGenerator(map[string]MetaCache{
		POD:       podCache,
		DAEMONSET: daemonsetCache,
	})
	podList := []*K8sMetaEvent{
		{
			EventType: "update",
			Object:    podCache.metaStore.Items["default/pod1"],
		},
		{
			EventType: "update",
			Object:    podCache.metaStore.Items["default/pod2"],
		},
	}
	results := linkGenerator.getPodDaemonSetLink(podList)
	assert.Equal(t, 2, len(results))
	assert.Equal(t, "daemonset1", results[0].Object.Raw.(*PodDaemonSet).DaemonSet.Name)
	assert.Equal(t, "daemonset2", results[1].Object.Raw.(*PodDaemonSet).DaemonSet.Name)
}

func TestGetPodStatefulSetLink(t *testing.T) {
	podCache := newK8sMetaCache(make(chan struct{}), POD)
	statefulsetCache := newK8sMetaCache(make(chan struct{}), STATEFULSET)
	statefulsetCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object: &ObjectWrapper{
			Raw: &app.StatefulSet{
				ObjectMeta: metav1.ObjectMeta{
					Name:      "statefulset1",
					Namespace: "default",
				},
			},
		},
	})
	statefulsetCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object: &ObjectWrapper{
			Raw: &app.StatefulSet{
				ObjectMeta: metav1.ObjectMeta{
					Name:      "statefulset2",
					Namespace: "default",
				},
			},
		},
	})
	pod1 := generateMockPod("1")
	pod1.Raw.(*corev1.Pod).OwnerReferences = []metav1.OwnerReference{
		{
			Kind: "StatefulSet",
			Name: "statefulset1",
		},
	}
	pod2 := generateMockPod("2")
	pod2.Raw.(*corev1.Pod).OwnerReferences = []metav1.OwnerReference{
		{
			Kind: "StatefulSet",
			Name: "statefulset2",
		},
	}
	podCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    pod1,
	})
	podCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    pod2,
	})
	linkGenerator := NewK8sMetaLinkGenerator(map[string]MetaCache{
		POD:         podCache,
		STATEFULSET: statefulsetCache,
	})
	podList := []*K8sMetaEvent{
		{
			EventType: "update",
			Object:    podCache.metaStore.Items["default/pod1"],
		},
		{
			EventType: "update",
			Object:    podCache.metaStore.Items["default/pod2"],
		},
	}
	results := linkGenerator.getPodStatefulSetLink(podList)
	assert.Equal(t, 2, len(results))
	assert.Equal(t, "statefulset1", results[0].Object.Raw.(*PodStatefulSet).StatefulSet.Name)
	assert.Equal(t, "statefulset2", results[1].Object.Raw.(*PodStatefulSet).StatefulSet.Name)
}

func TestGetPodJobLink(t *testing.T) {
	podCache := newK8sMetaCache(make(chan struct{}), POD)
	jobCache := newK8sMetaCache(make(chan struct{}), JOB)
	jobCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object: &ObjectWrapper{
			Raw: &batch.Job{
				ObjectMeta: metav1.ObjectMeta{
					Name:      "job1",
					Namespace: "default",
				},
			},
		},
	})
	jobCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object: &ObjectWrapper{
			Raw: &batch.Job{
				ObjectMeta: metav1.ObjectMeta{
					Name:      "job2",
					Namespace: "default",
				},
			},
		},
	})
	pod1 := generateMockPod("1")
	pod1.Raw.(*corev1.Pod).OwnerReferences = []metav1.OwnerReference{
		{
			Kind: "Job",
			Name: "job1",
		},
	}
	pod2 := generateMockPod("2")
	pod2.Raw.(*corev1.Pod).OwnerReferences = []metav1.OwnerReference{
		{
			Kind: "Job",
			Name: "job2",
		},
	}
	podCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    pod1,
	})
	podCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    pod2,
	})
	linkGenerator := NewK8sMetaLinkGenerator(map[string]MetaCache{
		POD: podCache,
		JOB: jobCache,
	})
	podList := []*K8sMetaEvent{
		{
			EventType: "update",
			Object:    podCache.metaStore.Items["default/pod1"],
		},
		{
			EventType: "update",
			Object:    podCache.metaStore.Items["default/pod2"],
		},
	}
	results := linkGenerator.getPodJobLink(podList)
	assert.Equal(t, 2, len(results))
	assert.Equal(t, "job1", results[0].Object.Raw.(*PodJob).Job.Name)
	assert.Equal(t, "job2", results[1].Object.Raw.(*PodJob).Job.Name)
}

func TestGetJobCronJobLink(t *testing.T) {
	jobCache := newK8sMetaCache(make(chan struct{}), JOB)
	cronJobCache := newK8sMetaCache(make(chan struct{}), CRONJOB)
	cronJobCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object: &ObjectWrapper{
			Raw: &batch.CronJob{
				ObjectMeta: metav1.ObjectMeta{
					Name:      "cronjob1",
					Namespace: "default",
				},
			},
		},
	})
	cronJobCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object: &ObjectWrapper{
			Raw: &batch.CronJob{
				ObjectMeta: metav1.ObjectMeta{
					Name:      "cronjob2",
					Namespace: "default",
				},
			},
		},
	})
	job1 := &ObjectWrapper{
		Raw: &batch.Job{
			ObjectMeta: metav1.ObjectMeta{
				Name:      "job1",
				Namespace: "default",
			},
		},
	}
	job1.Raw.(*batch.Job).OwnerReferences = []metav1.OwnerReference{
		{
			Kind: "CronJob",
			Name: "cronjob1",
		},
	}
	job2 := &ObjectWrapper{
		Raw: &batch.Job{
			ObjectMeta: metav1.ObjectMeta{
				Name:      "job2",
				Namespace: "default",
			},
		},
	}
	job2.Raw.(*batch.Job).OwnerReferences = []metav1.OwnerReference{
		{
			Kind: "CronJob",
			Name: "cronjob2",
		},
	}
	jobCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    job1,
	})
	jobCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    job2,
	})
	linkGenerator := NewK8sMetaLinkGenerator(map[string]MetaCache{
		JOB:     jobCache,
		CRONJOB: cronJobCache,
	})
	jobList := []*K8sMetaEvent{
		{
			EventType: "update",
			Object:    jobCache.metaStore.Items["default/job1"],
		},
		{
			EventType: "update",
			Object:    jobCache.metaStore.Items["default/job2"],
		},
	}
	results := linkGenerator.getJobCronJobLink(jobList)
	assert.Equal(t, 2, len(results))
	assert.Equal(t, "cronjob1", results[0].Object.Raw.(*JobCronJob).CronJob.Name)
	assert.Equal(t, "cronjob2", results[1].Object.Raw.(*JobCronJob).CronJob.Name)
}

func TestGetPodPVCLink(t *testing.T) {
	podCache := newK8sMetaCache(make(chan struct{}), POD)
	pvcCache := newK8sMetaCache(make(chan struct{}), PERSISTENTVOLUMECLAIM)
	pvcCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object: &ObjectWrapper{
			Raw: &corev1.PersistentVolumeClaim{
				ObjectMeta: metav1.ObjectMeta{
					Name:      "pvc1",
					Namespace: "default",
				},
			},
		},
	})
	pvcCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object: &ObjectWrapper{
			Raw: &corev1.PersistentVolumeClaim{
				ObjectMeta: metav1.ObjectMeta{
					Name:      "pvc2",
					Namespace: "default",
				},
			},
		},
	})
	pod1 := generateMockPod("1")
	pod1.Raw.(*corev1.Pod).Spec.Volumes = []corev1.Volume{
		{
			Name: "volume1",
			VolumeSource: corev1.VolumeSource{
				PersistentVolumeClaim: &corev1.PersistentVolumeClaimVolumeSource{
					ClaimName: "pvc1",
				},
			},
		},
	}
	pod2 := generateMockPod("2")
	pod2.Raw.(*corev1.Pod).Spec.Volumes = []corev1.Volume{
		{
			Name: "volume2",
			VolumeSource: corev1.VolumeSource{
				PersistentVolumeClaim: &corev1.PersistentVolumeClaimVolumeSource{
					ClaimName: "pvc2",
				},
			},
		},
	}
	podCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    pod1,
	})
	podCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    pod2,
	})
	linkGenerator := NewK8sMetaLinkGenerator(map[string]MetaCache{
		POD:                   podCache,
		PERSISTENTVOLUMECLAIM: pvcCache,
	})
	podList := []*K8sMetaEvent{
		{
			EventType: "update",
			Object:    podCache.metaStore.Items["default/pod1"],
		},
		{
			EventType: "update",
			Object:    podCache.metaStore.Items["default/pod2"],
		},
	}
	results := linkGenerator.getPodPVCLink(podList)
	assert.Equal(t, 2, len(results))
	assert.Equal(t, "pvc1", results[0].Object.Raw.(*PodPersistentVolumeClaim).PersistentVolumeClaim.Name)
	assert.Equal(t, "pvc2", results[1].Object.Raw.(*PodPersistentVolumeClaim).PersistentVolumeClaim.Name)
}

func TestGetPodConfigMapLink(t *testing.T) {
	podCache := newK8sMetaCache(make(chan struct{}), POD)
	configMapCache := newK8sMetaCache(make(chan struct{}), CONFIGMAP)
	configMapCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object: &ObjectWrapper{
			Raw: &corev1.ConfigMap{
				ObjectMeta: metav1.ObjectMeta{
					Name:      "configmap1",
					Namespace: "default",
				},
			},
		},
	})
	configMapCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object: &ObjectWrapper{
			Raw: &corev1.ConfigMap{
				ObjectMeta: metav1.ObjectMeta{
					Name:      "configmap2",
					Namespace: "default",
				},
			},
		},
	})
	pod1 := generateMockPod("1")
	pod1.Raw.(*corev1.Pod).Spec.Volumes = []corev1.Volume{
		{
			Name: "volume1",
			VolumeSource: corev1.VolumeSource{
				ConfigMap: &corev1.ConfigMapVolumeSource{
					LocalObjectReference: corev1.LocalObjectReference{
						Name: "configmap1",
					},
				},
			},
		},
	}
	pod2 := generateMockPod("2")
	pod2.Raw.(*corev1.Pod).Spec.Volumes = []corev1.Volume{
		{
			Name: "volume2",
			VolumeSource: corev1.VolumeSource{
				ConfigMap: &corev1.ConfigMapVolumeSource{
					LocalObjectReference: corev1.LocalObjectReference{
						Name: "configmap2",
					},
				},
			},
		},
	}
	podCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    pod1,
	})
	podCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    pod2,
	})
	linkGenerator := NewK8sMetaLinkGenerator(map[string]MetaCache{
		POD:       podCache,
		CONFIGMAP: configMapCache,
	})
	podList := []*K8sMetaEvent{
		{
			EventType: "update",
			Object:    podCache.metaStore.Items["default/pod1"],
		},
		{
			EventType: "update",
			Object:    podCache.metaStore.Items["default/pod2"],
		},
	}
	results := linkGenerator.getPodConfigMapLink(podList)
	assert.Equal(t, 2, len(results))
	assert.Equal(t, "configmap1", results[0].Object.Raw.(*PodConfigMap).ConfigMap.Name)
	assert.Equal(t, "configmap2", results[1].Object.Raw.(*PodConfigMap).ConfigMap.Name)
}

func TestGetPodServiceLink(t *testing.T) {
	podCache := newK8sMetaCache(make(chan struct{}), POD)
	serviceCache := newK8sMetaCache(make(chan struct{}), SERVICE)
	serviceCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object: &ObjectWrapper{
			Raw: &corev1.Service{
				ObjectMeta: metav1.ObjectMeta{
					Name:      "service1",
					Namespace: "default",
				},
				Spec: corev1.ServiceSpec{
					Selector: map[string]string{
						"app": "test",
					},
				},
			},
		},
	})
	serviceCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object: &ObjectWrapper{
			Raw: &corev1.Service{
				ObjectMeta: metav1.ObjectMeta{
					Name:      "service2",
					Namespace: "default",
				},
				Spec: corev1.ServiceSpec{
					Selector: map[string]string{
						"app": "test2",
					},
				},
			},
		},
	})
	pod1 := generateMockPod("1")
	pod1.Raw.(*corev1.Pod).Labels = map[string]string{
		"app": "test",
	}
	pod2 := generateMockPod("2")
	pod2.Raw.(*corev1.Pod).Labels = map[string]string{
		"app": "test2",
	}
	podCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    pod1,
	})
	podCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    pod2,
	})
	linkGenerator := NewK8sMetaLinkGenerator(map[string]MetaCache{
		POD:     podCache,
		SERVICE: serviceCache,
	})
	podList := []*K8sMetaEvent{
		{
			EventType: "update",
			Object:    podCache.metaStore.Items["default/pod1"],
		},
		{
			EventType: "update",
			Object:    podCache.metaStore.Items["default/pod2"],
		},
	}
	results := linkGenerator.getPodServiceLink(podList)
	assert.Equal(t, 2, len(results))
	assert.Equal(t, "service1", results[0].Object.Raw.(*PodService).Service.Name)
	assert.Equal(t, "service2", results[1].Object.Raw.(*PodService).Service.Name)
}

func TestGetPodContainerLink(t *testing.T) {
	podCache := newK8sMetaCache(make(chan struct{}), POD)
	podCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    generateMockPod("1"),
	})
	podCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    generateMockPod("2"),
	})
	linkGenerator := NewK8sMetaLinkGenerator(map[string]MetaCache{
		POD: podCache,
	})
	podList := []*K8sMetaEvent{
		{
			EventType: "update",
			Object:    podCache.metaStore.Items["default/pod1"],
		},
		{
			EventType: "update",
			Object:    podCache.metaStore.Items["default/pod2"],
		},
	}
	results := linkGenerator.getPodContainerLink(podList)
	assert.Equal(t, 2, len(results))
	assert.Equal(t, "test1", results[0].Object.Raw.(*PodContainer).Container.Name)
	assert.Equal(t, "test2", results[1].Object.Raw.(*PodContainer).Container.Name)
}

func TestGetIngressServiceLink(t *testing.T) {
	ingressCache := newK8sMetaCache(make(chan struct{}), INGRESS)
	serviceCache := newK8sMetaCache(make(chan struct{}), SERVICE)
	serviceCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object: &ObjectWrapper{
			Raw: &corev1.Service{
				ObjectMeta: metav1.ObjectMeta{
					Name:      "service1",
					Namespace: "default",
				},
				Spec: corev1.ServiceSpec{
					Selector: map[string]string{
						"app": "test",
					},
				},
			},
		},
	})
	serviceCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object: &ObjectWrapper{
			Raw: &corev1.Service{
				ObjectMeta: metav1.ObjectMeta{
					Name:      "service2",
					Namespace: "default",
				},
				Spec: corev1.ServiceSpec{
					Selector: map[string]string{
						"app": "test2",
					},
				},
			},
		},
	})
	ingressCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object: &ObjectWrapper{
			Raw: &networking.Ingress{
				ObjectMeta: metav1.ObjectMeta{
					Name:      "ingress1",
					Namespace: "default",
				},
				Spec: networking.IngressSpec{
					Rules: []networking.IngressRule{
						{
							IngressRuleValue: networking.IngressRuleValue{
								HTTP: &networking.HTTPIngressRuleValue{
									Paths: []networking.HTTPIngressPath{
										{
											Backend: networking.IngressBackend{
												Service: &networking.IngressServiceBackend{
													Name: "service1",
												},
											},
										},
									},
								},
							},
						},
					},
				},
			},
		},
	})
	ingressCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object: &ObjectWrapper{
			Raw: &networking.Ingress{
				ObjectMeta: metav1.ObjectMeta{
					Name:      "ingress2",
					Namespace: "default",
				},
				Spec: networking.IngressSpec{
					Rules: []networking.IngressRule{
						{
							IngressRuleValue: networking.IngressRuleValue{
								HTTP: &networking.HTTPIngressRuleValue{
									Paths: []networking.HTTPIngressPath{
										{
											Backend: networking.IngressBackend{
												Service: &networking.IngressServiceBackend{
													Name: "service2",
												},
											},
										},
									},
								},
							},
						},
					},
				},
			},
		},
	})
	linkGenerator := NewK8sMetaLinkGenerator(map[string]MetaCache{
		INGRESS: ingressCache,
		SERVICE: serviceCache,
	})
	ingressList := []*K8sMetaEvent{
		{
			EventType: "update",
			Object:    ingressCache.metaStore.Items["default/ingress1"],
		},
		{
			EventType: "update",
			Object:    ingressCache.metaStore.Items["default/ingress2"],
		},
	}
	results := linkGenerator.getIngressServiceLink(ingressList)
	assert.Equal(t, 2, len(results))
	assert.Equal(t, "service1", results[0].Object.Raw.(*IngressService).Service.Name)
	assert.Equal(t, "service2", results[1].Object.Raw.(*IngressService).Service.Name)
}

func TestGetPodNamespaceLink(t *testing.T) {
	podCache := newK8sMetaCache(make(chan struct{}), POD)
	namespaceCache := newK8sMetaCache(make(chan struct{}), NAMESPACE)
	namespaceCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    generateMockNamespace("default"),
	})
	namespaceCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    generateMockNamespace("kube-system"),
	})
	pod1 := generateMockPod("1")
	pod1.Raw.(*corev1.Pod).Namespace = "default"
	pod2 := generateMockPod("2")
	pod2.Raw.(*corev1.Pod).Namespace = "kube-system"
	pod3 := generateMockPod("3")
	pod3.Raw.(*corev1.Pod).Namespace = "kube-system"
	podCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    pod1,
	})
	podCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    pod2,
	})
	podCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    pod3,
	})
	linkGenerator := NewK8sMetaLinkGenerator(map[string]MetaCache{
		POD:       podCache,
		NAMESPACE: namespaceCache,
	})
	podList := []*K8sMetaEvent{
		{
			EventType: "update",
			Object:    podCache.metaStore.Items["default/pod1"],
		},
		{
			EventType: "update",
			Object:    podCache.metaStore.Items["kube-system/pod2"],
		},
		{
			EventType: "update",
			Object:    podCache.metaStore.Items["kube-system/pod3"],
		},
	}
	results := linkGenerator.getPodNamespaceLink(podList)
	assert.Equal(t, 3, len(results))
	assert.Equal(t, "default", results[0].Object.Raw.(*PodNamespace).Namespace.Name)
	assert.Equal(t, "pod1", results[0].Object.Raw.(*PodNamespace).Pod.Name)
	assert.Equal(t, "kube-system", results[1].Object.Raw.(*PodNamespace).Namespace.Name)
	assert.Equal(t, "pod2", results[1].Object.Raw.(*PodNamespace).Pod.Name)
	assert.Equal(t, "kube-system", results[2].Object.Raw.(*PodNamespace).Namespace.Name)
	assert.Equal(t, "pod3", results[2].Object.Raw.(*PodNamespace).Pod.Name)
	assert.Equal(t, POD_NAMESPACE, results[0].Object.ResourceType)
}

func TestGetServiceNamespaceLink(t *testing.T) {
	serviceCache := newK8sMetaCache(make(chan struct{}), SERVICE)
	namespaceCache := newK8sMetaCache(make(chan struct{}), NAMESPACE)
	namespaceCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    generateMockNamespace("default"),
	})
	namespaceCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    generateMockNamespace("kube-system"),
	})
	service1 := &ObjectWrapper{
		Raw: &corev1.Service{
			ObjectMeta: metav1.ObjectMeta{
				Name:      "service1",
				Namespace: "default",
			},
			Spec: corev1.ServiceSpec{
				Selector: map[string]string{
					"app": "test",
				},
			},
		},
	}
	service2 := &ObjectWrapper{
		Raw: &corev1.Service{
			ObjectMeta: metav1.ObjectMeta{
				Name:      "service2",
				Namespace: "kube-system",
			},
			Spec: corev1.ServiceSpec{
				Selector: map[string]string{
					"app": "test",
				},
			},
		},
	}
	serviceCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    service1,
	})
	serviceCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    service2,
	})

	serviceList := []*K8sMetaEvent{
		{
			EventType: "update",
			Object:    service1,
		},
		{
			EventType: "update",
			Object:    service2,
		},
	}
	linkGenerator := NewK8sMetaLinkGenerator(map[string]MetaCache{
		SERVICE:   serviceCache,
		NAMESPACE: namespaceCache,
	})

	results := linkGenerator.getServiceNamespaceLink(serviceList)
	assert.Equal(t, 2, len(results))
	assert.Equal(t, "default", results[0].Object.Raw.(*ServiceNamespace).Namespace.Name)
	assert.Equal(t, "service1", results[0].Object.Raw.(*ServiceNamespace).Service.Name)
	assert.Equal(t, "kube-system", results[1].Object.Raw.(*ServiceNamespace).Namespace.Name)
	assert.Equal(t, "service2", results[1].Object.Raw.(*ServiceNamespace).Service.Name)
	assert.Equal(t, SERVICE_NAMESPACE, results[0].Object.ResourceType)

}

func TestGetDeploymentNamespaceLink(t *testing.T) {
	deploymentCache := newK8sMetaCache(make(chan struct{}), DEPLOYMENT)
	namespaceCache := newK8sMetaCache(make(chan struct{}), NAMESPACE)
	namespaceCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    generateMockNamespace("default"),
	})
	namespaceCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    generateMockNamespace("kube-system"),
	})
	deployment1 := &ObjectWrapper{
		Raw: &app.Deployment{
			ObjectMeta: metav1.ObjectMeta{
				Name:      "deployment1",
				Namespace: "default",
			},
		},
	}
	deployment2 := &ObjectWrapper{
		Raw: &app.Deployment{
			ObjectMeta: metav1.ObjectMeta{
				Name:      "deployment2",
				Namespace: "kube-system",
			},
		},
	}
	deploymentCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    deployment1,
	})
	deploymentCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    deployment2,
	})

	deploymentList := []*K8sMetaEvent{
		{
			EventType: "update",
			Object:    deployment1,
		},
		{
			EventType: "update",
			Object:    deployment2,
		},
	}
	linkGenerator := NewK8sMetaLinkGenerator(map[string]MetaCache{
		DEPLOYMENT: deploymentCache,
		NAMESPACE:  namespaceCache,
	})

	results := linkGenerator.getDeploymentNamespaceLink(deploymentList)
	assert.Equal(t, 2, len(results))
	assert.Equal(t, "default", results[0].Object.Raw.(*DeploymentNamespace).Namespace.Name)
	assert.Equal(t, "deployment1", results[0].Object.Raw.(*DeploymentNamespace).Deployment.Name)
	assert.Equal(t, "kube-system", results[1].Object.Raw.(*DeploymentNamespace).Namespace.Name)
	assert.Equal(t, "deployment2", results[1].Object.Raw.(*DeploymentNamespace).Deployment.Name)
	assert.Equal(t, DEPLOYMENT_NAMESPACE, results[0].Object.ResourceType)

}

func TestGetDaemonSetNamespaceLink(t *testing.T) {
	daemonSetCache := newK8sMetaCache(make(chan struct{}), DAEMONSET)
	namespaceCache := newK8sMetaCache(make(chan struct{}), NAMESPACE)
	namespaceCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    generateMockNamespace("default"),
	})
	namespaceCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    generateMockNamespace("kube-system"),
	})
	daemonset1 := &ObjectWrapper{
		Raw: &app.DaemonSet{
			ObjectMeta: metav1.ObjectMeta{
				Name:      "daemonset1",
				Namespace: "default",
			},
		},
	}
	daemonset2 := &ObjectWrapper{
		Raw: &app.DaemonSet{
			ObjectMeta: metav1.ObjectMeta{
				Name:      "daemonset2",
				Namespace: "kube-system",
			},
		},
	}
	daemonSetCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    daemonset1,
	})
	daemonSetCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    daemonset2,
	})

	daemonsetList := []*K8sMetaEvent{
		{
			EventType: "update",
			Object:    daemonset1,
		},
		{
			EventType: "update",
			Object:    daemonset2,
		},
	}
	linkGenerator := NewK8sMetaLinkGenerator(map[string]MetaCache{
		DAEMONSET: daemonSetCache,
		NAMESPACE: namespaceCache,
	})

	results := linkGenerator.getDaemonSetNamespaceLink(daemonsetList)
	assert.Equal(t, 2, len(results))
	assert.Equal(t, "default", results[0].Object.Raw.(*DaemonSetNamespace).Namespace.Name)
	assert.Equal(t, "daemonset1", results[0].Object.Raw.(*DaemonSetNamespace).DaemonSet.Name)
	assert.Equal(t, "kube-system", results[1].Object.Raw.(*DaemonSetNamespace).Namespace.Name)
	assert.Equal(t, "daemonset2", results[1].Object.Raw.(*DaemonSetNamespace).DaemonSet.Name)
	assert.Equal(t, DAEMONSET_NAMESPACE, results[0].Object.ResourceType)
}

func TestGetStatefulSetNamespaceLink(t *testing.T) {
	statefulSetCache := newK8sMetaCache(make(chan struct{}), STATEFULSET)
	namespaceCache := newK8sMetaCache(make(chan struct{}), NAMESPACE)
	namespaceCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    generateMockNamespace("default"),
	})
	namespaceCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    generateMockNamespace("kube-system"),
	})
	statefulSet1 := &ObjectWrapper{
		Raw: &app.StatefulSet{
			ObjectMeta: metav1.ObjectMeta{
				Name:      "statefulSet1",
				Namespace: "default",
			},
		},
	}
	statefulSet2 := &ObjectWrapper{
		Raw: &app.StatefulSet{
			ObjectMeta: metav1.ObjectMeta{
				Name:      "statefulSet2",
				Namespace: "kube-system",
			},
		},
	}
	statefulSetCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    statefulSet1,
	})
	statefulSetCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    statefulSet2,
	})

	statefulSetList := []*K8sMetaEvent{
		{
			EventType: "update",
			Object:    statefulSet1,
		},
		{
			EventType: "update",
			Object:    statefulSet2,
		},
	}
	linkGenerator := NewK8sMetaLinkGenerator(map[string]MetaCache{
		DAEMONSET: statefulSetCache,
		NAMESPACE: namespaceCache,
	})

	results := linkGenerator.getStatefulsetNamespaceLink(statefulSetList)
	assert.Equal(t, 2, len(results))
	assert.Equal(t, "default", results[0].Object.Raw.(*StatefulSetNamespace).Namespace.Name)
	assert.Equal(t, "statefulSet1", results[0].Object.Raw.(*StatefulSetNamespace).StatefulSet.Name)
	assert.Equal(t, "kube-system", results[1].Object.Raw.(*StatefulSetNamespace).Namespace.Name)
	assert.Equal(t, "statefulSet2", results[1].Object.Raw.(*StatefulSetNamespace).StatefulSet.Name)
	assert.Equal(t, STATEFULSET_NAMESPACE, results[0].Object.ResourceType)
}

func TestGetConfigMapNamespaceLink(t *testing.T) {
	configmapCache := newK8sMetaCache(make(chan struct{}), CONFIGMAP)
	namespaceCache := newK8sMetaCache(make(chan struct{}), NAMESPACE)
	namespaceCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    generateMockNamespace("default"),
	})
	namespaceCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    generateMockNamespace("kube-system"),
	})
	configmap1 := &ObjectWrapper{
		Raw: &corev1.ConfigMap{
			ObjectMeta: metav1.ObjectMeta{
				Name:      "configmap1",
				Namespace: "default",
			},
		},
	}
	configmap2 := &ObjectWrapper{
		Raw: &corev1.ConfigMap{
			ObjectMeta: metav1.ObjectMeta{
				Name:      "configmap2",
				Namespace: "kube-system",
			},
		},
	}
	configmapCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    configmap1,
	})
	configmapCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    configmap2,
	})

	configmapList := []*K8sMetaEvent{
		{
			EventType: "update",
			Object:    configmap1,
		},
		{
			EventType: "update",
			Object:    configmap2,
		},
	}
	linkGenerator := NewK8sMetaLinkGenerator(map[string]MetaCache{
		CONFIGMAP: configmapCache,
		NAMESPACE: namespaceCache,
	})

	results := linkGenerator.getConfigMapNamesapceLink(configmapList)
	assert.Equal(t, 2, len(results))
	assert.Equal(t, "default", results[0].Object.Raw.(*ConfigMapNamespace).Namespace.Name)
	assert.Equal(t, "configmap1", results[0].Object.Raw.(*ConfigMapNamespace).ConfigMap.Name)
	assert.Equal(t, "kube-system", results[1].Object.Raw.(*ConfigMapNamespace).Namespace.Name)
	assert.Equal(t, "configmap2", results[1].Object.Raw.(*ConfigMapNamespace).ConfigMap.Name)
	assert.Equal(t, CONFIGMAP_NAMESPACE, results[0].Object.ResourceType)
}

func TestGetJobNamespaceLink(t *testing.T) {
	jobCache := newK8sMetaCache(make(chan struct{}), JOB)
	namespaceCache := newK8sMetaCache(make(chan struct{}), NAMESPACE)
	namespaceCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    generateMockNamespace("default"),
	})
	namespaceCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    generateMockNamespace("kube-system"),
	})
	job1 := &ObjectWrapper{
		Raw: &batch.Job{
			ObjectMeta: metav1.ObjectMeta{
				Name:      "job1",
				Namespace: "default",
			},
		},
	}
	job2 := &ObjectWrapper{
		Raw: &batch.Job{
			ObjectMeta: metav1.ObjectMeta{
				Name:      "job2",
				Namespace: "kube-system",
			},
		},
	}
	jobCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    job1,
	})
	jobCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    job2,
	})

	jobList := []*K8sMetaEvent{
		{
			EventType: "update",
			Object:    job1,
		},
		{
			EventType: "update",
			Object:    job2,
		},
	}
	linkGenerator := NewK8sMetaLinkGenerator(map[string]MetaCache{
		JOB:       jobCache,
		NAMESPACE: namespaceCache,
	})

	results := linkGenerator.getJobNamesapceLink(jobList)
	assert.Equal(t, 2, len(results))
	assert.Equal(t, "default", results[0].Object.Raw.(*JobNamespace).Namespace.Name)
	assert.Equal(t, "job1", results[0].Object.Raw.(*JobNamespace).Job.Name)
	assert.Equal(t, "kube-system", results[1].Object.Raw.(*JobNamespace).Namespace.Name)
	assert.Equal(t, "job2", results[1].Object.Raw.(*JobNamespace).Job.Name)
	assert.Equal(t, JOB_NAMESPACE, results[0].Object.ResourceType)
}

func TestGetCronJobNamespaceLink(t *testing.T) {
	cronjobCache := newK8sMetaCache(make(chan struct{}), CRONJOB)
	namespaceCache := newK8sMetaCache(make(chan struct{}), NAMESPACE)
	namespaceCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    generateMockNamespace("default"),
	})
	namespaceCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    generateMockNamespace("kube-system"),
	})
	cronjob1 := &ObjectWrapper{
		Raw: &batch.CronJob{
			ObjectMeta: metav1.ObjectMeta{
				Name:      "cronjob1",
				Namespace: "default",
			},
		},
	}
	cronjob2 := &ObjectWrapper{
		Raw: &batch.CronJob{
			ObjectMeta: metav1.ObjectMeta{
				Name:      "cronjob2",
				Namespace: "kube-system",
			},
		},
	}
	cronjobCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    cronjob1,
	})
	cronjobCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    cronjob2,
	})

	jobList := []*K8sMetaEvent{
		{
			EventType: "update",
			Object:    cronjob1,
		},
		{
			EventType: "update",
			Object:    cronjob2,
		},
	}
	linkGenerator := NewK8sMetaLinkGenerator(map[string]MetaCache{
		CRONJOB:   cronjobCache,
		NAMESPACE: namespaceCache,
	})

	results := linkGenerator.getCronJobNamesapceLink(jobList)
	assert.Equal(t, 2, len(results))
	assert.Equal(t, "default", results[0].Object.Raw.(*CronJobNamespace).Namespace.Name)
	assert.Equal(t, "cronjob1", results[0].Object.Raw.(*CronJobNamespace).CronJob.Name)
	assert.Equal(t, "kube-system", results[1].Object.Raw.(*CronJobNamespace).Namespace.Name)
	assert.Equal(t, "cronjob2", results[1].Object.Raw.(*CronJobNamespace).CronJob.Name)
	assert.Equal(t, CRONJOB_NAMESPACE, results[0].Object.ResourceType)
}

func TestGetPVCNamespaceLink(t *testing.T) {
	pvcCache := newK8sMetaCache(make(chan struct{}), PERSISTENTVOLUME)
	namespaceCache := newK8sMetaCache(make(chan struct{}), NAMESPACE)
	namespaceCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    generateMockNamespace("default"),
	})
	namespaceCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    generateMockNamespace("kube-system"),
	})
	pvc1 := &ObjectWrapper{
		Raw: &corev1.PersistentVolumeClaim{
			ObjectMeta: metav1.ObjectMeta{
				Name:      "pvc1",
				Namespace: "default",
			},
		},
	}
	pvc2 := &ObjectWrapper{
		Raw: &corev1.PersistentVolumeClaim{
			ObjectMeta: metav1.ObjectMeta{
				Name:      "pvc2",
				Namespace: "kube-system",
			},
		},
	}
	pvcCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    pvc1,
	})
	pvcCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    pvc2,
	})

	jobList := []*K8sMetaEvent{
		{
			EventType: "update",
			Object:    pvc1,
		},
		{
			EventType: "update",
			Object:    pvc2,
		},
	}
	linkGenerator := NewK8sMetaLinkGenerator(map[string]MetaCache{
		PERSISTENTVOLUMECLAIM: pvcCache,
		NAMESPACE:             namespaceCache,
	})

	results := linkGenerator.getPVCNamesapceLink(jobList)
	assert.Equal(t, 2, len(results))
	assert.Equal(t, "default", results[0].Object.Raw.(*PersistentVolumeClaimNamespace).Namespace.Name)
	assert.Equal(t, "pvc1", results[0].Object.Raw.(*PersistentVolumeClaimNamespace).PersistentVolumeClaim.Name)
	assert.Equal(t, "kube-system", results[1].Object.Raw.(*PersistentVolumeClaimNamespace).Namespace.Name)
	assert.Equal(t, "pvc2", results[1].Object.Raw.(*PersistentVolumeClaimNamespace).PersistentVolumeClaim.Name)
	assert.Equal(t, PERSISTENTVOLUMECLAIM_NAMESPACE, results[0].Object.ResourceType)
}

func TestGetIngressNamespaceLink(t *testing.T) {
	ingressCache := newK8sMetaCache(make(chan struct{}), INGRESS)
	namespaceCache := newK8sMetaCache(make(chan struct{}), NAMESPACE)
	namespaceCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    generateMockNamespace("default"),
	})
	namespaceCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    generateMockNamespace("kube-system"),
	})
	ingress1 := &ObjectWrapper{
		Raw: &networking.Ingress{
			ObjectMeta: metav1.ObjectMeta{
				Name:      "ingress1",
				Namespace: "default",
			},
		},
	}
	ingress2 := &ObjectWrapper{
		Raw: &networking.Ingress{
			ObjectMeta: metav1.ObjectMeta{
				Name:      "ingress2",
				Namespace: "kube-system",
			},
		},
	}
	ingressCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    ingress1,
	})
	ingressCache.metaStore.handleAddOrUpdateEvent(&K8sMetaEvent{
		EventType: "add",
		Object:    ingress2,
	})

	jobList := []*K8sMetaEvent{
		{
			EventType: "update",
			Object:    ingress1,
		},
		{
			EventType: "update",
			Object:    ingress2,
		},
	}
	linkGenerator := NewK8sMetaLinkGenerator(map[string]MetaCache{
		INGRESS:   ingressCache,
		NAMESPACE: namespaceCache,
	})

	results := linkGenerator.getIngressNamesapceLink(jobList)
	assert.Equal(t, 2, len(results))
	assert.Equal(t, "default", results[0].Object.Raw.(*IngressNamespace).Namespace.Name)
	assert.Equal(t, "ingress1", results[0].Object.Raw.(*IngressNamespace).Ingress.Name)
	assert.Equal(t, "kube-system", results[1].Object.Raw.(*IngressNamespace).Namespace.Name)
	assert.Equal(t, "ingress2", results[1].Object.Raw.(*IngressNamespace).Ingress.Name)
	assert.Equal(t, INGRESS_NAMESPACE, results[0].Object.ResourceType)
}

func generateMockNamespace(namespaceName string) *ObjectWrapper {
	return &ObjectWrapper{
		Raw: &corev1.Namespace{
			ObjectMeta: metav1.ObjectMeta{
				Name:      namespaceName,
				Namespace: "", // namesapce itself without namesapce
			},
		},
	}
}

func generateMockPod(index string) *ObjectWrapper {
	return &ObjectWrapper{
		Raw: &corev1.Pod{
			ObjectMeta: metav1.ObjectMeta{
				Name:      "pod" + index,
				Namespace: "default",
			},
			Spec: corev1.PodSpec{
				Containers: []corev1.Container{
					{
						Name:  "test" + index,
						Image: "test" + index,
					},
				},
			},
		},
	}
}
