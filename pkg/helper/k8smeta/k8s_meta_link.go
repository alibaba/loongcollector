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
	case POD_NAMESPACE:
		return g.getPodNamespaceLink(events)
	case SERVICE_NAMESPACE:
		return g.getServiceNamespaceLink(events)
	case DEPLOYMENT_NAMESPACE:
		return g.getDeploymentNamespaceLink(events)
	case DAEMONSET_NAMESPACE:
		return g.getDaemonSetNamespaceLink(events)
	case STATEFULSET_NAMESPACE:
		return g.getStatefulsetNamespaceLink(events)
	case CONFIGMAP_NAMESPACE:
		return g.getConfigMapNamespaceLink(events)
	case JOB_NAMESPACE:
		return g.getJobNamespaceLink(events)
	case CRONJOB_NAMESPACE:
		return g.getCronJobNamespaceLink(events)
	case PERSISTENTVOLUMECLAIM_NAMESPACE:
		return g.getPVCNamespaceLink(events)
	case INGRESS_NAMESPACE:
		return g.getIngressNamespaceLink(events)
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
		if pod.Spec.NodeName == "" {
			continue
		}
		nodes := nodeCache.Get([]string{pod.Spec.NodeName})
		for _, node := range nodes {
			for _, n := range node {
				if nodeObj, ok := n.Raw.(*v1.Node); ok {
					result = append(result, &K8sMetaEvent{
						EventType: event.EventType,
						Object: &ObjectWrapper{
							ResourceType: POD_NODE,
							Raw: &PodNode{
								Node: nodeObj,
								Pod:  pod,
							},
							FirstObservedTime: event.Object.FirstObservedTime,
							LastObservedTime:  event.Object.LastObservedTime,
						},
					})
				}
			}
		}
	}
	return result
}

