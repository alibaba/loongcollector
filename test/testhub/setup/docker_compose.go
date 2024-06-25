package setup

import (
	"context"
	"fmt"
	"os"
	"path/filepath"
	"time"

	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/test/config"
	"github.com/alibaba/ilogtail/test/testhub/setup/controller"
	"gopkg.in/yaml.v3"
)

const dependencyHome = "test_cases/dependencies"

type DockerComposeEnv struct {
	BootController *controller.BootController
}

func StartDockerComposeEnv(ctx context.Context, dependencyName string) (context.Context, error) {
	if dockerComposeEnv, ok := Env.(*DockerComposeEnv); ok {
		path := dependencyHome + "/" + dependencyName
		err := config.Load(path, config.TestConfig.Profile)
		if err != nil {
			return ctx, err
		}
		dockerComposeEnv.BootController = new(controller.BootController)
		if err = dockerComposeEnv.BootController.Init(); err != nil {
			return ctx, err
		}

		startTime := time.Now().Unix()
		if err = dockerComposeEnv.BootController.Start(); err != nil {
			return ctx, err
		}
		return context.WithValue(ctx, config.StartTimeContextKey, int32(startTime)), nil
	}
	return ctx, fmt.Errorf("env is not docker-compose")
}

func SetDockerComposeDependOn(ctx context.Context, dependOnContainers string) (context.Context, error) {
	if _, ok := Env.(*DockerComposeEnv); ok {
		containers := make([]string, 0)
		yaml.Unmarshal([]byte(dependOnContainers), &containers)
		ctx = context.WithValue(ctx, config.DependOnContainerKey, containers)
	} else {
		return ctx, fmt.Errorf("env is not docker-compose")
	}
	return ctx, nil
}

func NewDockerComposeEnv() *DockerComposeEnv {
	env := &DockerComposeEnv{}
	root, _ := filepath.Abs(".")
	reportDir := root + "/report/"
	_ = os.Mkdir(reportDir, 0750)
	config.ConfigDir = reportDir + "config"
	return env
}

func (d *DockerComposeEnv) GetType() string {
	return "docker-compose"
}

func (d *DockerComposeEnv) GetData() (*protocol.LogGroup, error) {
	return nil, fmt.Errorf("not implemented")
}

func (d *DockerComposeEnv) Clean() error {
	d.BootController.Clean()
	return nil
}

func (d *DockerComposeEnv) ExecOnLogtail(command string) error {
	return fmt.Errorf("not implemented")
}

func (h *DockerComposeEnv) ExecOnSource(command string) error {
	return fmt.Errorf("not implemented")
}
