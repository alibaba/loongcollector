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
	criv1alpha2 "k8s.io/cri-api/pkg/apis/runtime/v1alpha2"
)

type RuntimeServiceV1Alpha2Adapter struct {
	client criv1alpha2.RuntimeServiceClient
}

func newCRIRuntimeServiceV1Alpha2Adapter(conn *grpc.ClientConn) *RuntimeServiceV1Alpha2Adapter {
	return &RuntimeServiceV1Alpha2Adapter{client: criv1alpha2.NewRuntimeServiceClient(conn)}
}

func (a *RuntimeServiceV1Alpha2Adapter) Version(ctx context.Context) (criVersionResponse, error) {
	resp, err := a.client.Version(ctx, &criv1alpha2.VersionRequest{})
	if err != nil {
		return criVersionResponse{}, err
	}
	return criVersionResponse{
		resp.Version,
		resp.RuntimeName,
		resp.RuntimeVersion,
		resp.RuntimeApiVersion,
	}, nil
}

func (a *RuntimeServiceV1Alpha2Adapter) ListContainers(ctx context.Context) (criListContainersResponse, error) {
	resp, err := a.client.ListContainers(ctx, &criv1alpha2.ListContainersRequest{})
	if err != nil {
		return criListContainersResponse{}, err
	}

	containers := make([]*criContainer, 0, len(resp.Containers))
	if resp.Containers != nil {
		for _, rawContainer := range resp.Containers {
			container := &criContainer{
				Id:        rawContainer.Id,
				State:     criContainerState(rawContainer.State),
				CreatedAt: rawContainer.CreatedAt,
				ImageRef:  rawContainer.ImageRef,
				Labels:    rawContainer.Labels,
			}
			if rawContainer.Image != nil {
				container.Image = &criImageSpec{
					Image:       rawContainer.Image.Image,
					Annotations: rawContainer.Image.Annotations,
				}
			}
			if rawContainer.Metadata != nil {
				container.Metadata = &criContainerMetadata{
					Name:    rawContainer.Metadata.Name,
					Attempt: rawContainer.Metadata.Attempt,
				}
			}
			containers = append(containers, container)
		}
	}

	return criListContainersResponse{
		Containers: containers,
	}, nil
}

func (a *RuntimeServiceV1Alpha2Adapter) ContainerStatus(ctx context.Context, containerID string, verbose bool) (criContainerStatusResponse, error) {
	rawStatus, err := a.client.ContainerStatus(ctx, &criv1alpha2.ContainerStatusRequest{
		ContainerId: containerID,
		Verbose:     verbose,
	})
	if err != nil {
		return criContainerStatusResponse{}, err
	}

	status := &criContainerStatus{
		Id:          rawStatus.Status.Id,
		State:       criContainerState(rawStatus.Status.State),
		CreatedAt:   rawStatus.Status.CreatedAt,
		StartedAt:   rawStatus.Status.StartedAt,
		FinishedAt:  rawStatus.Status.FinishedAt,
		ExitCode:    rawStatus.Status.ExitCode,
		ImageRef:    rawStatus.Status.ImageRef,
		Reason:      rawStatus.Status.Reason,
		Message:     rawStatus.Status.Message,
		Labels:      rawStatus.Status.Labels,
		Annotations: rawStatus.Status.Annotations,
		LogPath:     rawStatus.Status.LogPath,
	}
	if rawStatus.Status.Metadata != nil {
		status.Metadata = &criContainerMetadata{
			Name:    rawStatus.Status.Metadata.Name,
			Attempt: rawStatus.Status.Metadata.Attempt,
		}
	}
	if rawStatus.Status.Image != nil {
		status.Image = &criImageSpec{
			Image:       rawStatus.Status.Image.Image,
			Annotations: rawStatus.Status.Image.Annotations,
		}
	}
	mounts := make([]*criMount, 0, len(rawStatus.Status.Mounts))
	if rawStatus.Status.Mounts != nil {
		for _, rawMount := range rawStatus.Status.Mounts {
			mounts = append(mounts, &criMount{
				ContainerPath:  rawMount.ContainerPath,
				HostPath:       rawMount.HostPath,
				Readonly:       rawMount.Readonly,
				SelinuxRelabel: rawMount.SelinuxRelabel,
				Propagation:    criMountPropagation(rawMount.Propagation),
			})
		}
	}
	status.Mounts = mounts

	return criContainerStatusResponse{
		Status: status,
		Info:   rawStatus.Info,
	}, nil
}

func (a *RuntimeServiceV1Alpha2Adapter) ListPodSandbox(ctx context.Context) (criListPodSandboxResponse, error) {
	resp, err := a.client.ListPodSandbox(ctx, &criv1alpha2.ListPodSandboxRequest{})
	if err != nil {
		return criListPodSandboxResponse{}, err
	}

	sandboxs := make([]*criPodSandbox, 0, len(resp.Items))
	if resp.Items != nil {
		for _, rawSandbox := range resp.Items {
			sandbox := &criPodSandbox{
				Id:             rawSandbox.Id,
				State:          criContainerState(rawSandbox.State),
				CreatedAt:      rawSandbox.CreatedAt,
				Labels:         rawSandbox.Labels,
				Annotations:    rawSandbox.Annotations,
				RuntimeHandler: rawSandbox.RuntimeHandler,
			}
			if rawSandbox.Metadata != nil {
				sandbox.Metadata = &criPodSandboxMetadata{
					Name:    rawSandbox.Metadata.Name,
					Attempt: rawSandbox.Metadata.Attempt,
				}
			}
			sandboxs = append(sandboxs, sandbox)
		}
	}

	return criListPodSandboxResponse{
		Items: sandboxs,
	}, nil
}

func (a *RuntimeServiceV1Alpha2Adapter) PodSandboxStatus(ctx context.Context, sandboxID string, verbose bool) (criPodSandboxStatusResponse, error) {
	rawStatus, err := a.client.PodSandboxStatus(ctx, &criv1alpha2.PodSandboxStatusRequest{
		PodSandboxId: sandboxID,
		Verbose:      verbose,
	})
	if err != nil {
		return criPodSandboxStatusResponse{}, err
	}

	status := &criPodSandboxStatus{
		Id:             rawStatus.Status.Id,
		State:          criContainerState(rawStatus.Status.State),
		CreatedAt:      rawStatus.Status.CreatedAt,
		Labels:         rawStatus.Status.Labels,
		Annotations:    rawStatus.Status.Annotations,
		RuntimeHandler: rawStatus.Status.RuntimeHandler,
	}
	if rawStatus.Status.Metadata != nil {
		status.Metadata = &criPodSandboxMetadata{
			Name:      rawStatus.Status.Metadata.Name,
			Uid:       rawStatus.Status.Metadata.Uid,
			Namespace: rawStatus.Status.Metadata.Namespace,
			Attempt:   rawStatus.Status.Metadata.Attempt,
		}
	}

	return criPodSandboxStatusResponse{
		Status: status,
		Info:   rawStatus.Info,
	}, nil
}
