package k8smeta

import (
	"reflect"

	app "k8s.io/api/apps/v1"
	v1 "k8s.io/api/core/v1"
	meta "k8s.io/apimachinery/pkg/api/meta"
)

// isSignificantUpdate returns true if the update carries changes that affect
// processor output or cache indexes. Status-only changes that don't alter
// any consumed field are filtered out to reduce EventCh pressure.
func isSignificantUpdate(resourceType string, oldObj, newObj interface{}) bool {
	switch resourceType {
	case POD:
		return isPodSignificantChange(oldObj, newObj)
	case SERVICE, DAEMONSET, STATEFULSET, REPLICASET,
		CONFIGMAP, NAMESPACE, CRONJOB, INGRESS, STORAGECLASS:
		return isMetadataOnlySignificantChange(oldObj, newObj)
	default:
		// Node, Deployment, Job, PV, PVC use status fields — allow all updates
		return true
	}
}

// isPodSignificantChange checks whether Pod fields consumed by the entity
// processor (Phase, PodIP) or cache indexes (HostIP, ContainerIDs) changed.
func isPodSignificantChange(oldObj, newObj interface{}) bool {
	oldPod, ok1 := oldObj.(*v1.Pod)
	newPod, ok2 := newObj.(*v1.Pod)
	if !ok1 || !ok2 {
		return true
	}
	if oldPod.Generation != newPod.Generation {
		return true
	}
	if !reflect.DeepEqual(oldPod.Labels, newPod.Labels) {
		return true
	}
	if !reflect.DeepEqual(oldPod.Annotations, newPod.Annotations) {
		return true
	}
	if oldPod.Status.Phase != newPod.Status.Phase {
		return true
	}
	if oldPod.Status.PodIP != newPod.Status.PodIP {
		return true
	}
	if oldPod.Status.HostIP != newPod.Status.HostIP {
		return true
	}
	if !sameContainerIDs(oldPod.Status.ContainerStatuses, newPod.Status.ContainerStatuses) {
		return true
	}
	return false
}

func sameContainerIDs(a, b []v1.ContainerStatus) bool {
	if len(a) != len(b) {
		return false
	}
	for i := range a {
		if a[i].ContainerID != b[i].ContainerID {
			return false
		}
	}
	return true
}

// isMetadataOnlySignificantChange is for resources whose processor uses only
// metadata/spec fields. Status-only changes (which bump ResourceVersion but
// not Generation) are filtered out.
func isMetadataOnlySignificantChange(oldObj, newObj interface{}) bool {
	oldMeta, err1 := meta.Accessor(oldObj)
	newMeta, err2 := meta.Accessor(newObj)
	if err1 != nil || err2 != nil {
		return true
	}
	if oldMeta.GetGeneration() != newMeta.GetGeneration() {
		return true
	}
	if !reflect.DeepEqual(oldMeta.GetLabels(), newMeta.GetLabels()) {
		return true
	}
	if !reflect.DeepEqual(oldMeta.GetAnnotations(), newMeta.GetAnnotations()) {
		return true
	}
	// DaemonSet/StatefulSet/ReplicaSet: also check Spec.Replicas via type assertion
	if oldDS, ok := oldObj.(*app.DaemonSet); ok {
		newDS := newObj.(*app.DaemonSet)
		if oldDS.Generation != newDS.Generation {
			return true
		}
	}
	return false
}
