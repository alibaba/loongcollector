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
	"encoding/json"
	"flag"
	"fmt"
	"io"
	"net/http"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"strings"
	"text/template"

	"gopkg.in/yaml.v3"
)

// this tool is for generate files integrating plugins
// usage: builder -root <project-root-dir> -config <plugins-config-file.yaml> -modfile <custom-go.mod-file>

var configFile = flag.String("config", "plugins.yml,external_plugins.yml", "file path or url including plugins config")
var projectRoot = flag.String("root-dir", "./", "project root directory")
var modFile = flag.String("modfile", "go.mod", "alternative mod file to the go.mod, generated mod file will write to here")
var goExe = flag.String("go-exe", "go", "the go executable binary path")
var gitExe = flag.String("git-exe", "git", "the git executable binary path")

func main() {
	flag.Parse()

	goModContent, err := os.ReadFile(filepath.Join(*projectRoot, "go.mod"))
	if err != nil {
		fmt.Println("failed to read go.mod in project root:", *projectRoot)
		os.Exit(1)
	}

	files := strings.Split(*configFile, ",")
	if *configFile == "" || len(files) == 0 {
		fmt.Println("no plugins config found")
		return
	}
	conf := &pluginConfig{}
	for _, file := range files {
		file = getAbsPath(file, *projectRoot)
		if _, err = os.Stat(file); err != nil {
			fmt.Println("plugins config file not found(skipped):", file, "error:", err)
			continue
		}
		c, e := loadPluginConfig(file)
		if e != nil {
			fmt.Println("fail to load config from file:", file, "error:", e)
			os.Exit(1)
		}
		conf.Merge(c)
	}

	if conf.Plugins.IsEmpty() {
		fmt.Println("empty plugins config file, nothing to do")
		return
	}

	ctx := buildContext{
		Config:       *conf,
		ProjectRoot:  *projectRoot,
		ModFile:      *modFile,
		GoExe:        *goExe,
		GitExe:       *gitExe,
		GoModContent: string(goModContent),
	}
	fmt.Println("using plugin config:", mustMarshal2Json(ctx))

	err = generatePluginSourceCode(&ctx)
	if err != nil {
		fmt.Println("failed to generate code for config, err:", err)
		os.Exit(1)
	}

	err = applyGitConfigs(&ctx)
	if err != nil {
		fmt.Println("failed to apply git configs, err:", err)
		os.Exit(1)
	}

	err = applyGoEnvs(&ctx)
	if err != nil {
		fmt.Println("failed to apply go envs, err:", err)
		os.Exit(1)
	}

	err = getGoModules(&ctx)
	if err != nil {
		fmt.Println("failed to get modules, err:", err)
		os.Exit(1)
	}
}

func loadPluginConfig(file string) (*pluginConfig, error) {
	var content []byte
	if ok, _ := regexp.MatchString("^http(s)?://", strings.ToLower(file)); ok {
		resp, err := http.Get(file) // nolint
		if err != nil {
			return nil, err
		}
		defer resp.Body.Close()
		content, err = io.ReadAll(resp.Body)
		if err != nil {
			return nil, err
		}
	} else {
		var err error
		content, err = os.ReadFile(file) // nolint
		if err != nil {
			return nil, err
		}
	}

	var conf pluginConfig
	err := yaml.Unmarshal(content, &conf)
	if err != nil {
		return nil, err
	}
	err = enrichPluginConfig(&conf)
	if err != nil {
		return nil, err
	}
	return &conf, nil
}

func enrichPluginConfig(conf *pluginConfig) error {
	groups := [][]*pluginModule{
		conf.Plugins.Common,
		conf.Plugins.Windows,
		conf.Plugins.Linux,
		conf.Plugins.Debug,
	}
	for _, mods := range groups {
		for _, mod := range mods {
			if mod.Import == "" {
				mod.Import = strings.Split(mod.GoMod, " ")[0]
			}
			if mod.Path != "" {
				var err error
				if mod.Path, err = filepath.Abs(mod.Path); err != nil {
					return fmt.Errorf("couldn't resolve absolute dir of path: %s, err: %s", mod.Path, err)
				}
			}
		}
	}
	return nil
}

