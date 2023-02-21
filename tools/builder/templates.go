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

package main

import (
	_ "embed"
	"text/template"
)

var (
	//go:embed templates/all.go.tmpl
	allPluginsTmplBytes []byte
	allPluginsTemplate  = parseTemplate("all.go", allPluginsTmplBytes)

	//go:embed templates/all_windows.go.tmpl
	windowsPluginsTmplBytes []byte
	windowsPluginsTemplate  = parseTemplate("all_windows.go", windowsPluginsTmplBytes)

	//go:embed templates/all_linux.go.tmpl
	linuxPluginsTmplBytes []byte
	linuxPluginsTemplate  = parseTemplate("all_linux.go", linuxPluginsTmplBytes)

	//go:embed templates/all_debug.go.tmpl
	debugPluginsTmplBytes []byte
	debugPluginsTemplate  = parseTemplate("all_debug.go", debugPluginsTmplBytes)

	//go:embed templates/go.mod.tmpl
	goModTmplBytes []byte
	goModTemplate  = parseTemplate("go.mod", goModTmplBytes)
)

func parseTemplate(name string, bytes []byte) *template.Template {
	return template.Must(template.New(name).Parse(string(bytes)))
}
