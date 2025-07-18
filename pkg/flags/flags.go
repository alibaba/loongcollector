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

package flags

import (
	"context"
	"encoding/json"
	"flag"
	"fmt"
	"os"
	"strconv"
	"strings"
	"sync"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/util"
)

const (
	DeployDaemonset   = "daemonset"
	DeployStatefulSet = "statefulset"
	DeploySingleton   = "singleton"
)

const (
	DefaultGlobalConfig     = `{"InputIntervalMs":5000,"AggregatIntervalMs":30,"FlushIntervalMs":30,"DefaultLogQueueSize":11,"DefaultLogGroupQueueSize":12}`
	DefaultPluginConfig     = `{"inputs":[{"type":"metric_mock","detail":{"Tags":{"tag1":"aaaa","tag2":"bbb"},"Fields":{"Content":"xxxxx","time":"2017.09.12 20:55:36"}}}],"flushers":[{"type":"flusher_stdout"}]}`
	DefaultFlusherConfig    = `{"type":"flusher_sls","detail":{}}`
	LoongcollectorEnvPrefix = "LOONG_"
)

var (
	flusherType     string
	flusherCfg      map[string]interface{}
	flusherLoadOnce sync.Once
)

type LogType string

const (
	LogTypeInfo    LogType = "info"
	LogTypeDebug   LogType = "debug"
	LogTypeWarning LogType = "warning"
	LogTypeError   LogType = "error"
)

// LogInfo contains metadata about a log message
type LogInfo struct {
	LogType LogType
	Content string
}

var (
	LogsWaitToPrint = []LogInfo{}
)

