package k8smeta

import (
	"strings"

	app "k8s.io/api/apps/v1"
	batch "k8s.io/api/batch/v1"
	v1 "k8s.io/api/core/v1"
	networking "k8s.io/api/networking/v1"
	"k8s.io/apimachinery/pkg/labels"
)

type LinkGenerator struct {
	metaCache map[string]MetaCache
}

func NewK8sMetaLinkGenerator(metaCache map[string]MetaCache) *LinkGenerator {
	return &LinkGenerator{
		metaCache: metaCache,
	}
}

func (g *LinkGenerator) GenerateLinks(events []*K8sMetaEvent, linkType string) []*K8sMetaEvent {
	if len(events) == 0 {
		return nil
	}
	resourceType := events[0].Object.ResourceType
	// only generate link from the src entity
	if !strings.HasPrefix(linkType, resourceType) {
		return nil
	}
	switch linkType {
	case POD_NODE:
		return g.getPodNodeLink(events)
	case POD_DEPLOYMENT:
		return g.getPodDeploymentLink(events)
	case POD_REPLICASET:
		return g.getPodReplicaSetLink(events)
	case POD_STATEFULSET:
		return g.getPodStatefulSetLink(events)
	case POD_DAEMONSET:
		return g.getPodDaemonSetLink(events)
	case POD_JOB:
		return g.getPodJobLink(events)
	case JOB_CRONJOB:
		return g.getJobCronJobLink(events)
	case POD_PERSISENTVOLUMECLAIN:
		return g.getPodPVCLink(events)
	case POD_CONFIGMAP:
		return g.getPodConfigMapLink(events)
	case POD_SERVICE:
		return g.getPodServiceLink(events)
	case POD_CONTAINER:
		return g.getPodContainerLink(events)
	case REPLICASET_DEPLOYMENT:
		return g.getReplicaSetDeploymentLink(events)
	case INGRESS_SERVICE:
		return g.getIngressServiceLink(events)
	default:
		return nil
	}
}

func (g *LinkGenerator) getPodNodeLink(podList []*K8sMetaEvent) []*K8sMetaEvent {
	nodeCache := g.metaCache[NODE]
	result := make([]*K8sMetaEvent, 0)
	for _, event := range podList {
		pod, ok := event.Object.Raw.(*v1.Pod)
		if !ok {
			continue
		}
		nodes := nodeCache.Get([]string{pod.Spec.NodeName})
		for _, node := range nodes {
			for _, n := range node {
				result = append(result, &K8sMetaEvent{
					EventType: event.EventType,
					Object: &ObjectWrapper{
						ResourceType: POD_NODE,
						Raw: &NodePod{
							Node: n.Raw.(*v1.Node),
							Pod:  pod,
						},
						FirstObservedTime: event.Object.FirstObservedTime,
						LastObservedTime:  event.Object.LastObservedTime,
					},
				})
			}
		}
	}
	return result
}

func (g *LinkGenerator) getPodDeploymentLink(podList []*K8sMetaEvent) []*K8sMetaEvent {
	result := make([]*K8sMetaEvent, 0)
	for _, data := range podList {
		pod, ok := data.Object.Raw.(*v1.Pod)
		if !ok || len(pod.OwnerReferences) == 0 || pod.OwnerReferences[0].Kind != "ReplicaSet" {
			continue
		}
		parentName := pod.OwnerReferences[0].Name
		rsList := g.metaCache[REPLICASET].Get([]string{generateNameWithNamespaceKey(pod.Namespace, parentName)})
		for _, rs := range rsList {
			for _, r := range rs {
				if deploymentName := r.Raw.(*app.ReplicaSet).OwnerReferences[0].Name; deploymentName != "" {
					deploymentList := g.metaCache[DEPLOYMENT].Get([]string{generateNameWithNamespaceKey(pod.Namespace, deploymentName)})
					for _, deployments := range deploymentList {
						for _, d := range deployments {
							result = append(result, &K8sMetaEvent{
								EventType: data.EventType,
								Object: &ObjectWrapper{
									ResourceType: POD_DEPLOYMENT,
									Raw: &PodDeployment{
										Deployment: d.Raw.(*app.Deployment),
										Pod:        pod,
									},
									FirstObservedTime: data.Object.FirstObservedTime,
									LastObservedTime:  data.Object.LastObservedTime,
								},
							})
						}
					}
				}
			}
		}
	}
	return result
}