func (g *LinkGenerator) getPodDeploymentLink(podList []*K8sMetaEvent) []*K8sMetaEvent {
	result := make([]*K8sMetaEvent, 0)
	for _, data := range podList {
		pod, ok := data.Object.Raw.(*v1.Pod)
		if !ok || len(pod.OwnerReferences) == 0 || pod.OwnerReferences[0].Kind != K8S_REPLICASET_TYPE {
			continue
		}
		parentName := pod.OwnerReferences[0].Name
		if parentName == "" {
			continue
		}
		rsList := g.metaCache[REPLICASET].Get([]string{generateNameWithNamespaceKey(pod.Namespace, parentName)})
		for _, rs := range rsList {
			for _, r := range rs {
				replicaSet, ok := r.Raw.(*app.ReplicaSet)
				if !ok || len(replicaSet.OwnerReferences) == 0 || replicaSet.OwnerReferences[0].Kind != K8S_DEPLOYMENT_TYPE {
					continue
				}
				deploymentName := replicaSet.OwnerReferences[0].Name
				if deploymentName == "" {
					continue
				}
				deploymentList := g.metaCache[DEPLOYMENT].Get([]string{generateNameWithNamespaceKey(pod.Namespace, deploymentName)})
				for _, deployments := range deploymentList {
					for _, d := range deployments {
						if deploymentObj, ok := d.Raw.(*app.Deployment); ok {
							result = append(result, &K8sMetaEvent{
								EventType: data.EventType,
								Object: &ObjectWrapper{
									ResourceType: POD_DEPLOYMENT,
									Raw: &PodDeployment{
										Deployment: deploymentObj,
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
		if !ok || len(replicaset.OwnerReferences) == 0 || replicaset.OwnerReferences[0].Kind != K8S_DEPLOYMENT_TYPE {
			continue
		}
		deploymentName := replicaset.OwnerReferences[0].Name
		if deploymentName == "" {
			continue
		}
		deployments := g.metaCache[DEPLOYMENT].Get([]string{generateNameWithNamespaceKey(replicaset.Namespace, deploymentName)})
		for _, deployment := range deployments {
			for _, d := range deployment {
				if deploymentObj, ok := d.Raw.(*app.Deployment); ok {
					result = append(result, &K8sMetaEvent{
						EventType: event.EventType,
						Object: &ObjectWrapper{
							ResourceType: REPLICASET_DEPLOYMENT,
							Raw: &ReplicaSetDeployment{
								Deployment: deploymentObj,
								ReplicaSet: replicaset,
							},
							FirstObservedTime: event.Object.FirstObservedTime,
							LastObservedTime:  event.Object.LastObservedTime,
						},
					})
				}
			}
		}
	}
	return result
}

func (g *LinkGenerator) getPodReplicaSetLink(podList []*K8sMetaEvent) []*K8sMetaEvent {
	result := make([]*K8sMetaEvent, 0)
	for _, data := range podList {
		pod, ok := data.Object.Raw.(*v1.Pod)
		if !ok || len(pod.OwnerReferences) == 0 || pod.OwnerReferences[0].Kind != K8S_REPLICASET_TYPE {
			continue
		}
		parentName := pod.OwnerReferences[0].Name
		if parentName == "" {
			continue
		}
		rsList := g.metaCache[REPLICASET].Get([]string{generateNameWithNamespaceKey(pod.Namespace, parentName)})
		for _, rs := range rsList {
			for _, r := range rs {
				if replicaSet, ok := r.Raw.(*app.ReplicaSet); ok {
					result = append(result, &K8sMetaEvent{
						EventType: data.EventType,
						Object: &ObjectWrapper{
							ResourceType: POD_REPLICASET,
							Raw: &PodReplicaSet{
								ReplicaSet: replicaSet,
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
	return result
}

func (g *LinkGenerator) getPodStatefulSetLink(podList []*K8sMetaEvent) []*K8sMetaEvent {
	result := make([]*K8sMetaEvent, 0)
	for _, data := range podList {
		pod, ok := data.Object.Raw.(*v1.Pod)
		if !ok || len(pod.OwnerReferences) == 0 || pod.OwnerReferences[0].Kind != K8S_STATEFULSET_TYPE {
			continue
		}
		parentName := pod.OwnerReferences[0].Name
		if parentName == "" {
			continue
		}
		ssList := g.metaCache[STATEFULSET].Get([]string{generateNameWithNamespaceKey(pod.Namespace, parentName)})
		for _, ss := range ssList {
			for _, s := range ss {
				if statefulSet, ok := s.Raw.(*app.StatefulSet); ok {
					result = append(result, &K8sMetaEvent{
						EventType: data.EventType,
						Object: &ObjectWrapper{
							ResourceType: POD_STATEFULSET,
							Raw: &PodStatefulSet{
								StatefulSet: statefulSet,
								Pod:         pod,
							},
							FirstObservedTime: data.Object.FirstObservedTime,
							LastObservedTime:  data.Object.LastObservedTime,
						},
					})
				}
			}
		}
	}
	return result
}

func (g *LinkGenerator) getPodDaemonSetLink(podList []*K8sMetaEvent) []*K8sMetaEvent {
	result := make([]*K8sMetaEvent, 0)
	for _, data := range podList {
		pod, ok := data.Object.Raw.(*v1.Pod)
		if !ok || len(pod.OwnerReferences) == 0 || pod.OwnerReferences[0].Kind != K8S_DAEMONSET_TYPE {
			continue
		}
		parentName := pod.OwnerReferences[0].Name
		if parentName == "" {
			continue
		}
		dsList := g.metaCache[DAEMONSET].Get([]string{generateNameWithNamespaceKey(pod.Namespace, parentName)})
		for _, ds := range dsList {
			for _, d := range ds {
				if daemonSet, ok := d.Raw.(*app.DaemonSet); ok {
					result = append(result, &K8sMetaEvent{
						EventType: data.EventType,
						Object: &ObjectWrapper{
							ResourceType: POD_DAEMONSET,
							Raw: &PodDaemonSet{
								DaemonSet: daemonSet,
								Pod:       pod,
							},
							FirstObservedTime: data.Object.FirstObservedTime,
							LastObservedTime:  data.Object.LastObservedTime,
						},
					})
				}
			}
		}
	}
	return result
}

func (g *LinkGenerator) getPodJobLink(podList []*K8sMetaEvent) []*K8sMetaEvent {
	result := make([]*K8sMetaEvent, 0)
	for _, data := range podList {
		pod, ok := data.Object.Raw.(*v1.Pod)
		if !ok || len(pod.OwnerReferences) == 0 || pod.OwnerReferences[0].Kind != K8S_JOB_TYPE {
			continue
		}
		parentName := pod.OwnerReferences[0].Name
		if parentName == "" {
			continue
		}
		jobList := g.metaCache[JOB].Get([]string{generateNameWithNamespaceKey(pod.Namespace, parentName)})
		for _, job := range jobList {
			for _, j := range job {
				if jobObj, ok := j.Raw.(*batch.Job); ok {
					result = append(result, &K8sMetaEvent{
						EventType: data.EventType,
						Object: &ObjectWrapper{
							ResourceType: POD_JOB,
							Raw: &PodJob{
								Job: jobObj,
								Pod: pod,
							},
							FirstObservedTime: data.Object.FirstObservedTime,
							LastObservedTime:  data.Object.LastObservedTime,
						},
					})
				}
			}
		}
	}
	return result
}

func (g *LinkGenerator) getJobCronJobLink(jobList []*K8sMetaEvent) []*K8sMetaEvent {
	result := make([]*K8sMetaEvent, 0)
	for _, data := range jobList {
		job, ok := data.Object.Raw.(*batch.Job)
		if !ok || len(job.OwnerReferences) == 0 || job.OwnerReferences[0].Kind != K8S_CRONJOB_TYPE {
			continue
		}
		cronJobName := job.OwnerReferences[0].Name
		if cronJobName == "" {
			continue
		}
		cronJobList := g.metaCache[CRONJOB].Get([]string{generateNameWithNamespaceKey(job.Namespace, cronJobName)})
		for _, cj := range cronJobList {
			for _, c := range cj {
				if cronJob, ok := c.Raw.(*batch.CronJob); ok {
					result = append(result, &K8sMetaEvent{
						EventType: data.EventType,
						Object: &ObjectWrapper{
							ResourceType: JOB_CRONJOB,
							Raw: &JobCronJob{
								CronJob: cronJob,
								Job:     job,
							},
							FirstObservedTime: data.Object.FirstObservedTime,
							LastObservedTime:  data.Object.LastObservedTime,
						},
					})
				}
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
				if pvcName == "" {
					continue
				}
				pvcList := g.metaCache[PERSISTENTVOLUMECLAIM].Get([]string{generateNameWithNamespaceKey(pod.Namespace, pvcName)})
				for _, pvc := range pvcList {
					for _, p := range pvc {
						if pvcObj, ok := p.Raw.(*v1.PersistentVolumeClaim); ok {
							result = append(result, &K8sMetaEvent{
								EventType: data.EventType,
								Object: &ObjectWrapper{
									ResourceType: POD_PERSISENTVOLUMECLAIN,
									Raw: &PodPersistentVolumeClaim{
										Pod:                   pod,
										PersistentVolumeClaim: pvcObj,
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
				if cmName == "" {
					continue
				}
				cmList := g.metaCache[CONFIGMAP].Get([]string{generateNameWithNamespaceKey(pod.Namespace, cmName)})
				for _, cm := range cmList {
					for _, c := range cm {
						if configMap, ok := c.Raw.(*v1.ConfigMap); ok {
							result = append(result, &K8sMetaEvent{
								EventType: data.EventType,
								Object: &ObjectWrapper{
									ResourceType: POD_CONFIGMAP,
									Raw: &PodConfigMap{
										Pod:       pod,
										ConfigMap: configMap,
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
				if service, ok := s.obj.(*v1.Service); ok {
					result = append(result, &K8sMetaEvent{
						EventType: data.EventType,
						Object: &ObjectWrapper{
							ResourceType: POD_SERVICE,
							Raw: &PodService{
								Pod:     pod,
								Service: service,
							},
							FirstObservedTime: data.Object.FirstObservedTime,
							LastObservedTime:  data.Object.LastObservedTime,
						},
					})
				}
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
			if rule.HTTP != nil {
				for _, path := range rule.HTTP.Paths {
					if path.Backend.Service != nil && path.Backend.Service.Name != "" {
						serviceNameSet[path.Backend.Service.Name] = struct{}{}
					}
				}
			}
		}
		if len(serviceNameSet) == 0 {
			continue
		}
		serviceNameList := make([]string, 0, len(serviceNameSet))
		for name := range serviceNameSet {
			serviceNameList = append(serviceNameList, generateNameWithNamespaceKey(ingress.Namespace, name))
		}
		serviceList := g.metaCache[SERVICE].Get(serviceNameList)
		for _, service := range serviceList {
			for _, s := range service {
				if serviceObj, ok := s.Raw.(*v1.Service); ok {
					result = append(result, &K8sMetaEvent{
						EventType: data.EventType,
						Object: &ObjectWrapper{
							ResourceType: INGRESS_SERVICE,
							Raw: &IngressService{
								Ingress: ingress,
								Service: serviceObj,
							},
							FirstObservedTime: data.Object.FirstObservedTime,
							LastObservedTime:  data.Object.LastObservedTime,
						},
					})
				}
			}
		}
	}
	return result
}

func (g *LinkGenerator) getPodNamespaceLink(podList []*K8sMetaEvent) []*K8sMetaEvent {
	result := make([]*K8sMetaEvent, 0)
	for _, data := range podList {
		pod, ok := data.Object.Raw.(*v1.Pod)
		if !ok {
			continue
		}
		if g.ifNotStandAlonePod(pod) {
			// If a Pod can be associated with a Namespace indirectly through controllers like Job/Deployment/DaemonSet,
			// there is no need to explicitly establish a direct association between the Pod and Namespace,
			// as the association can already be achieved indirectly.
			continue
		}
		nsList := g.metaCache[NAMESPACE].Get([]string{generateNameWithNamespaceKey("", pod.Namespace)})
		for _, ns := range nsList {
			for _, n := range ns {
				if namespace, ok := n.Raw.(*v1.Namespace); ok {
					result = append(result, &K8sMetaEvent{
						EventType: data.EventType,
						Object: &ObjectWrapper{
							ResourceType: POD_NAMESPACE,
							Raw: &PodNamespace{
								Pod:       pod,
								Namespace: namespace,
							},
							FirstObservedTime: data.Object.FirstObservedTime,
							LastObservedTime:  data.Object.LastObservedTime,
						},
					})
				}
			}
		}
	}
	return result
}

func (g *LinkGenerator) ifNotStandAlonePod(pod *v1.Pod) bool {
	return len(pod.OwnerReferences) != 0
}

func (g *LinkGenerator) getServiceNamespaceLink(serviceList []*K8sMetaEvent) []*K8sMetaEvent {
	result := make([]*K8sMetaEvent, 0)
	for _, data := range serviceList {
		service, ok := data.Object.Raw.(*v1.Service)
		if !ok {
			continue
		}
		nsList := g.metaCache[NAMESPACE].Get([]string{generateNameWithNamespaceKey("", service.Namespace)})
		for _, ns := range nsList {
			for _, n := range ns {
				if namespace, ok := n.Raw.(*v1.Namespace); ok {
					result = append(result, &K8sMetaEvent{
						EventType: data.EventType,
						Object: &ObjectWrapper{
							ResourceType: SERVICE_NAMESPACE,
							Raw: &ServiceNamespace{
								Service:   service,
								Namespace: namespace,
							},
							FirstObservedTime: data.Object.FirstObservedTime,
							LastObservedTime:  data.Object.LastObservedTime,
						},
					})
				}
			}
		}
	}
	return result
}

func (g *LinkGenerator) getDeploymentNamespaceLink(deploymentList []*K8sMetaEvent) []*K8sMetaEvent {
	result := make([]*K8sMetaEvent, 0)
	for _, data := range deploymentList {
		deployment, ok := data.Object.Raw.(*app.Deployment)
		if !ok {
			continue
		}
		nsList := g.metaCache[NAMESPACE].Get([]string{generateNameWithNamespaceKey("", deployment.Namespace)})
		for _, ns := range nsList {
			for _, n := range ns {
				if namespace, ok := n.Raw.(*v1.Namespace); ok {
					result = append(result, &K8sMetaEvent{
						EventType: data.EventType,
						Object: &ObjectWrapper{
							ResourceType: DEPLOYMENT_NAMESPACE,
							Raw: &DeploymentNamespace{
								Deployment: deployment,
								Namespace:  namespace,
							},
							FirstObservedTime: data.Object.FirstObservedTime,
							LastObservedTime:  data.Object.LastObservedTime,
						},
					})
				}
			}
		}
	}
	return result
}
func (g *LinkGenerator) getDaemonSetNamespaceLink(daemonsetList []*K8sMetaEvent) []*K8sMetaEvent {
	result := make([]*K8sMetaEvent, 0)
	for _, data := range daemonsetList {
		daemonset, ok := data.Object.Raw.(*app.DaemonSet)
		if !ok {
			continue
		}
		nsList := g.metaCache[NAMESPACE].Get([]string{generateNameWithNamespaceKey("", daemonset.Namespace)})
		for _, ns := range nsList {
			for _, n := range ns {
				if namespace, ok := n.Raw.(*v1.Namespace); ok {
					result = append(result, &K8sMetaEvent{
						EventType: data.EventType,
						Object: &ObjectWrapper{
							ResourceType: DAEMONSET_NAMESPACE,
							Raw: &DaemonSetNamespace{
								DaemonSet: daemonset,
								Namespace: namespace,
							},
							FirstObservedTime: data.Object.FirstObservedTime,
							LastObservedTime:  data.Object.LastObservedTime,
						},
					})
				}
			}
		}
	}
	return result
}
func (g *LinkGenerator) getStatefulsetNamespaceLink(statefulsetList []*K8sMetaEvent) []*K8sMetaEvent {
	result := make([]*K8sMetaEvent, 0)
	for _, data := range statefulsetList {
		statefulset, ok := data.Object.Raw.(*app.StatefulSet)
		if !ok {
			continue
		}
		nsList := g.metaCache[NAMESPACE].Get([]string{generateNameWithNamespaceKey("", statefulset.Namespace)})
		for _, ns := range nsList {
			for _, n := range ns {
				if namespace, ok := n.Raw.(*v1.Namespace); ok {
					result = append(result, &K8sMetaEvent{
						EventType: data.EventType,
						Object: &ObjectWrapper{
							ResourceType: STATEFULSET_NAMESPACE,
							Raw: &StatefulSetNamespace{
								StatefulSet: statefulset,
								Namespace:   namespace,
							},
							FirstObservedTime: data.Object.FirstObservedTime,
							LastObservedTime:  data.Object.LastObservedTime,
						},
					})
				}
			}
		}
	}
	return result
}
func (g *LinkGenerator) getConfigMapNamespaceLink(configMapList []*K8sMetaEvent) []*K8sMetaEvent {
	result := make([]*K8sMetaEvent, 0)
	for _, data := range configMapList {
		configmap, ok := data.Object.Raw.(*v1.ConfigMap)
		if !ok {
			continue
		}
		nsList := g.metaCache[NAMESPACE].Get([]string{generateNameWithNamespaceKey("", configmap.Namespace)})
		for _, ns := range nsList {
			for _, n := range ns {
				if namespace, ok := n.Raw.(*v1.Namespace); ok {
					result = append(result, &K8sMetaEvent{
						EventType: data.EventType,
						Object: &ObjectWrapper{
							ResourceType: CONFIGMAP_NAMESPACE,
							Raw: &ConfigMapNamespace{
								ConfigMap: configmap,
								Namespace: namespace,
							},
							FirstObservedTime: data.Object.FirstObservedTime,
							LastObservedTime:  data.Object.LastObservedTime,
						},
					})
				}
			}
		}
	}
	return result
}
func (g *LinkGenerator) getJobNamespaceLink(jobList []*K8sMetaEvent) []*K8sMetaEvent {
	result := make([]*K8sMetaEvent, 0)
	for _, data := range jobList {
		job, ok := data.Object.Raw.(*batch.Job)
		if !ok {
			continue
		}
		nsList := g.metaCache[NAMESPACE].Get([]string{generateNameWithNamespaceKey("", job.Namespace)})
		for _, ns := range nsList {
			for _, n := range ns {
				if namespace, ok := n.Raw.(*v1.Namespace); ok {
					result = append(result, &K8sMetaEvent{
						EventType: data.EventType,
						Object: &ObjectWrapper{
							ResourceType: JOB_NAMESPACE,
							Raw: &JobNamespace{
								Job:       job,
								Namespace: namespace,
							},
							FirstObservedTime: data.Object.FirstObservedTime,
							LastObservedTime:  data.Object.LastObservedTime,
						},
					})
				}
			}
		}
	}
	return result
}
func (g *LinkGenerator) getCronJobNamespaceLink(jobList []*K8sMetaEvent) []*K8sMetaEvent {
	result := make([]*K8sMetaEvent, 0)
	for _, data := range jobList {
		job, ok := data.Object.Raw.(*batch.CronJob)
		if !ok {
			continue
		}
		nsList := g.metaCache[NAMESPACE].Get([]string{generateNameWithNamespaceKey("", job.Namespace)})
		for _, ns := range nsList {
			for _, n := range ns {
				if namespace, ok := n.Raw.(*v1.Namespace); ok {
					result = append(result, &K8sMetaEvent{
						EventType: data.EventType,
						Object: &ObjectWrapper{
							ResourceType: CRONJOB_NAMESPACE,
							Raw: &CronJobNamespace{
								CronJob:   job,
								Namespace: namespace,
							},
							FirstObservedTime: data.Object.FirstObservedTime,
							LastObservedTime:  data.Object.LastObservedTime,
						},
					})
				}
			}
		}
	}
	return result
}

func (g *LinkGenerator) getPVCNamespaceLink(pvcList []*K8sMetaEvent) []*K8sMetaEvent {
	result := make([]*K8sMetaEvent, 0)
	for _, data := range pvcList {
		pvc, ok := data.Object.Raw.(*v1.PersistentVolumeClaim)
		if !ok {
			continue
		}
		nsList := g.metaCache[NAMESPACE].Get([]string{generateNameWithNamespaceKey("", pvc.Namespace)})
		for _, ns := range nsList {
			for _, n := range ns {
				if namespace, ok := n.Raw.(*v1.Namespace); ok {
					result = append(result, &K8sMetaEvent{
						EventType: data.EventType,
						Object: &ObjectWrapper{
							ResourceType: PERSISTENTVOLUMECLAIM_NAMESPACE,
							Raw: &PersistentVolumeClaimNamespace{
								PersistentVolumeClaim: pvc,
								Namespace:             namespace,
							},
							FirstObservedTime: data.Object.FirstObservedTime,
							LastObservedTime:  data.Object.LastObservedTime,
						},
					})
				}
			}
		}
	}
	return result
}

func (g *LinkGenerator) getIngressNamespaceLink(ingressList []*K8sMetaEvent) []*K8sMetaEvent {
	result := make([]*K8sMetaEvent, 0)
	for _, data := range ingressList {
		ingress, ok := data.Object.Raw.(*networking.Ingress)
		if !ok {
			continue
		}
		nsList := g.metaCache[NAMESPACE].Get([]string{generateNameWithNamespaceKey("", ingress.Namespace)})
		for _, ns := range nsList {
			for _, n := range ns {
				if namespace, ok := n.Raw.(*v1.Namespace); ok {
					result = append(result, &K8sMetaEvent{
						EventType: data.EventType,
						Object: &ObjectWrapper{
							ResourceType: INGRESS_NAMESPACE,
							Raw: &IngressNamespace{
								Ingress:   ingress,
								Namespace: namespace,
							},
							FirstObservedTime: data.Object.FirstObservedTime,
							LastObservedTime:  data.Object.LastObservedTime,
						},
					})
				}
			}
		}
	}
	return result
}