// flags used to control ilogtail.
var (
	K8sFlag = flag.Bool("ALICLOUD_LOG_K8S_FLAG", false, "alibaba log k8s event config flag, set true if you want to use it")
	// DockerConfigInitFlag is the alibaba log docker env config flag, set yes if you want to use it. And it is also a special flag to control enable go part in ilogtail. If you just want to
	// enable logtail plugin and off the env config, set the env called ALICLOUD_LOG_PLUGIN_ENV_CONFIG with false.
	DockerConfigInitFlag       = flag.Bool("ALICLOUD_LOG_DOCKER_ENV_CONFIG", false, "alibaba log docker env config flag, set true if you want to use it")
	DockerConfigPluginInitFlag = flag.Bool("ALICLOUD_LOG_PLUGIN_ENV_CONFIG", true, "alibaba log docker env config flag, set true if you want to use it")
	// AliCloudECSFlag set true if your docker is on alicloud ECS, so we can use ECS meta
	AliCloudECSFlag = flag.Bool("ALICLOUD_LOG_ECS_FLAG", false, "set true if your docker is on alicloud ECS, so we can use ECS meta")

	// DockerConfigPrefix docker env config prefix
	DockerConfigPrefix = flag.String("ALICLOUD_LOG_DOCKER_CONFIG_PREFIX", "aliyun_logs_", "docker env config prefix")

	// LogServiceEndpoint default project to create config
	// https://www.alibabacloud.com/help/doc-detail/29008.htm
	LogServiceEndpoint = flag.String("ALICLOUD_LOG_ENDPOINT", "cn-hangzhou.log.aliyuncs.com", "log service endpoint of your project's region")

	// DefaultLogProject default project to create config
	DefaultLogProject = flag.String("ALICLOUD_LOG_DEFAULT_PROJECT", "", "default project to create config")

	// DefaultLogMachineGroup default project to create config
	DefaultLogMachineGroup = flag.String("ALICLOUD_LOG_DEFAULT_MACHINE_GROUP", "", "default project to create config")

	// LogResourceCacheExpireSec log service's resources cache expire seconds
	LogResourceCacheExpireSec = flag.Int("ALICLOUD_LOG_CACHE_EXPIRE_SEC", 600, "log service's resources cache expire seconds")

	// LogOperationMaxRetryTimes log service's operation max retry times
	LogOperationMaxRetryTimes = flag.Int("ALICLOUD_LOG_OPERATION_MAX_TRY", 3, "log service's operation max retry times")

	// DefaultAccessKeyID your log service's access key id
	DefaultAccessKeyID = flag.String("ALICLOUD_LOG_ACCESS_KEY_ID", "xxxxxxxxx", "your log service's access key id")

	// DefaultAccessKeySecret your log service's access key secret
	DefaultAccessKeySecret = flag.String("ALICLOUD_LOG_ACCESS_KEY_SECRET", "xxxxxxxxx", "your log service's access key secret")

	// DefaultSTSToken your sts token
	DefaultSTSToken = flag.String("ALICLOUD_LOG_STS_TOKEN", "", "set sts token if you use sts")

	// LogConfigPrefix config prefix
	LogConfigPrefix = flag.String("ALICLOUD_LOG_CONFIG_PREFIX", "aliyun_logs_", "config prefix")

	// DockerEnvUpdateInterval docker env config update interval seconds
	DockerEnvUpdateInterval = flag.Int("ALICLOUD_LOG_ENV_CONFIG_UPDATE_INTERVAL", 10, "docker env config update interval seconds")

	// ProductAPIDomain product domain
	ProductAPIDomain = flag.String("ALICLOUD_LOG_PRODUCT_DOMAIN", "sls.aliyuncs.com", "product domain config")

	// DefaultRegion default log region"
	DefaultRegion = flag.String("ALICLOUD_LOG_REGION", "", "default log region")

	SelfEnvConfigFlag bool

	EnableContainerdUpperDirDetect = flag.Bool("enable_containerd_upper_dir_detect", false, "if enable containerd upper dir detect when locating rootfs")
	GlobalConfig                   = flag.String("global", "./global.json", "global config.")
	PluginConfig                   = flag.String("plugin", "./plugin.json", "plugin config.")
	FlusherConfig                  = flag.String("flusher", "./default_flusher.json", "the default flusher configuration is used not only in the plugins without flusher but also to transfer the self telemetry data.")
	ForceSelfCollect               = flag.Bool("force-statics", false, "force collect self telemetry data before closing.")
	AutoProfile                    = flag.Bool("prof-auto", true, "auto dump prof file when prof-flag is open.")
	HTTPProfFlag                   = flag.Bool("prof-flag", false, "http pprof flag.")
	Cpuprofile                     = flag.String("cpu-profile", "cpu.prof", "write cpu profile to file.")
	Memprofile                     = flag.String("mem-profile", "mem.prof", "write mem profile to file.")
	HTTPAddr                       = flag.String("server", ":18689", "http server address.")
	Doc                            = flag.Bool("doc", false, "generate plugin docs")
	DocPath                        = flag.String("docpath", "./docs/en/plugins", "generate plugin docs")
	HTTPLoadFlag                   = flag.Bool("http-load", false, "export http endpoint for load plugin config.")
	FileIOFlag                     = flag.Bool("file-io", false, "use file for input or output.")
	InputFile                      = flag.String("input-file", "./input.log", "input file")
	InputField                     = flag.String("input-field", "content", "input file")
	InputLineLimit                 = flag.Int("input-line-limit", 1000, "input file")
	OutputFile                     = flag.String("output-file", "./output.log", "output file")
	StatefulSetFlag                = flag.Bool("ALICLOUD_LOG_STATEFULSET_FLAG", false, "alibaba log export ports flag, set true if you want to use it")

	DeployMode           = flag.String("DEPLOY_MODE", DeployDaemonset, "alibaba log deploy mode, daemonset or statefulset or singleton")
	EnableKubernetesMeta = flag.Bool("ENABLE_KUBERNETES_META", false, "enable kubernetes meta")
	ClusterID            = flag.String("GLOBAL_CLUSTER_ID", "", "cluster id")
	ClusterName          = flag.String("GLOBAL_CLUSTER_NAME", "", "cluster name")
	ClusterRegion        = flag.String("GLOBAL_CLUSTER_REGION", "", "cluster region")
	ClusterType          = flag.String("GLOBAL_CLUSTER_TYPE", "k8s", "cluster domain, configurable: k8s (default)")
)

// lookupFlag returns the flag.Flag for the given name, or an error if not found
func lookupFlag(name string) (*flag.Flag, error) {
	if f := flag.Lookup(name); f != nil {
		return f, nil
	}
	return nil, fmt.Errorf("flag %s not found", name)
}

// GetStringFlag returns the string value of the named flag
func GetStringFlag(name string) (string, error) {
	f, err := lookupFlag(name)
	if err != nil {
		return "", err
	}
	return f.Value.String(), nil
}

// GetBoolFlag returns the bool value of the named flag
func GetBoolFlag(name string) (bool, error) {
	f, err := lookupFlag(name)
	if err != nil {
		return false, err
	}

	if v, ok := f.Value.(flag.Getter); ok {
		if val, ok := v.Get().(bool); ok {
			return val, nil
		}
	}
	return false, fmt.Errorf("flag %s is not bool type", name)
}

// GetIntFlag returns the int value of the named flag
func GetIntFlag(name string) (int, error) {
	f, err := lookupFlag(name)
	if err != nil {
		return 0, err
	}

	if v, ok := f.Value.(flag.Getter); ok {
		if val, ok := v.Get().(int); ok {
			return val, nil
		}
	}
	return 0, fmt.Errorf("flag %s is not int type", name)
}

// GetFloat64Flag returns the float64 value of the named flag
func GetFloat64Flag(name string) (float64, error) {
	f, err := lookupFlag(name)
	if err != nil {
		return 0.0, err
	}

	if v, ok := f.Value.(flag.Getter); ok {
		if val, ok := v.Get().(float64); ok {
			return val, nil
		}
	}
	return 0.0, fmt.Errorf("flag %s is not float64 type", name)
}

