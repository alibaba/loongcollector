// Copyright 2022 iLogtail Authors
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

type ConvertConfig struct {
	TagFieldsRename      map[string]string // Rename one or more fields from tags.
	ProtocolFieldsRename map[string]string // Rename one or more fields, The protocol field options can only be: contents, tags, time
	Separator            string            // Convert separator
	Protocol             string            // Convert protocol
	Encoding             string            // Convert encoding
	IgnoreUnExpectedData bool              // IgnoreUnExpectedData will skip on unexpected data if set to true, or will return error and stop processing the whole batch data if set to false
}
