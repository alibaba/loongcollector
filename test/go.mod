module github.com/alibaba/ilogtail/test

go 1.16

require (
	github.com/Microsoft/go-winio v0.5.2 // indirect
	github.com/Microsoft/hcsshim v0.9.4 // indirect
	github.com/alibaba/ilogtail/pkg v0.0.0
	github.com/cenkalti/backoff/v4 v4.1.3 // indirect
	github.com/containerd/cgroups v1.0.4 // indirect
	github.com/containerd/containerd v1.6.6 // indirect
	github.com/docker/distribution v2.8.1+incompatible // indirect
	github.com/docker/docker v20.10.17+incompatible
	github.com/docker/go-connections v0.4.0
	github.com/docker/go-units v0.4.0
	github.com/magiconair/properties v1.8.6 // indirect
	github.com/mitchellh/mapstructure v1.4.1
	github.com/moby/sys/mount v0.3.3 // indirect
	github.com/opencontainers/runc v1.1.3 // indirect
	github.com/sirupsen/logrus v1.9.0 // indirect
	github.com/spf13/viper v1.8.1
	github.com/stretchr/testify v1.7.0
	github.com/testcontainers/testcontainers-go v0.13.0
	github.com/urfave/cli/v2 v2.3.0
	golang.org/x/net v0.0.0-20220722155237-a158d28d115b // indirect
	golang.org/x/sys v0.0.0-20220722155257-8c9f86f7a55f // indirect
	google.golang.org/genproto v0.0.0-20220722212130-b98a9ff5e252 // indirect
	google.golang.org/grpc v1.48.0
	gopkg.in/yaml.v3 v3.0.1
)

replace github.com/alibaba/ilogtail/pkg => ../pkg
