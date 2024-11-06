package setup

import (
	"context"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"time"

	"gopkg.in/yaml.v3"

	"github.com/alibaba/ilogtail/pkg/protocol"
	"github.com/alibaba/ilogtail/test/config"
	"github.com/alibaba/ilogtail/test/engine/setup/controller"
	"github.com/alibaba/ilogtail/test/engine/setup/dockercompose"
)

const dependencyHome = "test_cases"

type DockerComposeEnv struct {
	BootController *controller.BootController
	BootType       dockercompose.BootType
}

func SetDockerComposeBootType(t dockercompose.BootType) error {
	if dockerComposeEnv, ok := Env.(*DockerComposeEnv); ok {
		if t != dockercompose.DockerComposeBootTypeE2E && t != dockercompose.DockerComposeBootTypeBenchmark {
			return fmt.Errorf("invalid docker compose boot type, not e2e or benchmark")
		}
		dockerComposeEnv.BootType = t
		return nil
	}
	return fmt.Errorf("env is not docker-compose")
}

func StartDockerComposeEnv(ctx context.Context, dependencyName string) (context.Context, error) {
	if dockerComposeEnv, ok := Env.(*DockerComposeEnv); ok {
		path := dependencyHome + "/" + dependencyName
		err := config.Load(path, config.TestConfig.Profile)
		if err != nil {
			return ctx, err
		}
		dockerComposeEnv.BootController = new(controller.BootController)
		if err = dockerComposeEnv.BootController.Init(dockerComposeEnv.BootType); err != nil {
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
	env.BootType = dockercompose.DockerComposeBootTypeE2E
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

func (d *DockerComposeEnv) ExecOnLogtail(command string) (string, error) {
	return "", fmt.Errorf("not implemented")
}

func (d *DockerComposeEnv) ExecOnSource(ctx context.Context, command string) (string, error) {
	// exec on host of docker compose
	cmd := exec.Command(command)
	stdout, err := cmd.StdoutPipe()
	if err != nil {
		return "", err
	}
	stderr, err := cmd.StderrPipe()
	if err != nil {
		return "", err
	}
	if err := cmd.Start(); err != nil {
		return "", err
	}
	buf := make([]byte, 1024)
	n, _ := stdout.Read(buf)
	if n > 0 {
		return string(buf[:n]), nil
	}
	n, _ = stderr.Read(buf)
	if n > 0 {
		return string(buf[:n]), nil
	}
	if err := cmd.Wait(); err != nil {
		return "", err
	}
	return "", nil
}
