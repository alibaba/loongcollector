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
	"k8s.io/apimachinery/pkg/labels"
	storage "k8s.io/client-go/listers/storage/v1"

	"github.com/alibaba/ilogtail/pkg/helper"
	"github.com/alibaba/ilogtail/pkg/logger"
)

// collectStorageClass list the kubernetes StorageClass by the label selector and collect the core metadata.
func (in *InputKubernetesMeta) collectStorageClass(lister interface{}, selector labels.Selector) (nodes []*helper.MetaNode, err error) {
	if !in.StorageClass {
		return
	}
	storageClasses, err := lister.(storage.StorageClassLister).List(selector)
	if err != nil {
		logger.Error(in.context.GetRuntimeContext(), "KUBERNETES_META_ALARM", "err", err)
		return
	}
	nodes = make([]*helper.MetaNode, 0, len(storageClasses))
	for _, sc := range storageClasses {
		node := helper.NewMetaNode(string(sc.UID), StorageClass).WithAttributes(make(helper.Attributes, 4)).WithLabels(sc.Labels).
			WithAttribute(KeyProvisioner, sc.Provisioner)
		addCommonAttributes(&sc.ObjectMeta, node)
		nodes = append(nodes, node)
	}
	return
}