func (g *LinkGenerator) getReplicaSetDeploymentLink(rsList []*K8sMetaEvent) []*K8sMetaEvent {
	result := make([]*K8sMetaEvent, 0)
	for _, event := range rsList {
		replicaset, ok := event.Object.Raw.(*app.ReplicaSet)
		if !ok || len(replicaset.OwnerReferences) == 0 {
			continue
		}
		deploymentName := replicaset.OwnerReferences[0].Name
		deployments := g.metaCache[DEPLOYMENT].Get([]string{generateNameWithNamespaceKey(replicaset.Namespace, deploymentName)})
		for _, deployment := range deployments {
			for _, d := range deployment {
				result = append(result, &K8sMetaEvent{
					EventType: event.EventType,
					Object: &ObjectWrapper{
						ResourceType: REPLICASET_DEPLOYMENT,
						Raw: &ReplicaSetDeployment{
							Deployment: d.Raw.(*app.Deployment),
							ReplicaSet: replicaset,
						},
						FirstObservedTime: event.Object.FirstObservedTime,
						LastObservedTime:  event.Object.LastObservedTime,
					},
				})
			}
		}
	}
	return result
}

func (g *LinkGenerator) getPodReplicaSetLink(podList []*K8sMetaEvent) []*K8sMetaEvent {
	result := make([]*K8sMetaEvent, 0)
	for _, data := range podList {
		pod, ok := data.Object.Raw.(*v1.Pod)
		if !ok || len(pod.OwnerReferences) == 0 || pod.OwnerReferences[0].Kind != "ReplicaSet" {
			continue
		}
		parentName := pod.OwnerReferences[0].Name
		rsList := g.metaCache[REPLICASET].Get([]string{generateNameWithNamespaceKey(pod.Namespace, parentName)})
		for _, rs := range rsList {
			for _, r := range rs {
				result = append(result, &K8sMetaEvent{
					EventType: data.EventType,
					Object: &ObjectWrapper{
						ResourceType: POD_REPLICASET,
						Raw: &PodReplicaSet{
							ReplicaSet: r.Raw.(*app.ReplicaSet),
							Pod:        pod,
						},
						FirstObservedTime: data.Object.FirstObservedTime,
						LastObservedTime:  data.Object.LastObservedTime,
					},
				})
			}
		}
	}
	return result
}

func (g *LinkGenerator) getPodStatefulSetLink(podList []*K8sMetaEvent) []*K8sMetaEvent {
	result := make([]*K8sMetaEvent, 0)
	for _, data := range podList {
		pod, ok := data.Object.Raw.(*v1.Pod)
		if !ok || len(pod.OwnerReferences) == 0 || pod.OwnerReferences[0].Kind != "StatefulSet" {
			continue
		}
		parentName := pod.OwnerReferences[0].Name
		ssList := g.metaCache[STATEFULSET].Get([]string{generateNameWithNamespaceKey(pod.Namespace, parentName)})
		for _, ss := range ssList {
			for _, s := range ss {
				result = append(result, &K8sMetaEvent{
					EventType: data.EventType,
					Object: &ObjectWrapper{
						ResourceType: POD_STATEFULSET,
						Raw: &PodStatefulSet{
							StatefulSet: s.Raw.(*app.StatefulSet),
							Pod:         pod,
						},
						FirstObservedTime: data.Object.FirstObservedTime,
						LastObservedTime:  data.Object.LastObservedTime,
					},
				})
			}
		}
	}
	return result
}

func (g *LinkGenerator) getPodDaemonSetLink(podList []*K8sMetaEvent) []*K8sMetaEvent {
	result := make([]*K8sMetaEvent, 0)
	for _, data := range podList {
		pod, ok := data.Object.Raw.(*v1.Pod)
		if !ok || len(pod.OwnerReferences) == 0 || pod.OwnerReferences[0].Kind != "DaemonSet" {
			continue
		}
		parentName := pod.OwnerReferences[0].Name
		dsList := g.metaCache[DAEMONSET].Get([]string{generateNameWithNamespaceKey(pod.Namespace, parentName)})
		for _, ds := range dsList {
			for _, d := range ds {
				result = append(result, &K8sMetaEvent{
					EventType: data.EventType,
					Object: &ObjectWrapper{
						ResourceType: POD_DAEMONSET,
						Raw: &PodDaemonSet{
							DaemonSet: d.Raw.(*app.DaemonSet),
							Pod:       pod,
						},
						FirstObservedTime: data.Object.FirstObservedTime,
						LastObservedTime:  data.Object.LastObservedTime,
					},
				})
			}
		}
	}
	return result
}

