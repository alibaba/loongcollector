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
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"

	"google.golang.org/grpc"
)

type CriContainerState int32

const (
	ContainerStateContainerCreated CriContainerState = 0
	ContainerStateContainerRunning CriContainerState = 1
	ContainerStateContainerExited  CriContainerState = 2
	ContainerStateContainerUnknown CriContainerState = 3
)

type CriMountPropagation int32

const (
	MountPropagationPropagationPrivate         CriMountPropagation = 0
	MountPropagationPropagationHostToContainer CriMountPropagation = 1
	MountPropagationPropagationBidirectional   CriMountPropagation = 2
)

type CriVersionInfo struct {
	Version           string
	RuntimeName       string
	RuntimeVersion    string
	RuntimeAPIVersion string
}

type CriVersionResponse struct {
	Version           string `protobuf:"bytes,1,opt,name=version,proto3" json:"version,omitempty"`
	RuntimeName       string `protobuf:"bytes,2,opt,name=runtime_name,proto3" json:"runtime_name,omitempty"`
	RuntimeVersion    string `protobuf:"bytes,3,opt,name=runtime_version,proto3" json:"runtime_version,omitempty"`
	RuntimeAPIVersion string `protobuf:"bytes,4,opt,name=runtime_api_version,proto3" json:"runtime_api_version,omitempty"`
}

type CriContainerMetadata struct {
	Name    string
	Attempt uint32
}

type CriImageSpec struct {
	Image       string
	Annotations map[string]string
}

type CriContainer struct {
	ID           string
	PodSandboxID string
	Metadata     *CriContainerMetadata
	Image        *CriImageSpec
	ImageRef     string
	State        CriContainerState
	CreatedAt    int64
	Labels       map[string]string
	Annotations  map[string]string
}

type CriListContainersResponse struct {
	Containers []*CriContainer
}

type CriMount struct {
	ContainerPath  string
	HostPath       string
	Readonly       bool
	SelinuxRelabel bool
	Propagation    CriMountPropagation
}

type CriContainerStatus struct {
	ID          string
	Metadata    *CriContainerMetadata
	State       CriContainerState
	CreatedAt   int64
	StartedAt   int64
	FinishedAt  int64
	ExitCode    int32
	Image       *CriImageSpec
	ImageRef    string
	Reason      string
	Message     string
	Labels      map[string]string
	Annotations map[string]string
	Mounts      []*CriMount
	LogPath     string
}

type CriContainerStatusResponse struct {
	Status *CriContainerStatus
	Info   map[string]string
}

type CriPodSandboxMetadata struct {
	Name      string
	UID       string
	Namespace string
	Attempt   uint32
}

type CriPodSandbox struct {
	ID             string
	Metadata       *CriPodSandboxMetadata
	State          CriContainerState
	CreatedAt      int64
	Labels         map[string]string
	Annotations    map[string]string
	RuntimeHandler string
}

type CriListPodSandboxResponse struct {
	Items []*CriPodSandbox
}

type CriPodSandboxStatus struct {
	ID             string
	Metadata       *CriPodSandboxMetadata
	State          CriContainerState
	CreatedAt      int64
	Labels         map[string]string
	Annotations    map[string]string
	RuntimeHandler string
}

type CriPodSandboxStatusResponse struct {
	Status *CriPodSandboxStatus
	Info   map[string]string
}

type RuntimeService interface {
	Version(ctx context.Context) (*CriVersionResponse, error)
	ListContainers(ctx context.Context) (*CriListContainersResponse, error)
	ContainerStatus(ctx context.Context, containerID string, verbose bool) (*CriContainerStatusResponse, error)
	ListPodSandbox(ctx context.Context) (*CriListPodSandboxResponse, error)
	PodSandboxStatus(ctx context.Context, sandboxID string, verbose bool) (*CriPodSandboxStatusResponse, error)
}

type RuntimeServiceClient struct {
	service RuntimeService
	info    CriVersionInfo
	conn    *grpc.ClientConn
}

var ( // for mock
	getAddressAndDialer = GetAddressAndDialer
	grpcDialContext     = grpc.DialContext
)

func NewRuntimeServiceClient(contextTimeout time.Duration, grpcMaxCallRecvMsgSize int) (*RuntimeServiceClient, error) {
	addr, dailer, err := getAddressAndDialer(containerdUnixSocket)
	if err != nil {
		return nil, err
	}
	ctx, cancel := getContextWithTimeout(contextTimeout)
	defer cancel()

	conn, err := grpcDialContext(ctx, addr, grpc.WithInsecure(), grpc.WithDialer(dailer), grpc.WithDefaultCallOptions(grpc.MaxCallRecvMsgSize(grpcMaxCallRecvMsgSize)))
	if err != nil {
		return nil, err
	}

	client := &RuntimeServiceClient{
		conn: conn,
	}
	// Try v1 first
	client.service = newCRIRuntimeServiceV1Adapter(conn)
	if client.getVersion(ctx) == nil {
		logger.Info(ctx, "Init cri client v1 success, cri info", client.info)
		return client, nil
	}

	// Fallback to v1alpha2
	client.service = newCRIRuntimeServiceV1Alpha2Adapter(conn)
	if client.getVersion(ctx) == nil {
		logger.Info(ctx, "Init cri client v1alpha2 success, cri info", client.info)
		return client, nil
	}

	// if create client failed, close the connection
	_ = conn.Close()
	return nil, fmt.Errorf("failed to initialize RuntimeServiceClient")
}

func (c *RuntimeServiceClient) Version(ctx context.Context) (*CriVersionResponse, error) {
	if c.service != nil {
		return c.service.Version(ctx)
	}
	return &CriVersionResponse{}, fmt.Errorf("invalid RuntimeServiceClient")
}

func (c *RuntimeServiceClient) ListContainers(ctx context.Context) (*CriListContainersResponse, error) {
	if c.service != nil {
		return c.service.ListContainers(ctx)
	}
	return &CriListContainersResponse{}, fmt.Errorf("invalid RuntimeServiceClient")
}

func (c *RuntimeServiceClient) ContainerStatus(ctx context.Context, containerID string, verbose bool) (*CriContainerStatusResponse, error) {
	if c.service != nil {
		return c.service.ContainerStatus(ctx, containerID, verbose)
	}
	return &CriContainerStatusResponse{}, fmt.Errorf("invalid RuntimeServiceClient")
}

func (c *RuntimeServiceClient) ListPodSandbox(ctx context.Context) (*CriListPodSandboxResponse, error) {
	if c.service != nil {
		return c.service.ListPodSandbox(ctx)
	}
	return &CriListPodSandboxResponse{}, fmt.Errorf("invalid RuntimeServiceClient")
}

func (c *RuntimeServiceClient) PodSandboxStatus(ctx context.Context, sandboxID string, verbose bool) (*CriPodSandboxStatusResponse, error) {
	if c.service != nil {
		return c.service.PodSandboxStatus(ctx, sandboxID, verbose)
	}
	return &CriPodSandboxStatusResponse{}, fmt.Errorf("invalid RuntimeServiceClient")
}

func (c *RuntimeServiceClient) getVersion(ctx context.Context) error {
	versionResp, err := c.service.Version(ctx)
	if err == nil {
		c.info = CriVersionInfo{
			versionResp.Version,
			versionResp.RuntimeName,
			versionResp.RuntimeVersion,
			versionResp.RuntimeAPIVersion,
		}
	}
	return err
}

func (c *RuntimeServiceClient) Close() {
	if c.conn != nil {
		_ = c.conn.Close()
	}
}
