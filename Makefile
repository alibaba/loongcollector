# Copyright 2021 iLogtail Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

.DEFAULT_GOAL := all
VERSION ?= 2.0.0
DOCKER_PUSH ?= false
DOCKER_REPOSITORY ?= aliyun/ilogtail
BUILD_REPOSITORY ?= aliyun/ilogtail_build
GENERATED_HOME ?= generated_files
PLUGINS_CONFIG_FILE ?= "plugins.yml,external_plugins.yml"
GO_MOD_FILE ?= go.mod
DOCKER_BUILD_EXPORT_GO_ENVS ?= true
DOCKER_BUILD_COPY_GIT_CONFIGS ?= true
DOCKER_BUILD_USE_BUILDKIT ?=

SCOPE ?= .

TEST_DEBUG ?= false
TEST_PROFILE ?= false
TEST_SCOPE ?= "all"

PLATFORMS := linux darwin windows

ifeq ($(shell uname -m),x86_64)
    ARCH := amd64
else
    ARCH := arm64
endif

ifndef DOCKER_BUILD_USE_BUILDKIT
	DOCKER_BUILD_USE_BUILDKIT = true

	# docker BuildKit supported start from 19.03
	docker_version := $(shell docker version --format '{{.Server.Version}}')
	least_version := "19.03"
	ifeq ($(shell printf "$(least_version)\n$(docker_version)" | sort -V | tail -n 1),$(least_version))
		DOCKER_BUILD_USE_BUILDKIT = false
	endif

	# check if SSH_AUTH_SOCK env set which means ssh-agent running
	ifndef SSH_AUTH_SOCK
		DOCKER_BUILD_USE_BUILDKIT = false
	endif
endif

GO = go
GO_PATH = $$($(GO) env GOPATH)
GO_BUILD = $(GO) build
GO_GET = $(GO) get
GO_TEST = $(GO) test
GO_LINT = golangci-lint
GO_ADDLICENSE = $(GO_PATH)/bin/addlicense
GO_PACKR = $(GO_PATH)/bin/packr2
GO_BUILD_FLAGS = -v

LICENSE_COVERAGE_FILE=license_coverage.txt
OUT_DIR = output
DIST_DIR = dist
PACKAGE_DIR = ilogtail-$(VERSION)
EXTERNAL_DIR = external
DIST_FILE = $(DIST_DIR)/ilogtail-$(VERSION).linux-$(ARCH).tar.gz

.PHONY: tools
tools:
	$(GO_LINT) version || curl -sfL https://raw.githubusercontent.com/golangci/golangci-lint/master/install.sh | sh -s -- -b $(GO_PATH)/bin v1.49.0
	$(GO_ADDLICENSE) version || go install github.com/google/addlicense@latest

