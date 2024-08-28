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

import (
	"encoding/json"
	"fmt"
	"os"
	"path/filepath"
	"reflect"
	"sort"
)

const (
	topLevel       = "# "
	secondLevel    = "## "
	lf             = "\n"
	markdownSuffix = ".md"
	tableSplitor   = "|"
)

type FieldConfig struct {
	Name    string
	Type    string
	Comment string
	Default string
}

// Generate plugin doc to the path/category directory.
func Generate(path string) {
	fileName := path + "/plugin-list" + markdownSuffix
	str := topLevel + "Plugin List" + lf

	var sortedCategories []string
	for category := range docCenter {
		sortedCategories = append(sortedCategories, category)
	}
	sort.Strings(sortedCategories)
	for _, category := range sortedCategories {
		plugins := docCenter[category]
		home := filepath.Clean(path + "/" + category)
		_ = os.MkdirAll(home, 0750)
		str += "- " + category + lf
		var pluginNames []string
		for name := range plugins {
			pluginNames = append(pluginNames, name)
		}
		sort.Strings(pluginNames)
		for _, name := range pluginNames {
			pluginFile := home + "/" + name + markdownSuffix
			relativeFile := category + "/" + name + markdownSuffix
			generatePluginDoc(pluginFile, name, docCenter[category][name])
			str += "	- [" + name + "](" + relativeFile + ")" + lf
		}
	}
	_ = os.WriteFile(fileName, []byte(str), 0600)
}

func generatePluginDoc(fileName, pluginType string, doc Doc) {
	str := topLevel + pluginType + lf
	str += secondLevel + "Description" + lf
	str += doc.Description() + lf
	str += secondLevel + "Config" + lf
	if configs := extractDocConfig(doc); len(configs) > 0 {
		str += `|  field   |   type   |   description   | default value   |
| ---- | ---- | ---- | ---- |`
		for _, config := range configs {
			str += lf + tableSplitor + config.Name + tableSplitor + config.Type + tableSplitor + config.Comment + tableSplitor + config.Default + tableSplitor
		}
	}
	_ = os.WriteFile(fileName, []byte(str), 0600)
}

func extractDocConfig(doc Doc) (configs []*FieldConfig) {
	rt := reflect.TypeOf(doc)
	val := reflect.ValueOf(doc)
	if rt.Kind() == reflect.Ptr {
		rt = rt.Elem()
		val = val.Elem()
	}
	overrideName := func(index int, tag string, field *FieldConfig) {
		name := rt.Field(index).Tag.Get(tag)
		if name != "" {
			field.Name = name
		}
	}
	for i := 0; i < rt.NumField(); i++ {
		if rt.Field(i).PkgPath != "" {
			continue
		}
		field := &FieldConfig{
			Name:    rt.Field(i).Name,
			Comment: rt.Field(i).Tag.Get("comment"),
		}
		if field.Comment == "" {
			continue
		}

		field.Type = rt.Field(i).Type.String()

		overrideName(i, "yaml", field)
		overrideName(i, "json", field)
		overrideName(i, "mapstructure", field)

		bytes, err := json.Marshal(val.Field(i).Interface())
		if err != nil {
			panic(fmt.Errorf("cannot extract default value: %+v", err))
		}
		field.Default = string(bytes)
		configs = append(configs, field)
	}
	return
}
