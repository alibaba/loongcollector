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

package doc

var docCenter = make(map[string]map[string]Doc)

// Doc generates markdown doc with the exported field.
// If some field want to add comment, please use comment tag to describe it.
type Doc interface {
	Description() string
}

// Register the doc description to the doc center.
func Register(category, name string, doc Doc) {
	_, ok := docCenter[category]
	if !ok {
		docCenter[category] = make(map[string]Doc)
	}
	docCenter[category][name] = doc
}