.PHONY: clean
clean:
	rm -rf $(LICENSE_COVERAGE_FILE)
	rm -rf $(OUT_DIR) $(DIST_DIR)
	rm -rf behavior-test
	rm -rf performance-test
	rm -rf core-test
	rm -rf e2e-engine-coverage.txt
	rm -rf find_licenses
	rm -rf $(GENERATED_HOME)
	rm -rf .testCoverage.txt
	rm -rf .coretestCoverage.txt
	rm -rf core/build
	rm -rf plugin_main/*.dll
	rm -rf plugin_main/*.so
	rm -rf plugins/all/all.go
	rm -rf plugins/all/all_debug.go
	rm -rf plugins/all/all_windows.go
	rm -rf plugins/all/all_linux.go
	go mod tidy -modfile $(GO_MOD_FILE) || true

.PHONY: license
license:  clean tools import_plugins
	./scripts/package_license.sh add-license $(SCOPE)

.PHONY: check-license
check-license: clean tools import_plugins
	./scripts/package_license.sh check $(SCOPE) | tee $(LICENSE_COVERAGE_FILE)

.PHONY: lint
lint: clean tools
	$(GO_LINT) run -v --timeout 10m $(SCOPE)/... && make lint-pkg && make lint-e2e

.PHONY: lint-pkg
lint-pkg: clean tools
	cd pkg && pwd && $(GO_LINT) run -v --timeout 5m ./...

.PHONY: lint-test
lint-e2e: clean tools
	cd test && pwd && $(GO_LINT) run -v --timeout 5m ./...

.PHONY: core
core: clean import_plugins
	./scripts/gen_build_scripts.sh core $(GENERATED_HOME) $(VERSION) $(BUILD_REPOSITORY) $(OUT_DIR) $(DOCKER_BUILD_EXPORT_GO_ENVS) $(DOCKER_BUILD_COPY_GIT_CONFIGS) $(PLUGINS_CONFIG_FILE) $(GO_MOD_FILE)
	./scripts/docker_build.sh build $(GENERATED_HOME) $(VERSION) $(BUILD_REPOSITORY) false $(DOCKER_BUILD_USE_BUILDKIT)
	./$(GENERATED_HOME)/gen_copy_docker.sh

.PHONY: plugin
plugin: clean import_plugins
	./scripts/gen_build_scripts.sh plugin $(GENERATED_HOME) $(VERSION) $(BUILD_REPOSITORY) $(OUT_DIR) $(DOCKER_BUILD_EXPORT_GO_ENVS) $(DOCKER_BUILD_COPY_GIT_CONFIGS) $(PLUGINS_CONFIG_FILE) $(GO_MOD_FILE)
	./scripts/docker_build.sh build $(GENERATED_HOME) $(VERSION) $(BUILD_REPOSITORY) false $(DOCKER_BUILD_USE_BUILDKIT)
	./$(GENERATED_HOME)/gen_copy_docker.sh

.PHONY: upgrade_adapter_lib
upgrade_adapter_lib:
	./scripts/upgrade_adapter_lib.sh ${OUT_DIR}

.PHONY: plugin_main
plugin_main: clean
	./scripts/plugin_build.sh mod default $(OUT_DIR) $(VERSION) $(PLUGINS_CONFIG_FILE) $(GO_MOD_FILE)
	cp pkg/logtail/libPluginAdapter.so $(OUT_DIR)/libPluginAdapter.so
	cp pkg/logtail/PluginAdapter.dll $(OUT_DIR)/PluginAdapter.dll

.PHONY: plugin_local
plugin_local:
	./scripts/plugin_build.sh mod c-shared $(OUT_DIR) $(VERSION) $(PLUGINS_CONFIG_FILE) $(GO_MOD_FILE)

.PHONY: import_plugins
import_plugins:
	./scripts/import_plugins.sh $(PLUGINS_CONFIG_FILE) $(GO_MOD_FILE)

.PHONY: e2edocker
e2edocker: clean import_plugins
	./scripts/gen_build_scripts.sh e2e $(GENERATED_HOME) $(VERSION) $(DOCKER_REPOSITORY) $(OUT_DIR) $(DOCKER_BUILD_EXPORT_GO_ENVS) $(DOCKER_BUILD_COPY_GIT_CONFIGS) $(PLUGINS_CONFIG_FILE) $(GO_MOD_FILE)
	./scripts/docker_build.sh development $(GENERATED_HOME) $(VERSION) $(DOCKER_REPOSITORY) false $(DOCKER_BUILD_USE_BUILDKIT)

# provide a goc server for e2e testing
.PHONY: gocdocker
gocdocker: clean
	./scripts/docker_build.sh goc $(GENERATED_HOME) latest goc-server false $(DOCKER_BUILD_USE_BUILDKIT)

.PHONY: vendor
vendor: clean import_plugins
	rm -rf vendor
	$(GO) mod vendor

.PHONY: check-dependency-licenses
check-dependency-licenses: clean import_plugins
	./scripts/dependency_licenses.sh plugin_main LICENSE_OF_ILOGTAIL_DEPENDENCIES.md && ./scripts/dependency_licenses.sh test LICENSE_OF_TESTENGINE_DEPENDENCIES.md

.PHONY: docs
docs: clean build
	./bin/ilogtail --doc

# e2e test
.PHONY: e2e
e2e: clean gocdocker e2edocker
	./scripts/e2e.sh e2e

.PHONY: e2e-core
e2e-core: clean gocdocker e2edocker
	./scripts/e2e.sh e2e core

.PHONY: e2e-performance
e2e-performance: clean docker gocdocker
	./scripts/e2e.sh e2e performance

.PHONY: unittest_e2e_engine
unittest_e2e_engine: clean gocdocker
	cd test && go test  $$(go list ./... | grep -Ev "engine|e2e") -coverprofile=../e2e-engine-coverage.txt -covermode=atomic -tags docker_ready

.PHONY: unittest_plugin
unittest_plugin: clean import_plugins
	cp pkg/logtail/libPluginAdapter.so ./plugin_main
	cp pkg/logtail/PluginAdapter.dll ./plugin_main
	mv ./plugins/input/prometheus/input_prometheus.go ./plugins/input/prometheus/input_prometheus.go.bak
	go test $$(go list ./...|grep -Ev "telegraf|external|envconfig|(input\/prometheus)|(input\/syslog)"| grep -Ev "plugin_main|pluginmanager") -coverprofile .testCoverage.txt
	mv ./plugins/input/prometheus/input_prometheus.go.bak ./plugins/input/prometheus/input_prometheus.go
	rm -rf plugins/input/jmxfetch/test/

.PHONY: unittest_core
unittest_core:
	./scripts/run_core_ut.sh

.PHONY: unittest_pluginmanager
unittest_pluginmanager: clean import_plugins
	cp pkg/logtail/libPluginAdapter.so ./plugin_main
	cp pkg/logtail/PluginAdapter.dll ./plugin_main
	cp pkg/logtail/libPluginAdapter.so ./pluginmanager
	mv ./plugins/input/prometheus/input_prometheus.go ./plugins/input/prometheus/input_prometheus.go.bak
	go test $$(go list ./...|grep -Ev "telegraf|external|envconfig"| grep -E "plugin_main|pluginmanager") -coverprofile .coretestCoverage.txt
	mv ./plugins/input/prometheus/input_prometheus.go.bak ./plugins/input/prometheus/input_prometheus.go

.PHONY: all
all: clean import_plugins
	./scripts/gen_build_scripts.sh all $(GENERATED_HOME) $(VERSION) $(BUILD_REPOSITORY) $(OUT_DIR) $(DOCKER_BUILD_EXPORT_GO_ENVS) $(DOCKER_BUILD_COPY_GIT_CONFIGS) $(PLUGINS_CONFIG_FILE) $(GO_MOD_FILE)
	./scripts/docker_build.sh build $(GENERATED_HOME) $(VERSION) $(BUILD_REPOSITORY) false $(DOCKER_BUILD_USE_BUILDKIT)
	./$(GENERATED_HOME)/gen_copy_docker.sh

.PHONY: dist
dist: all
	./scripts/dist.sh $(OUT_DIR) $(DIST_DIR) $(PACKAGE_DIR)

$(DIST_FILE):
	@echo 'ilogtail-$(VERSION) dist does not exist! Please download or run `make dist` first!'
	@false

.PHONY: docker
docker: $(DIST_FILE)
	./scripts/docker_build.sh production $(GENERATED_HOME) $(VERSION) $(DOCKER_REPOSITORY) $(DOCKER_PUSH) $(DOCKER_BUILD_USE_BUILDKIT)

.PHONY: multi-arch-docker
multi-arch-docker: $(DIST_FILE)
	@echo "will push to $(DOCKER_REPOSITORY):edge. Make sure this tag does not exist or push will fail."
	./scripts/docker_build.sh multi-arch-production $(GENERATED_HOME) $(VERSION) $(DOCKER_REPOSITORY) $(DOCKER_PUSH) $(DOCKER_BUILD_USE_BUILDKIT)
