// Copyright 2021 iLogtail Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http:www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package skywalkingv2

import (
	"testing"

	"github.com/alibaba/ilogtail/plugins/input/skywalkingv2/skywalking/apm/network/language/agent"
)

func TestRegistryApplication(test *testing.T) {
	cache := NewRegistryInformationCache()

	expectedID := cache.registryApplication("test")
	realID := cache.registryApplication("test")

	if realID != expectedID {
		test.Fatalf("expected: %d, real: %d", expectedID, realID)
	}
}

func TestExistApplication(test *testing.T) {
	cache := NewRegistryInformationCache()

	expectedID := cache.registryApplication("test")
	realID, ok := cache.applicationExist("test")

	if !ok {
		test.Fatalf("not found application")
	}

	if realID != expectedID {
		test.Fatalf("expected: %d, real: %d", expectedID, realID)
	}
}

func TestNotExistApplication(test *testing.T) {
	cache := NewRegistryInformationCache()

	_, ok := cache.applicationExist("test")

	if ok {
		test.Fatalf("found not exist application")
	}

}

func TestRegistryApplicationInstanceWithoutApplication(test *testing.T) {
	cache := NewRegistryInformationCache()

	id := cache.registryApplicationInstance(1, "test", nil)

	if id != 0 {
		test.Fatalf("TestRegistryApplicationInstanceWithoutApplication Failed")
	}
}

func TestNotExistApplicationInstance(test *testing.T) {
	cache := NewRegistryInformationCache()
	_, ok := cache.instanceExist(-1, "test")
	if ok {
		test.Fatalf("found not exist application instance")
	}
}

func TestExistApplicationInstance(test *testing.T) {
	cache := NewRegistryInformationCache()

	applicationID := cache.registryApplication("test")
	applicationInstanceID := cache.registryApplicationInstance(applicationID, "test", &agent.OSInfo{})

	instance, ok := cache.instanceExist(applicationID, "test")
	if !ok {
		test.Fatalf("not found  exist application instance")
	}

	if instance != applicationInstanceID {
		test.Fatalf("application isntance id not equals")
	}
}

func TestRegistryApplicationInstanceWithApplication(test *testing.T) {
	cache := NewRegistryInformationCache()

	applicationID := cache.registryApplication("test")

	id := cache.registryApplicationInstance(applicationID, "test", &agent.OSInfo{})

	if id == 0 {
		test.Fatalf("TestRegistryApplicationInstanceWithApplication Failed")
	}
}

func TestFindNotExistApplication(test *testing.T) {
	cache := NewRegistryInformationCache()

	_, ok := cache.findApplicationRegistryInfo("test")
	if ok {
		test.Fatalf("TestFindNotExistApplication Failed")
	}
}

func TestFindExistApplication(test *testing.T) {
	cache := NewRegistryInformationCache()
	expectedID := cache.registryApplication("test")

	application, ok := cache.findApplicationRegistryInfo("test")
	if !ok {
		test.Fatalf("TestFindNotExistApplication Failed")
	}

	if expectedID != application.id {
		test.Fatalf("application ID not equals")
	}

	if application.applicationName != "test" {
		test.Fatalf("application name not equals")
	}
}

func TestFindNotExistApplicationInstance(test *testing.T) {
	cache := NewRegistryInformationCache()
	_, ok := cache.findApplicationInstanceRegistryInfo(-1)
	if ok {
		test.Fatalf("TestFindNotExistApplicationInstance Failed")
	}
}

func TestFindExistApplicationInstance(test *testing.T) {
	cache := NewRegistryInformationCache()
	applicationID := cache.registryApplication("test")
	applicationInstanceID := cache.registryApplicationInstance(applicationID, "test", &agent.OSInfo{})
	applicationInstance, ok := cache.findApplicationInstanceRegistryInfo(applicationInstanceID)

	if !ok {
		test.Fatalf("TestFindNotExistApplicationInstance Failed")
	}

	if applicationInstanceID != applicationInstance.id {
		test.Fatalf("application ID not equals")
	}

	if applicationInstance.uuid != "test" {
		test.Fatalf("application name not equals")
	}

	if applicationInstance.lastHeartBeatTime == 0 {
		test.Fatalf("not update last heart beat time")
	}
}

func TestRegistryEndpointWithNotExistApplication(test *testing.T) {
	cache := NewRegistryInformationCache()
	endpoint := cache.registryEndpoint(0, "test")

	if endpoint != nil {
		test.Fatalf("found not exist application")
	}
}

func TestRegistryEndpointWithExistApplication(test *testing.T) {
	cache := NewRegistryInformationCache()
	applicationID := cache.registryApplication("test")
	endpoint := cache.registryEndpoint(applicationID, "test")

	if endpoint == nil {
		test.Fatalf("not found exist application")
	}

	if endpoint.endpointName != "test" {
		test.Fatalf("endpoint name is not exist")
	}

	if endpoint.id == 0 {
		test.Fatalf("endpoint id cannot be zero")
	}
}

func TestRegistryEndpointWithSameEndpoint(test *testing.T) {
	cache := NewRegistryInformationCache()
	applicationID := cache.registryApplication("test")
	endpoint := cache.registryEndpoint(applicationID, "test")
	endpoint1 := cache.registryEndpoint(applicationID, "test")

	if endpoint == nil {
		test.Fatalf("not found exist application")
	}

	if endpoint.applicationID != endpoint1.applicationID {
		test.Fatalf("application id not equals")
	}

	if endpoint.endpointName != endpoint1.endpointName {
		test.Fatalf("endpoint name not equals")
	}

	if endpoint.id != endpoint1.id {
		test.Fatalf("endpoint id not equals")
	}
}
