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

package helper

import (
	"github.com/alibaba/ilogtail/pkg/pipeline"

	"time"
	"unsafe"
)

var metaKeys = make([]string, 0, 5)

const (
	defaultLen = 16
	empty      = "{}"
	emptyArr   = "[]"
)

// MetaNode describes a superset of the metadata that probes can collect
// about a given node in a given topology, along with the edges (aka
// adjacency) emanating from the node.
type MetaNode struct {
	ID         string
	Type       string
	Attributes Attributes
	Labels     Labels
	Parents    Parents
}

// Attributes used to store attributes in common conditions.
//
//easyjson:json
type Attributes map[string]interface{}

//easyjson:json
type Labels map[string]string

//easyjson:json
type Parents []string

func NewMetaNode(id, nodeType string) *MetaNode {
	return &MetaNode{
		ID:   id,
		Type: nodeType,
	}
}

func (n *MetaNode) WithLabels(labels Labels) *MetaNode {
	n.Labels = labels
	return n
}

func (n *MetaNode) WithAttributes(attributes Attributes) *MetaNode {
	n.Attributes = attributes
	return n
}

func (n *MetaNode) WithParents(parents Parents) *MetaNode {
	n.Parents = parents
	return n
}

func (n *MetaNode) WithParent(key, parentID, parentName string) *MetaNode {
	n.Parents = append(n.Parents, key+":"+parentID+":"+parentName)
	return n
}

func (n *MetaNode) WithLabel(k, v string) *MetaNode {
	if n.Labels == nil {
		n.Labels = make(Labels, defaultLen)
	}
	n.Labels[k] = v
	return n
}

func (n *MetaNode) WithAttribute(k string, v interface{}) *MetaNode {
	if n.Attributes == nil {
		n.Attributes = make(Attributes, defaultLen)
	}
	n.Attributes[k] = v
	return n
}

// AddMetadata to the collector.
func AddMetadata(collector pipeline.Collector, time time.Time, node *MetaNode) {
	keys, vals := makeMetaLog(node)
	collector.AddDataArray(nil, keys, vals, time)
}

// makeMetaLog convert MetaNode to the log of Logtail
//
//nolint:gosec
func makeMetaLog(node *MetaNode) (keys, values []string) {
	values = make([]string, 5)
	values[0] = node.ID
	values[1] = node.Type
	if node.Attributes != nil {
		bytes, _ := node.Attributes.MarshalJSON()
		values[2] = *(*string)(unsafe.Pointer(&bytes))
	} else {
		values[2] = empty
	}
	if len(node.Labels) == 0 {
		values[3] = empty
	} else {
		bytes, _ := node.Labels.MarshalJSON()
		values[3] = *(*string)(unsafe.Pointer(&bytes))
	}
	if len(node.Parents) == 0 {
		values[4] = emptyArr
	} else {
		bytes, _ := node.Parents.MarshalJSON()
		values[4] = *(*string)(unsafe.Pointer(&bytes))
	}
	return metaKeys, values
}
func init() {
	metaKeys = append(metaKeys, "id", "type", "attributes", "labels", "parents")
}
