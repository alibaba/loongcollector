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

type pluginConfig struct {
	Plugins pluginCategory `yaml:"plugins"`
	Project projectConfig  `yaml:"project"`
}

type projectConfig struct {
	Replaces   []string          `yaml:"replaces"`    // replaces add to go.mod, used to resolve dependency conflict between different plugins
	GitConfigs map[string]string `yaml:"git_configs"` // you may need to specify url.insteadof config to add access token or change url to ssh, see https://go.dev/doc/faq#git_https
	GoEnvs     map[string]string `yaml:"go_envs"`     // you may need to set GOPRIVATE env for some of your plugin modules
}

func (p *pluginConfig) Merge(conf *pluginConfig) {
	p.Plugins.Linux = append(p.Plugins.Linux, conf.Plugins.Linux...)
	p.Plugins.Windows = append(p.Plugins.Windows, conf.Plugins.Windows...)
	p.Plugins.Debug = append(p.Plugins.Debug, conf.Plugins.Debug...)
	p.Plugins.Common = append(p.Plugins.Common, conf.Plugins.Common...)

	if p.Project.GoEnvs == nil {
		p.Project.GoEnvs = make(map[string]string)
	}

	if p.Project.GitConfigs == nil {
		p.Project.GitConfigs = make(map[string]string)
	}

	for k, v := range conf.Project.GoEnvs {
		p.Project.GoEnvs[k] = v
	}
	for k, v := range conf.Project.GitConfigs {
		p.Project.GitConfigs[k] = v
	}
	p.Project.Replaces = append(p.Project.Replaces, conf.Project.Replaces...)
}

type pluginCategory struct {
	Common  []*pluginModule `yaml:"common"`  // plugins that available on all os
	Windows []*pluginModule `yaml:"windows"` // plugins that available only on Windows
	Linux   []*pluginModule `yaml:"linux"`   // plugins that available only on linux
	Debug   []*pluginModule `yaml:"debug"`   // plugins that available only when debugging
}

func (p *pluginCategory) IsEmpty() bool {
	return len(p.Common)|len(p.Windows)|len(p.Linux)|len(p.Debug) == 0
}

func (p *pluginCategory) NeedUpdateGoMod() bool {
	groups := [][]*pluginModule{p.Common, p.Linux, p.Debug, p.Windows}
	for _, group := range groups {
		for _, module := range group {
			if module.GoMod != "" {
				return true
			}
		}
	}
	return false
}

type pluginModule struct {
	GoMod  string `yaml:"gomod"`  // gomod spec for the module
	Import string `yaml:"import"` // if not specified, this is the path part of the gomod
	Path   string `yaml:"path"`   // optional path to the local version of this module
}

type buildContext struct {
	ProjectRoot  string
	GoModContent string `json:"-"`
	ModFile      string
	GoExe        string
	GitExe       string
	Config       pluginConfig
}
