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

package validator

import (
	"context"
	"fmt"
	"io/ioutil"
	"testing"
	"time"

	"github.com/docker/docker/api/types"
	containertypes "github.com/docker/docker/api/types/container"
	networktypes "github.com/docker/docker/api/types/network"
	"github.com/docker/docker/client"
	"github.com/docker/go-units"
	"github.com/stretchr/testify/assert"
)

func createDockerContainer(cli *client.Client, t *testing.T) string {
	config := containertypes.Config{
		Image: "ubuntu:18.04",
		Cmd: []string{
			"cat", "/dev/urandom", "gzip -9", ">", "/dev/null",
		},
	}
	res, err := cli.ImageList(context.Background(), types.ImageListOptions{})
	assert.NoError(t, err)
	var found bool
	for _, re := range res {
		for _, tag := range re.RepoTags {
			if tag == "ubuntu:18.04" {
				found = true
			}
		}
	}
	if !found {
		pull, errPull := cli.ImagePull(context.Background(), "ubuntu:18.04", types.ImagePullOptions{})
		assert.NoError(t, errPull)
		defer pull.Close()
		// download of docker image finishes at EOF of the pull request
		_, errPull = ioutil.ReadAll(pull)
		assert.NoError(t, errPull)
	}

	container, err := cli.ContainerCreate(context.Background(), &config, &containertypes.HostConfig{}, &networktypes.NetworkingConfig{}, nil, "")
	assert.NoError(t, err)

	err = cli.ContainerStart(context.Background(), container.ID, types.ContainerStartOptions{})
	assert.NoError(t, err)
	return container.ID
}

func closeDockerContainer(cli *client.Client, id string) {
	_ = cli.ContainerStop(context.Background(), id, nil)
}

func Test_dockerProfileValidator_FetchProfile(t *testing.T) {
	cli, err := client.NewClientWithOpts(client.FromEnv)
	assert.NoError(t, err)
	defer cli.Close()

	ID := createDockerContainer(cli, t)
	defer closeDockerContainer(cli, ID)

	// test cpu
	v := &dockerProfileSystemValidator{
		ExpectEverySecondMaximumCPU: 0.1,
		cli:                         cli,
		containerID:                 ID,
		cancel:                      make(chan struct{}),
	}
	for i := 0; i < 3; i++ {
		v.FetchProfile()
		time.Sleep(time.Second)
	}
	result := v.FetchResult()
	assert.Equal(t, 3, len(result))
	for _, report := range result {
		fmt.Printf("%s\n", report)
	}

	// test mem
	limit := "1k"
	v = &dockerProfileSystemValidator{
		ExpectEverySecondMaximumMem: limit,
		cli:                         cli,
		containerID:                 ID,
		cancel:                      make(chan struct{}),
	}
	size, _ := units.FromHumanSize(limit)
	v.memSize = size
	for i := 0; i < 3; i++ {
		v.FetchProfile()
		time.Sleep(time.Second)
	}
	result = v.FetchResult()
	assert.Equal(t, 3, len(result))
	for _, report := range result {
		fmt.Printf("%s\n", report)
	}
}