// SetStringFlag sets the string value of the named flag
func SetStringFlag(name, value string) error {
	f, err := lookupFlag(name)
	if err != nil {
		return err
	}
	return f.Value.Set(value)
}

// SetBoolFlag sets the bool value of the named flag
func SetBoolFlag(name string, value bool) error {
	f, err := lookupFlag(name)
	if err != nil {
		return err
	}
	return f.Value.Set(strconv.FormatBool(value))
}

// SetIntFlag sets the int value of the named flag
func SetIntFlag(name string, value int) error {
	f, err := lookupFlag(name)
	if err != nil {
		return err
	}
	return f.Value.Set(strconv.Itoa(value))
}

// SetFloat64Flag sets the float64 value of the named flag
func SetFloat64Flag(name string, value float64) error {
	f, err := lookupFlag(name)
	if err != nil {
		return err
	}
	return f.Value.Set(strconv.FormatFloat(value, 'g', -1, 64))
}

// LoadEnvToFlags loads environment variables into flags
func LoadEnvToFlags() {
	for _, env := range os.Environ() {
		name, value, found := strings.Cut(env, "=")
		if !found {
			continue
		}
		var flagName string
		if strings.HasPrefix(name, LoongcollectorEnvPrefix) {
			flagName = strings.ToLower(strings.TrimPrefix(name, LoongcollectorEnvPrefix))
		} else {
			flagName = name
		}

		f := flag.Lookup(flagName)
		if f == nil {
			continue
		}

		oldValue := f.Value.String()
		getter, ok := f.Value.(flag.Getter)
		if !ok {
			LogsWaitToPrint = append(LogsWaitToPrint, LogInfo{
				LogType: LogTypeError,
				Content: fmt.Sprintf("Flag does not support Get operation, flag: %s, value: %s", flagName, oldValue),
			})
			continue
		}

		actualValue := getter.Get()
		var err error

		// Validate value type before setting
		switch actualValue.(type) {
		case bool:
			_, err = strconv.ParseBool(value)
		case int, int64:
			_, err = strconv.ParseInt(value, 10, 64)
		case uint, uint64:
			_, err = strconv.ParseUint(value, 10, 64)
		case float64:
			_, err = strconv.ParseFloat(value, 64)
		case string:
			// No validation needed
		default:
			LogsWaitToPrint = append(LogsWaitToPrint, LogInfo{
				LogType: LogTypeError,
				Content: fmt.Sprintf("Unsupported flag type: %s (%T)", flagName, actualValue),
			})
			continue
		}

		if err != nil {
			LogsWaitToPrint = append(LogsWaitToPrint, LogInfo{
				LogType: LogTypeError,
				Content: fmt.Sprintf("Invalid value for flag %s (%T): %s - %v", flagName, actualValue, value, err),
			})
			continue
		}

		if err := f.Value.Set(value); err != nil {
			LogsWaitToPrint = append(LogsWaitToPrint, LogInfo{
				LogType: LogTypeError,
				Content: fmt.Sprintf("Failed to set flag %s: %v (old: %s, new: %s)", flagName, err, oldValue, value),
			})
			continue
		}

		LogsWaitToPrint = append(LogsWaitToPrint, LogInfo{
			LogType: LogTypeInfo,
			Content: fmt.Sprintf("Updated flag %s (%T): %s -> %s", flagName, actualValue, oldValue, f.Value.String()),
		})
	}
}

