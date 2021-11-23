module github.com/alibaba/ilogtail/test

go 1.16

require (
	github.com/docker/docker v20.10.7+incompatible
	github.com/docker/go-connections v0.4.0
	github.com/docker/go-units v0.4.0
	github.com/mitchellh/mapstructure v1.4.1
	github.com/pkg/profile v1.6.0 // indirect
	github.com/spf13/viper v1.8.1
	github.com/stretchr/testify v1.7.0
	github.com/testcontainers/testcontainers-go v0.11.1
	github.com/urfave/cli/v2 v2.3.0
	google.golang.org/grpc v1.40.0
	gopkg.in/yaml.v3 v3.0.0-20210107192922-496545a6307b
	github.com/alibaba/ilogtail/pkg v0.0.0
)

replace github.com/alibaba/ilogtail/pkg => ../pkg
