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

package kubernetesmeta

import (
	"strconv"
	"strings"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"

	"k8s.io/apimachinery/pkg/labels"
	networking "k8s.io/client-go/listers/networking/v1beta1"
)

// collectIngresses list the kubernetes Ingresses by the label selector and collect the core metadata.
func (in *InputKubernetesMeta) collectIngresses(lister interface{}, selector labels.Selector) (nodes []*helper.MetaNode, err error) {
	ingresses, err := lister.(networking.IngressLister).List(selector)
	if err != nil {
		logger.Error(in.context.GetRuntimeContext(), "KUBERNETES_META_ALARM", "err", err)
		return
	}
	if in.Ingress {
		nodes = make([]*helper.MetaNode, 0, len(ingresses))
	}
	for _, i := range ingresses {
		id := string(i.UID)
		if !in.DisableReportParents && len(i.Spec.Rules) > 0 {
			refs := make(map[string]struct{}, 16)
			for _, rule := range i.Spec.Rules {
				for _, path := range rule.HTTP.Paths {
					refs[path.Backend.ServiceName] = struct{}{}
				}
			}
			in.addIngressMapping(i.Namespace, id, i.Name, refs)
		}
		if in.Ingress {
			node := helper.NewMetaNode(id, Ingress).WithAttributes(make(helper.Attributes, 4)).WithLabels(i.Labels)
			if len(i.Status.LoadBalancer.Ingress) > 0 {
				ips := make([]string, 0, len(i.Status.LoadBalancer.Ingress))
				for _, ingress := range i.Status.LoadBalancer.Ingress {
					ips = append(ips, ingress.IP)
				}
				node.WithAttribute(KeyLoadBalancerIP, strings.Join(ips, ","))
			}
			if len(i.Spec.Rules) > 0 {
				rules := make([]map[string]interface{}, 0, len(i.Spec.Rules))
				for _, rule := range i.Spec.Rules {
					if rule.HTTP != nil {
						paths := make([]string, 0, len(rule.HTTP.Paths))
						for _, path := range rule.HTTP.Paths {
							paths = append(paths, path.Backend.ServiceName+":"+strconv.Itoa(int(path.Backend.ServicePort.IntVal))+":"+path.Path)
						}
						rules = append(rules, map[string]interface{}{
							"host":  rule.Host,
							"paths": paths,
						})
					}
				}
				node.WithAttribute(KeyRules, rules)
			}
			addCommonAttributes(&i.ObjectMeta, node).WithAttribute(KeyNamespace, i.Namespace)
			nodes = append(nodes, node)
		}
	}
	return
}