func init() {
	_ = util.InitFromEnvBool("ALICLOUD_LOG_K8S_FLAG", K8sFlag, *K8sFlag)
	_ = util.InitFromEnvBool("ALICLOUD_LOG_DOCKER_ENV_CONFIG", DockerConfigInitFlag, *DockerConfigInitFlag)
	_ = util.InitFromEnvBool("ALICLOUD_LOG_ECS_FLAG", AliCloudECSFlag, *AliCloudECSFlag)
	_ = util.InitFromEnvString("ALICLOUD_LOG_DOCKER_CONFIG_PREFIX", DockerConfigPrefix, *DockerConfigPrefix)
	_ = util.InitFromEnvString("ALICLOUD_LOG_DEFAULT_PROJECT", DefaultLogProject, *DefaultLogProject)
	_ = util.InitFromEnvString("ALICLOUD_LOG_DEFAULT_MACHINE_GROUP", DefaultLogMachineGroup, *DefaultLogMachineGroup)
	_ = util.InitFromEnvString("ALICLOUD_LOG_ENDPOINT", LogServiceEndpoint, *LogServiceEndpoint)
	_ = util.InitFromEnvString("ALICLOUD_LOG_ACCESS_KEY_ID", DefaultAccessKeyID, *DefaultAccessKeyID)
	_ = util.InitFromEnvString("ALICLOUD_LOG_ACCESS_KEY_SECRET", DefaultAccessKeySecret, *DefaultAccessKeySecret)
	_ = util.InitFromEnvString("ALICLOUD_LOG_STS_TOKEN", DefaultSTSToken, *DefaultSTSToken)
	_ = util.InitFromEnvString("ALICLOUD_LOG_CONFIG_PREFIX", LogConfigPrefix, *LogConfigPrefix)
	_ = util.InitFromEnvString("ALICLOUD_LOG_PRODUCT_DOMAIN", ProductAPIDomain, *ProductAPIDomain)
	_ = util.InitFromEnvString("ALICLOUD_LOG_REGION", DefaultRegion, *DefaultRegion)
	_ = util.InitFromEnvBool("ALICLOUD_LOG_PLUGIN_ENV_CONFIG", DockerConfigPluginInitFlag, *DockerConfigPluginInitFlag)

	_ = util.InitFromEnvBool("enable_containerd_upper_dir_detect", EnableContainerdUpperDirDetect, *EnableContainerdUpperDirDetect)
	_ = util.InitFromEnvBool("LOGTAIL_DEBUG_FLAG", HTTPProfFlag, *HTTPProfFlag)
	_ = util.InitFromEnvBool("LOGTAIL_AUTO_PROF", AutoProfile, *AutoProfile)
	_ = util.InitFromEnvBool("LOGTAIL_FORCE_COLLECT_SELF_TELEMETRY", ForceSelfCollect, *ForceSelfCollect)
	_ = util.InitFromEnvBool("LOGTAIL_HTTP_LOAD_CONFIG", HTTPLoadFlag, *HTTPLoadFlag)
	_ = util.InitFromEnvBool("ALICLOUD_LOG_STATEFULSET_FLAG", StatefulSetFlag, *StatefulSetFlag)

	_ = util.InitFromEnvString("DEPLOY_MODE", DeployMode, *DeployMode)
	_ = util.InitFromEnvBool("ENABLE_KUBERNETES_META", EnableKubernetesMeta, *EnableKubernetesMeta)
	_ = util.InitFromEnvString("GLOBAL_CLUSTER_ID", ClusterID, *ClusterID)
	_ = util.InitFromEnvString("GLOBAL_CLUSTER_NAME", ClusterName, *ClusterName)
	_ = util.InitFromEnvString("GLOBAL_CLUSTER_REGION", ClusterRegion, *ClusterRegion)
	_ = util.InitFromEnvString("GLOBAL_CLUSTER_TYPE", ClusterType, *ClusterType)

	if len(*DefaultRegion) == 0 {
		*DefaultRegion = util.GuessRegionByEndpoint(*LogServiceEndpoint, "cn-hangzhou")
		LogsWaitToPrint = append(LogsWaitToPrint, LogInfo{
			LogType: LogTypeInfo,
			Content: fmt.Sprintf("guess region by endpoint, endpoint: %s, region: %s", *LogServiceEndpoint, *DefaultRegion),
		})
	}

	_ = util.InitFromEnvInt("ALICLOUD_LOG_ENV_CONFIG_UPDATE_INTERVAL", DockerEnvUpdateInterval, *DockerEnvUpdateInterval)

	if *DockerConfigInitFlag && *DockerConfigPluginInitFlag {
		_ = util.InitFromEnvBool("ALICLOUD_LOG_DOCKER_ENV_CONFIG_SELF", &SelfEnvConfigFlag, false)
	}
	// 最后执行，优先级最高
	LoadEnvToFlags()
}

// GetFlusherConfiguration returns the flusher category and options.
func GetFlusherConfiguration() (flusherCategory string, flusherOptions map[string]interface{}) {
	flusherLoadOnce.Do(func() {
		extract := func(cfg []byte) (string, map[string]interface{}, bool) {
			m := make(map[string]interface{})
			err := json.Unmarshal(cfg, &m)
			if err != nil {
				logger.Error(context.Background(), "DEFAULT_FLUSHER_ALARM", "err", err)
				return "", nil, false
			}
			c, ok := m["type"].(string)
			if !ok {
				return "", nil, false
			}
			options, ok := m["detail"].(map[string]interface{})
			if !ok {
				return c, nil, true
			}
			return c, options, true
		}
		if fCfg, err := os.ReadFile(*FlusherConfig); err == nil {
			category, options, ok := extract(fCfg)
			if ok {
				flusherType = category
				flusherCfg = options
			} else {
				flusherType, flusherCfg, _ = extract([]byte(DefaultFlusherConfig))
			}
		} else {
			flusherType, flusherCfg, _ = extract([]byte(DefaultFlusherConfig))
		}

	})
	return flusherType, flusherCfg
}