func generatePluginSourceCode(ctx *buildContext) error {
	// generate plugins/all/*.go
	for _, tmpl := range []*template.Template{
		allPluginsTemplate,
		windowsPluginsTemplate,
		linuxPluginsTemplate,
		debugPluginsTemplate,
	} {
		path := filepath.Clean(filepath.Join(ctx.ProjectRoot, "plugins/all", tmpl.Name()))
		err := os.MkdirAll(filepath.Dir(path), os.ModePerm)
		if err != nil {
			return err
		}
		outFile, err := os.Create(path)
		if err != nil {
			return fmt.Errorf("failed to create file: %s, err: %s", path, err)
		}
		err = tmpl.Execute(outFile, ctx)
		if err != nil {
			return fmt.Errorf("failed to generate file: %s, err: %s", path, err)
		}
		fmt.Println("generate file:", path)
	}

	// generate go.mod
	path := getAbsPath(ctx.ModFile, ctx.ProjectRoot)
	if !ctx.Config.Plugins.NeedUpdateGoMod() {
		fmt.Println("skip generate file:", path)
		return nil
	}
	outFile, err := os.Create(path) // nolint
	if err != nil {
		return fmt.Errorf("failed to generate file: %s, err: %s", path, err)
	}

	err = goModTemplate.Execute(outFile, ctx)
	if err != nil {
		return fmt.Errorf("failed to generate file: %s, err: %s", path, err)
	}
	fmt.Println("generate file:", path)

	return nil
}

func applyGitConfigs(ctx *buildContext) error {
	if len(ctx.Config.Project.GitConfigs) == 0 {
		return nil
	}
	for k, v := range ctx.Config.Project.GitConfigs {
		cmd := exec.Command(ctx.GitExe, "config", "--global", k, v)
		cmd.Dir = ctx.ProjectRoot
		out, err := cmd.CombinedOutput()
		if err != nil {
			return fmt.Errorf("failed to apply git config, err: %w, output: %s", err, out)
		}
		fmt.Println("apply git config:", k, v)
	}
	return nil
}

func applyGoEnvs(ctx *buildContext) error {
	if len(ctx.Config.Project.GoEnvs) == 0 {
		return nil
	}
	for k, v := range ctx.Config.Project.GoEnvs {
		value := fmt.Sprintf("%s=%s", k, v)
		cmd := exec.Command(ctx.GoExe, "env", "-w", value)
		cmd.Dir = ctx.ProjectRoot
		out, err := cmd.CombinedOutput()
		if err != nil {
			return fmt.Errorf("failed to apply go env, err: %w, output: %s", err, out)
		}
		fmt.Println("apply go env:", k, v)
	}
	return nil
}

func getGoModules(ctx *buildContext) error {
	cmd := exec.Command(ctx.GoExe, "mod", "tidy", "-modfile", ctx.ModFile)
	cmd.Dir = ctx.ProjectRoot
	out, err := cmd.CombinedOutput()
	if err != nil {
		return fmt.Errorf("failed to execute go mod tidy, err: %w, output: %s", err, out)
	}

	cmd = exec.Command(ctx.GoExe, "mod", "download", "-modfile", ctx.ModFile)
	cmd.Dir = ctx.ProjectRoot
	out, err = cmd.CombinedOutput()
	if err != nil {
		return fmt.Errorf("failed to download go modules, err: %w, output: %s", err, out)
	}

	path := getAbsPath(ctx.ModFile, ctx.ProjectRoot)
	mods, err := os.ReadFile(path) // nolint
	if err != nil {
		return fmt.Errorf("failed to read file content, err: %v", err)
	}
	fmt.Printf("generated gomod file content:\n%s", string(mods))
	return nil
}

func mustMarshal2Json(data interface{}) string {
	bytes, err := json.MarshalIndent(data, "", "    ")
	if err != nil {
		panic(err)
	}
	return string(bytes)
}

func getAbsPath(path string, root string) string {
	if filepath.IsAbs(path) {
		return path
	}
	return filepath.Join(root, path)
}