func (g *LinkGenerator) getPodJobLink(podList []*K8sMetaEvent) []*K8sMetaEvent {
	result := make([]*K8sMetaEvent, 0)
	for _, data := range podList {
		pod, ok := data.Object.Raw.(*v1.Pod)
		if !ok || len(pod.OwnerReferences) == 0 || pod.OwnerReferences[0].Kind != "Job" {
			continue
		}
		parentName := pod.OwnerReferences[0].Name
		jobList := g.metaCache[JOB].Get([]string{generateNameWithNamespaceKey(pod.Namespace, parentName)})
		for _, job := range jobList {
			for _, j := range job {
				result = append(result, &K8sMetaEvent{
					EventType: data.EventType,
					Object: &ObjectWrapper{
						ResourceType: POD_JOB,
						Raw: &PodJob{
							Job: j.Raw.(*batch.Job),
							Pod: pod,
						},
						FirstObservedTime: data.Object.FirstObservedTime,
						LastObservedTime:  data.Object.LastObservedTime,
					},
				})
			}
		}
	}
	return result
}

func (g *LinkGenerator) getJobCronJobLink(jobList []*K8sMetaEvent) []*K8sMetaEvent {
	result := make([]*K8sMetaEvent, 0)
	for _, data := range jobList {
		job, ok := data.Object.Raw.(*batch.Job)
		if !ok || len(job.OwnerReferences) == 0 {
			continue
		}
		cronJobName := job.OwnerReferences[0].Name
		cronJobList := g.metaCache[CRONJOB].Get([]string{generateNameWithNamespaceKey(job.Namespace, cronJobName)})
		for _, cj := range cronJobList {
			for _, c := range cj {
				result = append(result, &K8sMetaEvent{
					EventType: data.EventType,
					Object: &ObjectWrapper{
						ResourceType: JOB_CRONJOB,
						Raw: &JobCronJob{
							CronJob: c.Raw.(*batch.CronJob),
							Job:     job,
						},
						FirstObservedTime: data.Object.FirstObservedTime,
						LastObservedTime:  data.Object.LastObservedTime,
					},
				})
			}
		}
	}
	return result
}

func (g *LinkGenerator) getPodPVCLink(podList []*K8sMetaEvent) []*K8sMetaEvent {
	result := make([]*K8sMetaEvent, 0)
	for _, data := range podList {
		pod, ok := data.Object.Raw.(*v1.Pod)
		if !ok {
			continue
		}
		for _, volume := range pod.Spec.Volumes {
			if volume.PersistentVolumeClaim != nil {
				pvcName := volume.PersistentVolumeClaim.ClaimName
				pvcList := g.metaCache[PERSISTENTVOLUMECLAIM].Get([]string{generateNameWithNamespaceKey(pod.Namespace, pvcName)})
				for _, pvc := range pvcList {
					for _, p := range pvc {
						result = append(result, &K8sMetaEvent{
							EventType: data.EventType,
							Object: &ObjectWrapper{
								ResourceType: POD_PERSISENTVOLUMECLAIN,
								Raw: &PodPersistentVolumeClaim{
									Pod:                   pod,
									PersistentVolumeClaim: p.Raw.(*v1.PersistentVolumeClaim),
								},
								FirstObservedTime: data.Object.FirstObservedTime,
								LastObservedTime:  data.Object.LastObservedTime,
							},
						})
					}
				}
			}
		}
	}
	return result
}

