// Copyright 2021 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//	http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package command

const (
	pluginType              = "input_command"
	defaultContentType      = "PlainText"
	defaultIntervalMs       = 5000 // The default is Alibaba Cloud's collection frequency of 5s
	defaltExecScriptTimeOut = 3000 // Default 3 seconds timeout
	UserRoot                = "root"
	ContentTypeBase64       = "Base64"
	ContentTypePlainText    = "PlainText"
	ScriptMd5               = "script_md5"
)

type ScriptMeta struct {
	scriptSuffix   string
	defaultCmdPath string
}

// ScriptTypeToSuffix Supported script types
var ScriptTypeToSuffix = map[string]ScriptMeta{
	"bash": {
		scriptSuffix:   "sh",
		defaultCmdPath: "/usr/bin/bash",
	},
	"shell": {
		scriptSuffix:   "sh",
		defaultCmdPath: "/usr/bin/sh",
	},
	"python2": {
		scriptSuffix:   "py",
		defaultCmdPath: "/usr/bin/python2",
	},
	"python3": {
		scriptSuffix:   "py",
		defaultCmdPath: "/usr/bin/python3",
	},
}

// SupportContentType Supported content types
var SupportContentType = map[string]bool{
	ContentTypePlainText: true,
	ContentTypeBase64:    true,
}

const (
	errWaitNoChild   = "wait: no child processes"
	errWaitIDNoChild = "waitid: no child processes"
)
