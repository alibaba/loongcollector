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

	"google.golang.org/grpc"
	criv1 "k8s.io/cri-api/pkg/apis/runtime/v1"
)

type RuntimeServiceV1Adapter struct {
	client criv1.RuntimeServiceClient
}

func newCRIRuntimeServiceV1Adapter(conn *grpc.ClientConn) *RuntimeServiceV1Adapter {
	return &RuntimeServiceV1Adapter{client: criv1.NewRuntimeServiceClient(conn)}
}

func (a *RuntimeServiceV1Adapter) Version(ctx context.Context) (*CriVersionResponse, error) {
	resp, err := a.client.Version(ctx, &criv1.VersionRequest{})
	if err != nil {
		return &CriVersionResponse{}, err
	}
	return &CriVersionResponse{
		resp.Version,
		resp.RuntimeName,
		resp.RuntimeVersion,
		resp.RuntimeApiVersion,
	}, nil
}

func (a *RuntimeServiceV1Adapter) ListContainers(ctx context.Context) (*CriListContainersResponse, error) {
	resp, err := a.client.ListContainers(ctx, &criv1.ListContainersRequest{})
	if err != nil {
		return &CriListContainersResponse{}, err
	}

	containers := make([]*CriContainer, 0, len(resp.Containers))
	for _, rawContainer := range resp.Containers {
		container := &CriContainer{
			ID:           rawContainer.Id,
			PodSandboxID: rawContainer.PodSandboxId,
			Metadata:     &CriContainerMetadata{},
			Image:        &CriImageSpec{},
			ImageRef:     rawContainer.ImageRef,
			State:        CriContainerState(rawContainer.State),
			CreatedAt:    rawContainer.CreatedAt,
			Labels:       rawContainer.Labels,
			Annotations:  rawContainer.Annotations,
		}
		if rawContainer.Image != nil {
			container.Image.Image = rawContainer.Image.Image
			container.Image.Annotations = rawContainer.Image.Annotations
		}
		if rawContainer.Metadata != nil {
			container.Metadata.Name = rawContainer.Metadata.Name
			container.Metadata.Attempt = rawContainer.Metadata.Attempt
		}
		containers = append(containers, container)
	}

	return &CriListContainersResponse{
		Containers: containers,
	}, nil
}

func (a *RuntimeServiceV1Adapter) ContainerStatus(ctx context.Context, containerID string, verbose bool) (*CriContainerStatusResponse, error) {
	rawStatus, err := a.client.ContainerStatus(ctx, &criv1.ContainerStatusRequest{
		ContainerId: containerID,
		Verbose:     verbose,
	})
	if err != nil {
		return &CriContainerStatusResponse{}, err
	}

	status := &CriContainerStatus{}
	if rawStatus.Status != nil {
		status.ID = rawStatus.Status.Id
		status.Metadata = &CriContainerMetadata{}
		if rawStatus.Status.Metadata != nil {
			status.Metadata.Name = rawStatus.Status.Metadata.Name
			status.Metadata.Attempt = rawStatus.Status.Metadata.Attempt
		}
		status.State = CriContainerState(rawStatus.Status.State)
		status.CreatedAt = rawStatus.Status.CreatedAt
		status.StartedAt = rawStatus.Status.StartedAt
		status.FinishedAt = rawStatus.Status.FinishedAt
		status.ExitCode = rawStatus.Status.ExitCode
		status.Image = &CriImageSpec{}
		if rawStatus.Status.Image != nil {
			status.Image.Image = rawStatus.Status.Image.Image
			status.Image.Annotations = rawStatus.Status.Image.Annotations
		}
		status.ImageRef = rawStatus.Status.ImageRef
		status.Reason = rawStatus.Status.Reason
		status.Message = rawStatus.Status.Message
		status.Labels = rawStatus.Status.Labels
		status.Annotations = rawStatus.Status.Annotations
		status.Mounts = []*CriMount{}
		if rawStatus.Status.Mounts != nil {
			for _, rawMount := range rawStatus.Status.Mounts {
				status.Mounts = append(status.Mounts, &CriMount{
					ContainerPath:  rawMount.ContainerPath,
					HostPath:       rawMount.HostPath,
					Readonly:       rawMount.Readonly,
					SelinuxRelabel: rawMount.SelinuxRelabel,
					Propagation:    CriMountPropagation(rawMount.Propagation),
				})
			}
		}
		status.LogPath = rawStatus.Status.LogPath
	}

	return &CriContainerStatusResponse{
		Status: status,
		Info:   rawStatus.Info,
	}, nil
}

func (a *RuntimeServiceV1Adapter) ListPodSandbox(ctx context.Context) (*CriListPodSandboxResponse, error) {
	resp, err := a.client.ListPodSandbox(ctx, &criv1.ListPodSandboxRequest{})
	if err != nil {
		return &CriListPodSandboxResponse{}, err
	}

	sandboxs := make([]*CriPodSandbox, 0, len(resp.Items))
	if resp.Items != nil {
		for _, rawSandbox := range resp.Items {
			sandbox := &CriPodSandbox{
				ID:             rawSandbox.Id,
				Metadata:       &CriPodSandboxMetadata{},
				State:          CriContainerState(rawSandbox.State),
				CreatedAt:      rawSandbox.CreatedAt,
				Labels:         rawSandbox.Labels,
				Annotations:    rawSandbox.Annotations,
				RuntimeHandler: rawSandbox.RuntimeHandler,
			}
			if rawSandbox.Metadata != nil {
				sandbox.Metadata.Name = rawSandbox.Metadata.Name
				sandbox.Metadata.Attempt = rawSandbox.Metadata.Attempt
			}
			sandboxs = append(sandboxs, sandbox)
		}
	}

	return &CriListPodSandboxResponse{
		Items: sandboxs,
	}, nil
}

func (a *RuntimeServiceV1Adapter) PodSandboxStatus(ctx context.Context, sandboxID string, verbose bool) (*CriPodSandboxStatusResponse, error) {
	rawStatus, err := a.client.PodSandboxStatus(ctx, &criv1.PodSandboxStatusRequest{
		PodSandboxId: sandboxID,
		Verbose:      verbose,
	})
	if err != nil {
		return &CriPodSandboxStatusResponse{}, err
	}

	status := &CriPodSandboxStatus{}
	if rawStatus.Status != nil {
		status.ID = rawStatus.Status.Id
		status.Metadata = &CriPodSandboxMetadata{}
		if rawStatus.Status.Metadata != nil {
			status.Metadata.Name = rawStatus.Status.Metadata.Name
			status.Metadata.UID = rawStatus.Status.Metadata.Uid
			status.Metadata.Namespace = rawStatus.Status.Metadata.Namespace
			status.Metadata.Attempt = rawStatus.Status.Metadata.Attempt
		}
		status.State = CriContainerState(rawStatus.Status.State)
		status.CreatedAt = rawStatus.Status.CreatedAt
		status.Labels = rawStatus.Status.Labels
		status.Annotations = rawStatus.Status.Annotations
		status.RuntimeHandler = rawStatus.Status.RuntimeHandler
	}

	return &CriPodSandboxStatusResponse{
		Status: status,
		Info:   rawStatus.Info,
	}, nil
}