func (g *LinkGenerator) getPodConfigMapLink(podList []*K8sMetaEvent) []*K8sMetaEvent {
	result := make([]*K8sMetaEvent, 0)
	for _, data := range podList {
		pod, ok := data.Object.Raw.(*v1.Pod)
		if !ok {
			continue
		}
		for _, volume := range pod.Spec.Volumes {
			if volume.ConfigMap != nil {
				cmName := volume.ConfigMap.Name
				cmList := g.metaCache[CONFIGMAP].Get([]string{generateNameWithNamespaceKey(pod.Namespace, cmName)})
				for _, cm := range cmList {
					for _, c := range cm {
						result = append(result, &K8sMetaEvent{
							EventType: data.EventType,
							Object: &ObjectWrapper{
								ResourceType: POD_CONFIGMAP,
								Raw: &PodConfigMap{
									Pod:       pod,
									ConfigMap: c.Raw.(*v1.ConfigMap),
								},
								FirstObservedTime: data.Object.FirstObservedTime,
								LastObservedTime:  data.Object.LastObservedTime,
							},
						})
					}
				}
			}
		}
	}
	return result
}

func (g *LinkGenerator) getPodServiceLink(podList []*K8sMetaEvent) []*K8sMetaEvent {
	serviceList := g.metaCache[SERVICE].List()
	result := make([]*K8sMetaEvent, 0)
	matchers := make(map[string]labelMatchers)
	for _, data := range serviceList {
		service, ok := data.Raw.(*v1.Service)
		if !ok {
			continue
		}

		_, ok = matchers[service.Namespace]
		lm := newLabelMatcher(data.Raw, labels.SelectorFromSet(service.Spec.Selector))
		if !ok {
			matchers[service.Namespace] = []*labelMatcher{lm}
		} else {
			matchers[service.Namespace] = append(matchers[service.Namespace], lm)
		}
	}

	for _, data := range podList {
		pod, ok := data.Object.Raw.(*v1.Pod)
		if !ok {
			continue
		}
		nsSelectors, ok := matchers[pod.Namespace]
		if !ok {
			continue
		}
		set := labels.Set(pod.Labels)
		for _, s := range nsSelectors {
			if !s.selector.Empty() && s.selector.Matches(set) {
				result = append(result, &K8sMetaEvent{
					EventType: data.EventType,
					Object: &ObjectWrapper{
						ResourceType: POD_SERVICE,
						Raw: &PodService{
							Pod:     pod,
							Service: s.obj.(*v1.Service),
						},
						FirstObservedTime: data.Object.FirstObservedTime,
						LastObservedTime:  data.Object.LastObservedTime,
					},
				})
			}
		}
	}
	return result
}

func (g *LinkGenerator) getPodContainerLink(podList []*K8sMetaEvent) []*K8sMetaEvent {
	result := make([]*K8sMetaEvent, 0)
	for _, data := range podList {
		pod, ok := data.Object.Raw.(*v1.Pod)
		if !ok {
			continue
		}
		for i := range pod.Spec.Containers {
			result = append(result, &K8sMetaEvent{
				EventType: data.EventType,
				Object: &ObjectWrapper{
					ResourceType: POD_CONTAINER,
					Raw: &PodContainer{
						Pod:       pod,
						Container: &pod.Spec.Containers[i],
					},
					FirstObservedTime: data.Object.FirstObservedTime,
					LastObservedTime:  data.Object.LastObservedTime,
				},
			})
		}
	}
	return result
}

func (g *LinkGenerator) getIngressServiceLink(ingressList []*K8sMetaEvent) []*K8sMetaEvent {
	result := make([]*K8sMetaEvent, 0)
	for _, data := range ingressList {
		ingress, ok := data.Object.Raw.(*networking.Ingress)
		if !ok {
			continue
		}
		serviceNameSet := make(map[string]struct{}, 0)
		for _, rule := range ingress.Spec.Rules {
			for _, path := range rule.HTTP.Paths {
				serviceNameSet[path.Backend.Service.Name] = struct{}{}
			}
		}
		serviceNameList := make([]string, 0, len(serviceNameSet))
		for name := range serviceNameSet {
			serviceNameList = append(serviceNameList, generateNameWithNamespaceKey(ingress.Namespace, name))
		}
		serviceList := g.metaCache[SERVICE].Get(serviceNameList)
		for _, service := range serviceList {
			for _, s := range service {
				result = append(result, &K8sMetaEvent{
					EventType: data.EventType,
					Object: &ObjectWrapper{
						ResourceType: INGRESS_SERVICE,
						Raw: &IngressService{
							Ingress: ingress,
							Service: s.Raw.(*v1.Service),
						},
						FirstObservedTime: data.Object.FirstObservedTime,
						LastObservedTime:  data.Object.LastObservedTime,
					},
				})
			}
		}
	}
	return result
}
