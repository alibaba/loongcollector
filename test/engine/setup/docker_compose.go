package setup

import (
	"context"
	"fmt"
	"os"
	"path/filepath"
	"time"

	"gopkg.in/yaml.v3"

	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/test/config"
	"github.com/alibaba/ilogtail/test/engine/setup/controller"
)

const dependencyHome = "test_cases"

var dockerComposeType = "e2e"

type DockerComposeEnv struct {
	BootController *controller.BootController
}

func SetDockerComposeType(t string) {
	dockerComposeType = t
}

func StartDockerComposeEnv(ctx context.Context, dependencyName string) (context.Context, error) {
	if dockerComposeEnv, ok := Env.(*DockerComposeEnv); ok {
		path := dependencyHome + "/" + dependencyName
		err := config.Load(path, config.TestConfig.Profile)
		if err != nil {
			return ctx, err
		}
		dockerComposeEnv.BootController = new(controller.BootController)
		if err = dockerComposeEnv.BootController.Init(dockerComposeType); err != nil {
			return ctx, err
		}

		startTime := time.Now().Unix()
		if err = dockerComposeEnv.BootController.Start(ctx); err != nil {
			return ctx, err
		}
		return context.WithValue(ctx, config.StartTimeContextKey, int32(startTime)), nil
	}
	return ctx, fmt.Errorf("env is not docker-compose")
}

func SetDockerComposeDependOn(ctx context.Context, dependOnContainers string) (context.Context, error) {
	if _, ok := Env.(*DockerComposeEnv); ok {
		containers := make([]string, 0)
		err := yaml.Unmarshal([]byte(dependOnContainers), &containers)
		if err != nil {
			return ctx, err
		}
		ctx = context.WithValue(ctx, config.DependOnContainerKey, containers)
	} else {
		return ctx, fmt.Errorf("env is not docker-compose")
	}
	return ctx, nil
}

func MountVolume(ctx context.Context, source, target string) (context.Context, error) {
	if _, ok := Env.(*DockerComposeEnv); ok {
		var existVolume []string
		var ok bool
		if existVolume, ok = ctx.Value(config.MountVolumeKey).([]string); ok {
			existVolume = append(existVolume, source+":"+target)
		} else {
			existVolume = []string{source + ":" + target}
		}
		ctx = context.WithValue(ctx, config.MountVolumeKey, existVolume)
	} else {
		return ctx, fmt.Errorf("env is not docker-compose")
	}
	return ctx, nil
}

func ExposePort(ctx context.Context, source, target string) (context.Context, error) {
	if _, ok := Env.(*DockerComposeEnv); ok {
		var existPort []string
		var ok bool
		if existPort, ok = ctx.Value(config.ExposePortKey).([]string); ok {
			existPort = append(existPort, source+":"+target)
		} else {
			existPort = []string{source + ":" + target}
		}
		ctx = context.WithValue(ctx, config.ExposePortKey, existPort)
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

func (d *DockerComposeEnv) ExecOnSource(command string) error {
	return fmt.Errorf("not implemented")
}
