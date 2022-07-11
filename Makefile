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
VERSION ?= 1.1.0
DOCKER_PUSH ?= false
DOCKER_REPOSITORY ?= aliyun/ilogtail
BUILD_REPOSITORY ?= aliyun/ilogtail_build
GENERATED_HOME ?= generated_files

SCOPE ?= .
BINARY = logtail-plugin

TEST_DEBUG ?= false
TEST_PROFILE ?= false
TEST_SCOPE ?= "all"

PLATFORMS := linux darwin windows

OS = $(shell uname)
ARCH = amd64

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
DIST_DIR = ilogtail-$(VERSION)

.PHONY: tools
tools:
	$(GO_LINT) version || curl -sfL https://raw.githubusercontent.com/golangci/golangci-lint/master/install.sh | sh -s -- -b $(GO_PATH)/bin v1.39.0
	$(GO_ADDLICENSE) version || GO111MODULE=off $(GO_GET) -u github.com/google/addlicense

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

.PHONY: license
license:  clean tools
	./scripts/package_license.sh add-license $(SCOPE)

.PHONY: check-license
check-license: clean tools
	./scripts/package_license.sh check $(SCOPE) > $(LICENSE_COVERAGE_FILE)

.PHONY: lint
lint: clean tools
	$(GO_LINT) run -v --timeout 5m $(SCOPE)/... && make lint-pkg && make lint-e2e

.PHONY: lint-pkg
lint-pkg: clean tools
	cd pkg && pwd && $(GO_LINT) run -v --timeout 5m ./...

.PHONY: lint-test
lint-e2e: clean tools
	cd test && pwd && $(GO_LINT) run -v --timeout 5m ./...

.PHONY: core
core: clean
	./scripts/gen_build_scripts.sh core $(GENERATED_HOME) $(VERSION) $(BUILD_REPOSITORY) $(OUT_DIR)
	./scripts/docker_build.sh build $(GENERATED_HOME) $(VERSION) $(BUILD_REPOSITORY) false
	./$(GENERATED_HOME)/gen_copy_docker.sh

.PHONY: plugin
plugin: clean
	./scripts/gen_build_scripts.sh plugin $(GENERATED_HOME) $(VERSION) $(BUILD_REPOSITORY) $(OUT_DIR)
	./scripts/docker_build.sh build $(GENERATED_HOME) $(VERSION) $(BUILD_REPOSITORY) false
	./$(GENERATED_HOME)/gen_copy_docker.sh

.PHONY: plugin_main
plugin_main: clean
	./scripts/plugin_build.sh vendor default $(OUT_DIR)
	cp pkg/logtail/libPluginAdapter.so bin/libPluginAdapter.so
	cp pkg/logtail/PluginAdapter.dll bin/PluginAdapter.dll

.PHONY: plugin_local
plugin_main:
	./scripts/plugin_build.sh vendor c-shared $(OUT_DIR)

.PHONY: docker
docker: clean
	./scripts/gen_build_scripts.sh all $(GENERATED_HOME) $(VERSION) $(DOCKER_REPOSITORY) $(OUT_DIR)
	./scripts/docker_build.sh production $(GENERATED_HOME) $(VERSION) $(DOCKER_REPOSITORY) $(DOCKER_PUSH)

.PHONY: e2edocker
e2edocker: clean
	./scripts/gen_build_scripts.sh e2e $(GENERATED_HOME) $(VERSION) $(DOCKER_REPOSITORY) $(OUT_DIR)
	./scripts/docker_build.sh development $(GENERATED_HOME) $(VERSION) $(DOCKER_REPOSITORY) false

# provide a goc server for e2e testing
.PHONY: gocdocker
gocdocker: clean
	./scripts/docker_build.sh goc $(GENERATED_HOME) latest goc-server false

.PHONY: vendor
vendor: clean
	rm -rf vendor
	$(GO) mod vendor
	./external/sync_vendor.py

.PHONY: check-dependency-licenses
check-dependency-licenses: clean
	./scripts/dependency_licenses.sh plugin_main LICENSE_OF_ILOGTAIL_DEPENDENCIES.md && ./scripts/dependency_licenses.sh test LICENSE_OF_TESTENGINE_DEPENDENCIES.md

.PHONY: docs
docs: clean build
	./bin/ilogtail --doc

.PHONY: e2e-docs
e2e-docs: clean
	cd test && go build -o ilogtail-test-tool  . && ./ilogtail-test-tool docs && rm -f ilogtail-test-tool

# e2e test
.PHONY: e2e
e2e: clean gocdocker e2edocker
	TEST_DEBUG=$(TEST_DEBUG) TEST_PROFILE=$(TEST_PROFILE)  ./scripts/e2e.sh behavior $(TEST_SCOPE)

.PHONY: e2e-core
e2e-core: clean gocdocker e2edocker
	TEST_DEBUG=$(TEST_DEBUG) TEST_PROFILE=$(TEST_PROFILE)  ./scripts/e2e.sh core $(TEST_SCOPE)

.PHONY: e2e-performance
e2e-performance: clean docker gocdocker
	TEST_DEBUG=$(TEST_DEBUG) TEST_PROFILE=$(TEST_PROFILE)  ./scripts/e2e.sh performance $(TEST_SCOPE)

.PHONY: unittest_e2e_engine
unittest_e2e_engine: clean gocdocker
	cd test && go test  ./... -coverprofile=../e2e-engine-coverage.txt -covermode=atomic -tags docker_ready

.PHONY: unittest_plugin
unittest_plugin: clean
	cp pkg/logtail/libPluginAdapter.so ./plugin_main
	cp pkg/logtail/PluginAdapter.dll ./plugin_main
	mv ./plugins/input/prometheus/input_prometheus.go ./plugins/input/prometheus/input_prometheus.go.bak
	go test $$(go list ./...|grep -Ev "vendor|telegraf|external|envconfig|(input\/prometheus)|(input\/syslog)"| grep -Ev "plugin_main|pluginmanager") -coverprofile .testCoverage.txt
	mv ./plugins/input/prometheus/input_prometheus.go.bak ./plugins/input/prometheus/input_prometheus.go

.PHONY: unittest_pluginmanager
unittest_pluginmanager: clean
	cp pkg/logtail/libPluginAdapter.so ./plugin_main
	cp pkg/logtail/PluginAdapter.dll ./plugin_main
	mv ./plugins/input/prometheus/input_prometheus.go ./plugins/input/prometheus/input_prometheus.go.bak
	go test $$(go list ./...|grep -Ev "vendor|telegraf|external|envconfig|()"| grep -E "plugin_main|pluginmanager") -coverprofile .coretestCoverage.txt
	mv ./plugins/input/prometheus/input_prometheus.go.bak ./plugins/input/prometheus/input_prometheus.go

.PHONY: all
all: clean
	./scripts/gen_build_scripts.sh all $(GENERATED_HOME) $(VERSION) $(BUILD_REPOSITORY) $(OUT_DIR)
	./scripts/docker_build.sh build $(GENERATED_HOME) $(VERSION) $(BUILD_REPOSITORY) false
	./$(GENERATED_HOME)/gen_copy_docker.sh

.PHONY: dist
dist: all
	./scripts/dist.sh $(OUT_DIR) $(DIST_DIR)
