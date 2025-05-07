// Copyright 2025 iLogtail Authors
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

package containercenter

import (
	"context"
	"fmt"

	"google.golang.org/grpc"
)

type criContainerState int32

const (
	ContainerState_CONTAINER_CREATED criContainerState = 0
	ContainerState_CONTAINER_RUNNING criContainerState = 1
	ContainerState_CONTAINER_EXITED  criContainerState = 2
	ContainerState_CONTAINER_UNKNOWN criContainerState = 3
)

type criMountPropagation int32

const (
	MountPropagation_PROPAGATION_PRIVATE           criMountPropagation = 0
	MountPropagation_PROPAGATION_HOST_TO_CONTAINER criMountPropagation = 1
	MountPropagation_PROPAGATION_BIDIRECTIONAL     criMountPropagation = 2
)

type criVersionResponse struct {
	Version           string
	RuntimeName       string
	RuntimeVersion    string
	RuntimeAPIVersion string
}

type criContainerMetadata struct {
	Name    string
	Attempt uint32
}

type criImageSpec struct {
	Image       string
	Annotations map[string]string
}

type criContainer struct {
	Id           string
	PodSandboxId string
	Metadata     *criContainerMetadata
	Image        *criImageSpec
	ImageRef     string
	State        criContainerState
	CreatedAt    int64
	Labels       map[string]string
	Annotations  map[string]string
}

type criListContainersResponse struct {
	Containers []*criContainer
}

type criMount struct {
	ContainerPath  string
	HostPath       string
	Readonly       bool
	SelinuxRelabel bool
	Propagation    criMountPropagation
}

type criContainerStatus struct {
	Id          string
	Metadata    *criContainerMetadata
	State       criContainerState
	CreatedAt   int64
	StartedAt   int64
	FinishedAt  int64
	ExitCode    int32
	Image       *criImageSpec
	ImageRef    string
	Reason      string
	Message     string
	Labels      map[string]string
	Annotations map[string]string
	Mounts      []*criMount
	LogPath     string
}

type criContainerStatusResponse struct {
	Status *criContainerStatus
	Info   map[string]string
}

type criPodSandboxMetadata struct {
	Name      string
	Uid       string
	Namespace string
	Attempt   uint32
}

type criPodSandbox struct {
	Id             string
	Metadata       *criPodSandboxMetadata
	State          criContainerState
	CreatedAt      int64
	Labels         map[string]string
	Annotations    map[string]string
	RuntimeHandler string
}

type criListPodSandboxResponse struct {
	Items []*criPodSandbox
}

type criPodSandboxStatus struct {
	Id             string
	Metadata       *criPodSandboxMetadata
	State          criContainerState
	CreatedAt      int64
	Labels         map[string]string
	Annotations    map[string]string
	RuntimeHandler string
}

type criPodSandboxStatusResponse struct {
	Status *criPodSandboxStatus
	Info   map[string]string
}

type RuntimeService interface {
	Version(ctx context.Context) (criVersionResponse, error)
	ListContainers(ctx context.Context) (criListContainersResponse, error)
	ContainerStatus(ctx context.Context, containerID string, verbose bool) (criContainerStatusResponse, error)
	ListPodSandbox(ctx context.Context) (criListPodSandboxResponse, error)
	PodSandboxStatus(ctx context.Context, sandboxID string, verbose bool) (criPodSandboxStatusResponse, error)
}

type RuntimeServiceClient struct {
	service RuntimeService
	info    RuntimeInfo
}

func NewRuntimeServiceClient(ctx context.Context, conn *grpc.ClientConn) (*RuntimeServiceClient, error) {
	client := &RuntimeServiceClient{}
	// Try v1alpha2 first
	client.service = newCRIRuntimeServiceV1Alpha2Adapter(conn)
	if client.checkVersion(ctx) == nil {
		return client, nil
	}

	// Fallback to v1
	client.service = newCRIRuntimeServiceV1Adapter(conn)
	if client.checkVersion(ctx) == nil {
		return client, nil
	}

	return nil, fmt.Errorf("failed to initialize RuntimeServiceClient")
}

func (c *RuntimeServiceClient) Version(ctx context.Context) (criVersionResponse, error) {
	if c.service != nil {
		return c.service.Version(ctx)
	}
	return criVersionResponse{}, fmt.Errorf("invalid RuntimeServiceClient")
}

func (c *RuntimeServiceClient) ListContainers(ctx context.Context) (criListContainersResponse, error) {
	if c.service != nil {
		return c.service.ListContainers(ctx)
	}
	return criListContainersResponse{}, fmt.Errorf("invalid RuntimeServiceClient")
}

func (c *RuntimeServiceClient) ContainerStatus(ctx context.Context, containerID string, verbose bool) (criContainerStatusResponse, error) {
	if c.service != nil {
		return c.service.ContainerStatus(ctx, containerID, verbose)
	}
	return criContainerStatusResponse{}, fmt.Errorf("invalid RuntimeServiceClient")
}

func (c *RuntimeServiceClient) ListPodSandbox(ctx context.Context) (criListPodSandboxResponse, error) {
	if c.service != nil {
		return c.service.ListPodSandbox(ctx)
	}
	return criListPodSandboxResponse{}, fmt.Errorf("invalid RuntimeServiceClient")
}

func (c *RuntimeServiceClient) PodSandboxStatus(ctx context.Context, sandboxID string, verbose bool) (criPodSandboxStatusResponse, error) {
	if c.service != nil {
		return c.service.PodSandboxStatus(ctx, sandboxID, verbose)
	}
	return criPodSandboxStatusResponse{}, fmt.Errorf("invalid RuntimeServiceClient")
}

func (c *RuntimeServiceClient) checkVersion(ctx context.Context) error {
	versionResp, err := c.service.Version(ctx)
	if err == nil {
		c.info = RuntimeInfo{
			version:           versionResp.Version,
			runtimeName:       versionResp.RuntimeName,
			runtimeVersion:    versionResp.RuntimeVersion,
			runtimeAPIVersion: versionResp.RuntimeAPIVersion,
		}
	}
	return err
}
