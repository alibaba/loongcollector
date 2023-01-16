package helper

import (
	"context"
	"testing"

	"github.com/docker/docker/api/types"
	"github.com/docker/docker/api/types/container"
	"github.com/stretchr/testify/suite"

	"github.com/alibaba/ilogtail/pkg/flags"
	"github.com/alibaba/ilogtail/pkg/logger"
)

func TestContainerExport(t *testing.T) {
	suite.Run(t, new(containerExportTestSuite))
}

func (s *containerExportTestSuite) TestCastContainerDetail() {

	envSet := make(map[string]struct{})
	labelSet := make(map[string]struct{})

	envSet["testEnv1"] = struct{}{}
	labelSet["testLebel1"] = struct{}{}

	info := mockDockerInfoDetail("testConfig", []string{"testEnv1=stdout"})
	cInfo := CastContainerDetail(info, envSet, labelSet)

	s.Equal(1, len(cInfo.Env))
	s.Equal(0, len(cInfo.Labels))
}

func (s *containerExportTestSuite) TestGetAllContainerIncludeEnvAndLabelToRecord() {
	envSet := make(map[string]struct{})
	labelSet := make(map[string]struct{})

	diffEnvSet := make(map[string]struct{})
	diffLabelSet := make(map[string]struct{})

	envSet["testEnv1"] = struct{}{}
	labelSet["testLebel1"] = struct{}{}

	diffEnvSet["testEnv1"] = struct{}{}

	info := mockDockerInfoDetail("testConfig", []string{"testEnv1=stdout"})
	cMap := GetContainerMap()
	cMap["test"] = info

	result := GetAllContainerIncludeEnvAndLabelToRecord(envSet, labelSet, diffEnvSet, diffLabelSet)
	s.Equal(1, len(result))
}

func (s *containerExportTestSuite) TestGetAllContainerToRecord() {
	envSet := make(map[string]struct{})
	labelSet := make(map[string]struct{})

	envSet["testEnv1"] = struct{}{}
	labelSet["testLebel1"] = struct{}{}

	info := mockDockerInfoDetail("testConfig", []string{"testEnv1=stdout"})
	cMap := GetContainerMap()
	cMap["test"] = info

	result := GetAllContainerToRecord(envSet, labelSet, make(map[string]struct{}))
	s.Equal(1, len(result))
}

type containerExportTestSuite struct {
	suite.Suite
}

func (s *containerExportTestSuite) BeforeTest(suiteName, testName string) {
	logger.Infof(context.Background(), "========== %s %s test start ========================", suiteName, testName)
}

func (s *containerExportTestSuite) AfterTest(suiteName, testName string) {
	logger.Infof(context.Background(), "========== %s %s test end =======================", suiteName, testName)

}

func mockDockerInfoDetail(containerName string, envList []string) *DockerInfoDetail {
	dockerInfo := types.ContainerJSON{
		ContainerJSONBase: &types.ContainerJSONBase{
			Name: containerName,
			ID:   "test",
		},
	}
	dockerInfo.Config = &container.Config{}
	dockerInfo.Config.Env = envList
	return CreateContainerInfoDetail(dockerInfo, *flags.LogConfigPrefix, false)
}
