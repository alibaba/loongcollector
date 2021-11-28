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

VERSION ?= latest
DOCKER_TYPE ?= default
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
OUT_DIR = bin

.PHONY: tools
tools:
	$(GO_LINT) version || curl -sfL https://raw.githubusercontent.com/golangci/golangci-lint/master/install.sh | sh -s -- -b $(GO_PATH)/bin v1.39.0
	$(GO_ADDLICENSE) version || GO111MODULE=off $(GO_GET) -u github.com/google/addlicense


.PHONY: clean
clean:
	rm -rf $(LICENSE_COVERAGE_FILE)
	rm -rf $(OUT_DIR)
	rm -rf behavior-test
	rm -rf performance-test
	rm -rf core-test
	rm -rf e2e-engine-coverage.txt
	rm -rf find_licenses

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

.PHONY: build
build: clean
	./scripts/build.sh vendor default

.PHONY: cgobuild
cgobuild: clean
	./scripts/build.sh vendor c-shared

.PHONY: gocbuild
gocbuild: clean
	./scripts/gocbuild.sh

.PHONY: docker
docker: clean
	./scripts/docker-build.sh $(VERSION) $(DOCKER_TYPE)

# coveragedocker compile with goc to analysis the coverage in e2e testing
coveragedocker: clean
	./scripts/docker-build.sh $(VERSION) coverage

# provide base environment for ilogtail
basedocker: clean
	./scripts/docker-build.sh $(VERSION) base

# provide a goc server for e2e testing
.PHONY: gocdocker
gocdocker: clean
	docker build -t goc-server:latest  --no-cache . -f ./docker/Dockerfile_goc

.PHONY: wholedocker
wholedocker: clean
	./scripts/docker-build.sh $(VERSION) whole

.PHONY: solib
solib: clean
	./scripts/docker-build.sh $(VERSION) lib && ./scripts/solib.sh

.PHONY: vendor
vendor: clean
	rm -rf vendor
	$(GO) mod vendor
	./external/sync_vendor.py

.PHONY: check-dependency-licenses
check-dependency-licenses: clean
	./scripts/dependency_licenses.sh main LICENSE_OF_ILOGTAIL_DEPENDENCIES.md && ./scripts/dependency_licenses.sh test LICENSE_OF_TESTENGINE_DEPENDENCIES.md

.PHONY: e2e-docs
e2e-docs: clean
	cd test && go build -o ilogtail-test-tool  . && ./ilogtail-test-tool docs && rm -f ilogtail-test-tool

# e2e test
.PHONY: e2e
e2e: clean gocdocker coveragedocker
	TEST_DEBUG=$(TEST_DEBUG) TEST_PROFILE=$(TEST_PROFILE)  ./scripts/e2e.sh behavior $(TEST_SCOPE)

.PHONY: e2e-core
e2e-core: clean gocdocker coveragedocker
	TEST_DEBUG=$(TEST_DEBUG) TEST_PROFILE=$(TEST_PROFILE)  ./scripts/e2e.sh core $(TEST_SCOPE)

.PHONY: e2e-performance
e2e-performance: clean docker
	TEST_DEBUG=$(TEST_DEBUG) TEST_PROFILE=$(TEST_PROFILE)  ./scripts/e2e.sh performance $(TEST_SCOPE)

# unit test
.PHONY: test-e2e-engine
test-e2e-engine: clean
	cd test && go test  ./... -coverprofile=../e2e-engine-coverage.txt -covermode=atomic -tags docker_ready

.PHONY: test
test: clean
	cp pkg/logtail/libPluginAdapter.so ./main
	go test $$(go list ./...|grep -v vendor |grep -v telegraf|grep -v external|grep -v envconfig) -coverprofile .testCoverage.txt
