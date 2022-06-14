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

package helper

import (
	"context"
	"encoding/json"
	"hash/fnv"
	"path"
	"regexp"
	"runtime"
	"strings"
	"sync"
	"sync/atomic"
	"time"

	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/util"

	docker "github.com/fsouza/go-dockerclient"
)

var dockerCenterInstance *DockerCenter
var containerFindingManager *ContainerDiscoverManager
var onceDocker sync.Once

// set default value to aliyun_logs_
var envConfigPrefix = "aliyun_logs_"

const DockerTimeFormat = "2006-01-02T15:04:05.999999999Z"

var DefaultSyncContainersPeriod = time.Second * 3 // should be same as docker_config_update_interval gflag in C
var ContainerInfoTimeoutMax = time.Second * time.Duration(450)
var ContainerInfoDeletedTimeout = time.Second * time.Duration(30)
var EventListenerTimeout = time.Second * time.Duration(3600)

// "io.kubernetes.pod.name": "logtail-z2224",
// "io.kubernetes.pod.namespace": "kube-system",
// "io.kubernetes.pod.uid": "222e88ff-8f08-11e8-851d-00163f008685",
const k8sPodNameLabel = "io.kubernetes.pod.name"
const k8sPodNameSpaceLabel = "io.kubernetes.pod.namespace"
const k8sPodUUIDLabel = "io.kubernetes.pod.uid"
const k8sInnerLabelPrefix = "io.kubernetes"
const k8sInnerAnnotationPrefix = "annotation."

type EnvConfigInfo struct {
	ConfigName    string
	ConfigItemMap map[string]string
}

// K8SFilter used for find specific container
type K8SFilter struct {
	NamespaceReg     *regexp.Regexp
	PodReg           *regexp.Regexp
	ContainerReg     *regexp.Regexp
	IncludeLabels    map[string]string
	ExcludeLabels    map[string]string
	IncludeLabelRegs map[string]*regexp.Regexp
	ExcludeLabelRegs map[string]*regexp.Regexp
	hashKey          uint64
}

// CreateK8SFilter ...
func CreateK8SFilter(ns, pod, container string, includeK8sLabels, excludeK8sLabels map[string]string) (*K8SFilter, error) {
	var filter K8SFilter
	var err error
	var hashStrBuilder strings.Builder
	if len(ns) > 0 {
		if filter.NamespaceReg, err = regexp.Compile(ns); err != nil {
			return nil, err
		}
	}
	hashStrBuilder.WriteString(ns)
	hashStrBuilder.WriteString("$$$")
	if len(pod) > 0 {
		if filter.PodReg, err = regexp.Compile(pod); err != nil {
			return nil, err
		}
	}
	hashStrBuilder.WriteString(pod)
	hashStrBuilder.WriteString("$$$")
	if len(container) > 0 {
		if filter.ContainerReg, err = regexp.Compile(container); err != nil {
			return nil, err
		}
	}
	hashStrBuilder.WriteString(container)
	hashStrBuilder.WriteString("$$$")

	if filter.IncludeLabels, filter.IncludeLabelRegs, err = SplitRegexFromMap(includeK8sLabels); err != nil {
		return nil, err
	}
	for includeKey, val := range includeK8sLabels {
		hashStrBuilder.WriteString(includeKey)
		hashStrBuilder.WriteByte('#')
		hashStrBuilder.WriteString(val)
	}
	if filter.ExcludeLabels, filter.ExcludeLabelRegs, err = SplitRegexFromMap(excludeK8sLabels); err != nil {
		return nil, err
	}
	hashStrBuilder.WriteString("$$$")
	for excludeKey, val := range excludeK8sLabels {
		hashStrBuilder.WriteString(excludeKey)
		hashStrBuilder.WriteByte('#')
		hashStrBuilder.WriteString(val)
	}

	h := fnv.New64a()
	_, _ = h.Write([]byte(hashStrBuilder.String()))
	filter.hashKey = h.Sum64()
	return &filter, nil
}

// "io.kubernetes.container.logpath": "/var/log/pods/222e88ff-8f08-11e8-851d-00163f008685/logtail_0.log",
// "io.kubernetes.container.name": "logtail",
// "io.kubernetes.docker.type": "container",
// "io.kubernetes.pod.name": "logtail-z2224",
// "io.kubernetes.pod.namespace": "kube-system",
// "io.kubernetes.pod.uid": "222e88ff-8f08-11e8-851d-00163f008685",
type K8SInfo struct {
	Namespace       string
	Pod             string
	ContainerName   string
	Labels          map[string]string
	PausedContainer bool

	matchedCache map[uint64]bool
	mu           sync.Mutex
}

func (info *K8SInfo) IsSamePod(o *K8SInfo) bool {
	return info.Namespace == o.Namespace && info.Pod == o.Pod
}

func (info *K8SInfo) GetLabel(key string) string {
	if info.Labels != nil {
		return info.Labels[key]
	}
	return ""
}

// ExtractK8sLabels only work for original docker container.
func (info *K8SInfo) ExtractK8sLabels(containerInfo *docker.Container) {
	info.mu.Lock()
	defer info.mu.Unlock()
	// only pause container has k8s labels
	if info.ContainerName == "POD" || info.ContainerName == "pause" {
		info.PausedContainer = true
		if info.Labels == nil {
			info.Labels = make(map[string]string)
		}
		for key, val := range containerInfo.Config.Labels {
			if strings.HasPrefix(key, k8sInnerLabelPrefix) || strings.HasPrefix(key, k8sInnerAnnotationPrefix) {
				continue
			}
			info.Labels[key] = val
		}
	}
}

func (info *K8SInfo) Merge(o *K8SInfo) {
	info.mu.Lock()
	o.mu.Lock()
	defer info.mu.Unlock()
	defer o.mu.Unlock()

	// only pause container has k8s labels, so we can only check len(labels)
	if len(o.Labels) > len(info.Labels) {
		info.Labels = o.Labels
		info.matchedCache = nil
	}
	if len(o.Labels) < len(info.Labels) {
		o.Labels = info.Labels
		o.matchedCache = nil
	}
}

// IsMatch ...
func (info *K8SInfo) IsMatch(filter *K8SFilter) bool {
	if info.PausedContainer {
		return false
	}
	if filter == nil {
		return true
	}

	info.mu.Lock()
	defer info.mu.Unlock()
	if info.matchedCache == nil {
		info.matchedCache = make(map[uint64]bool)
	} else if cacheRst, ok := info.matchedCache[filter.hashKey]; ok {
		return cacheRst
	}
	var rst = info.innerMatch(filter)
	info.matchedCache[filter.hashKey] = rst
	return rst
}

// innerMatch ...
func (info *K8SInfo) innerMatch(filter *K8SFilter) bool {
	if filter.NamespaceReg != nil && !filter.NamespaceReg.MatchString(info.Namespace) {
		return false
	}
	if filter.PodReg != nil && !filter.PodReg.MatchString(info.Pod) {
		return false
	}
	if filter.ContainerReg != nil && !filter.ContainerReg.MatchString(info.ContainerName) {
		return false
	}
	// if labels is nil, create an empty map
	if info.Labels == nil {
		info.Labels = make(map[string]string)
	}
	return isMapLabelsMatch(filter.IncludeLabels, filter.ExcludeLabels, filter.IncludeLabelRegs, filter.ExcludeLabelRegs, info.Labels)
}

type DockerInfoDetail struct {
	ContainerInfo    *docker.Container
	ContainerNameTag map[string]string
	K8SInfo          *K8SInfo
	EnvConfigInfoMap map[string]*EnvConfigInfo
	ContainerIP      string
	DefaultRootPath  string

	lastUpdateTime time.Time
	deleteFlag     bool
}

func (did *DockerInfoDetail) IsTimeout() bool {
	nowTime := time.Now()
	if nowTime.Sub(did.lastUpdateTime) > ContainerInfoTimeoutMax ||
		(did.deleteFlag && nowTime.Sub(did.lastUpdateTime) > ContainerInfoDeletedTimeout) {
		return true
	}
	return false
}

func (did *DockerInfoDetail) GetExternalTags(envs, k8sLabels map[string]string) map[string]string {
	if len(envs) == 0 && len(k8sLabels) == 0 {
		return did.ContainerNameTag
	}
	tags := make(map[string]string)
	for k, v := range did.ContainerNameTag {
		tags[k] = v
	}
	for k, realName := range envs {
		tags[realName] = did.GetEnv(k)
	}
	if did.K8SInfo != nil {
		for k, realName := range k8sLabels {
			tags[realName] = did.K8SInfo.GetLabel(k)
		}
	}
	return tags

}

func (did *DockerInfoDetail) GetEnv(key string) string {
	for _, env := range did.ContainerInfo.Config.Env {
		kvPair := strings.SplitN(env, "=", 2)
		if len(kvPair) != 2 {
			continue
		}
		if key == kvPair[0] {
			return kvPair[1]
		}
	}
	return ""
}

func (did *DockerInfoDetail) DiffName(other *DockerInfoDetail) bool {
	return did.ContainerInfo.ID != other.ContainerInfo.ID || did.ContainerInfo.Name != other.ContainerInfo.Name
}

func (did *DockerInfoDetail) DiffMount(other *DockerInfoDetail) bool {
	return len(did.ContainerInfo.Config.Volumes) != len(other.ContainerInfo.Config.Volumes)
}

func isPathSeparator(c byte) bool {
	return c == '/' || c == '\\'
}

func (did *DockerInfoDetail) FindBestMatchedPath(pth string) (sourcePath, containerPath string) {
	pth = path.Clean(pth)
	pthSize := len(pth)

	logger.Debugf(context.Background(), "FindBestMatchedPath for container %s, target path: %s, containerInfo: %#v", did.ContainerInfo.ID, pth, did.ContainerInfo)

	// check mounts
	var bestMatchedMounts docker.Mount
	for _, mount := range did.ContainerInfo.Mounts {
		// logger.Debugf("container(%s-%s) mount: source-%s destination-%s", did.ContainerInfo.ID, did.ContainerInfo.Name, mount.Source, mount.Destination)

		dst := path.Clean(mount.Destination)
		dstSize := len(dst)

		if strings.HasPrefix(pth, dst) &&
			(pthSize == dstSize || (pthSize > dstSize && isPathSeparator(pth[dstSize]))) &&
			len(bestMatchedMounts.Destination) < dstSize {
			bestMatchedMounts = mount
		}
	}
	if len(bestMatchedMounts.Source) > 0 {
		return bestMatchedMounts.Source, bestMatchedMounts.Destination
	}

	return did.DefaultRootPath, ""
}

func (did *DockerInfoDetail) MakeSureEnvConfigExist(configName string) *EnvConfigInfo {
	if did.EnvConfigInfoMap == nil {
		did.EnvConfigInfoMap = make(map[string]*EnvConfigInfo)
	}
	config, ok := did.EnvConfigInfoMap[configName]
	if !ok {
		envConfig := &EnvConfigInfo{
			ConfigName:    configName,
			ConfigItemMap: make(map[string]string),
		}
		did.EnvConfigInfoMap[configName] = envConfig
		return envConfig
	}
	return config
}

// FindAllEnvConfig find and pre process all env config, add tags for docker info
func (did *DockerInfoDetail) FindAllEnvConfig(envConfigPrefix string, selfConfigFlag bool) {

	if len(envConfigPrefix) == 0 {
		return
	}
	selfEnvConfig := false
	for _, env := range did.ContainerInfo.Config.Env {
		kvPair := strings.SplitN(env, "=", 2)
		if len(kvPair) != 2 {
			continue
		}
		key := kvPair[0]
		value := kvPair[1]

		if key == "ALICLOUD_LOG_DOCKER_ENV_CONFIG_SELF" && (value == "true" || value == "TRUE") {
			logger.Debug(context.Background(), "this container is self env config", did.ContainerInfo.Name)
			selfEnvConfig = true
			continue
		}

		if !strings.HasPrefix(key, envConfigPrefix) {
			continue
		}
		logger.Debug(context.Background(), "docker env config, name", did.ContainerInfo.Name, "item", key)
		envKey := key[len(envConfigPrefix):]
		lastIndex := strings.LastIndexByte(envKey, '_')
		var configName string
		// end with '_', invalid, just skip
		if lastIndex == len(envKey)-1 {
			continue
		}

		// raw config
		if lastIndex < 0 {
			configName = envKey
		} else {
			configName = envKey[0:lastIndex]
		}
		// invalid config name
		if len(configName) == 0 {
			continue
		}
		envConfig := did.MakeSureEnvConfigExist(configName)
		if lastIndex < 0 {
			envConfig.ConfigItemMap[""] = value
		} else {
			tailKey := envKey[lastIndex+1:]
			envConfig.ConfigItemMap[tailKey] = value
			// process tags
			if tailKey == "tags" {
				tagKV := strings.SplitN(value, "=", 2)
				// if tag exist in EnvTags, just skip this tag
				if len(tagKV) == 2 {
					if !HasEnvTags(tagKV[0], tagKV[1]) {
						did.ContainerNameTag[tagKV[0]] = tagKV[1]
					} else {
						logger.Info(context.Background(), "skip set this tag, as this exist in self env tags, key", tagKV[0], "value", tagKV[1])
					}
				} else {
					if !HasEnvTags(tagKV[0], tagKV[0]) {
						did.ContainerNameTag[tagKV[0]] = tagKV[0]
					} else {
						logger.Info(context.Background(), "skip set this tag, as this exist in self env tags, key&value", tagKV[0])
					}
				}
			}
		}
	}
	logger.Debug(context.Background(), "docker env", did.ContainerInfo.Config.Env, "prefix", envConfigPrefix, "env config", did.EnvConfigInfoMap, "self env config", selfEnvConfig)
	// ignore self env config
	if !selfConfigFlag && selfEnvConfig {
		did.EnvConfigInfoMap = make(map[string]*EnvConfigInfo)
	}
}

type DockerCenter struct {
	// ContainerMap contains all container information.
	// For the docker scenario, the container list is the same as the result executed with `docker ps` commands. So the container
	// list would also contain the sandbox containers when docker is used as an engine in Kubernetes.
	// For the CRI scenario, the container list only contains the real containers and excludes the sandbox containers. But the
	// sandbox meta would be saved to its bound container.
	containerMap                   map[string]*DockerInfoDetail // all containers will in this map
	client                         *docker.Client
	lastErrMu                      sync.Mutex
	lastErr                        error
	lock                           sync.RWMutex
	lastUpdateMapTime              int64
	eventChan                      chan *docker.APIEvents
	eventChanLock                  sync.Mutex
	imageLock                      sync.RWMutex
	imageCache                     map[string]string
	lastFetchAllSuccessTime        time.Time
	initStaticContainerInfoSuccess bool
}

func getIPByHosts(hostFileName, hostname string) string {
	lines, err := util.ReadLines(hostFileName)
	if err != nil {
		logger.Info(context.Background(), "read container hosts file error, file", hostFileName, "error", err.Error())
		return ""
	}
	for _, line := range lines {
		if strings.HasPrefix(line, "#") {
			continue
		}
		if strings.Index(line, hostname) > 0 {
			line = strings.Trim(line, "#/* \t\n")
			return util.ReadFirstBlock(line)
		}
	}
	return ""
}

func (dc *DockerCenter) registerEventListener(c chan *docker.APIEvents) {
	dc.eventChanLock.Lock()
	defer dc.eventChanLock.Unlock()
	dc.eventChan = c
}

func (dc *DockerCenter) unRegisterEventListener(_ chan *docker.APIEvents) {
	dc.eventChanLock.Lock()
	defer dc.eventChanLock.Unlock()
	dc.eventChan = nil
}

func (dc *DockerCenter) lookupImageCache(id string) (string, bool) {
	dc.imageLock.RLock()
	defer dc.imageLock.RUnlock()
	imageName, ok := dc.imageCache[id]
	return imageName, ok
}

func (dc *DockerCenter) getImageName(id, defaultVal string) string {
	if len(id) == 0 || dc.client == nil {
		return defaultVal
	}
	if imageName, ok := dc.lookupImageCache(id); ok {
		return imageName
	}

	image, err := dc.client.InspectImage(id)
	logger.Debug(context.Background(), "get image name, id", id, "error", err)
	if err == nil && image != nil && len(image.RepoTags) > 0 {
		dc.imageLock.Lock()
		dc.imageCache[id] = image.RepoTags[0]
		dc.imageLock.Unlock()
		return image.RepoTags[0]
	}
	return defaultVal
}

func (dc *DockerCenter) getIPAddress(info *docker.Container) string {
	if detail, ok := dc.getContainerDetail(info.ID); ok && detail != nil {
		return detail.ContainerIP
	}
	if info.NetworkSettings != nil && len(info.NetworkSettings.IPAddress) > 0 {
		return info.NetworkSettings.IPAddress
	}
	if len(info.Config.Hostname) > 0 && len(info.HostsPath) > 0 {
		return getIPByHosts(GetMountedFilePath(info.HostsPath), info.Config.Hostname)
	}
	return ""
}

// CreateInfoDetail create DockerInfoDetail with docker.Container
// Container property used in this function : HostsPath, Config.Hostname, Name, Config.Image, Config.Env, Mounts
//                                            ContainerInfo.GraphDriver.Data["UpperDir"] Config.Labels
func (dc *DockerCenter) CreateInfoDetail(info *docker.Container, envConfigPrefix string, selfConfigFlag bool) *DockerInfoDetail {
	containerNameTag := make(map[string]string)
	k8sInfo := K8SInfo{}
	ip := dc.getIPAddress(info)

	containerNameTag["_image_name_"] = dc.getImageName(info.Image, info.Config.Image)
	if strings.HasPrefix(info.Name, "/k8s_") || strings.HasPrefix(info.Name, "k8s_") || strings.Count(info.Name, "_") >= 4 {
		// 1. container name is k8s
		// k8s_php-redis_frontend-2337258262-154p7_default_d8a2e2dd-3617-11e7-a4b0-ecf4bbe5d414_0
		// terway_terway-multi-ip-mgslw_kube-system_b07b491e-995a-11e9-94ea-00163e080931_8
		tags := strings.SplitN(info.Name, "_", 6)
		// containerNamePrefix：k8s
		// containerName：php-redis
		// podFullName：frontend-2337258262-154p7
		// computeHash：154p7
		// deploymentName：frontend
		// replicaSetName：frontend-2337258262
		// namespace：default
		// podUID：d8a2e2dd-3617-11e7-a4b0-ecf4bbe5d414

		baseIndex := 0
		if len(tags) == 6 {
			baseIndex = 1
		}
		containerNameTag["_container_name_"] = tags[baseIndex]
		containerNameTag["_pod_name_"] = tags[baseIndex+1]
		containerNameTag["_namespace_"] = tags[baseIndex+2]
		containerNameTag["_pod_uid_"] = tags[baseIndex+3]
		k8sInfo.ContainerName = tags[baseIndex]
		k8sInfo.Pod = tags[baseIndex+1]
		k8sInfo.Namespace = tags[baseIndex+2]
		k8sInfo.ExtractK8sLabels(info)
	} else if _, ok := info.Config.Labels[k8sPodNameLabel]; ok {
		// 2. container labels has k8sPodNameLabel
		containerNameTag["_container_name_"] = info.Name
		containerNameTag["_pod_name_"] = info.Config.Labels[k8sPodNameLabel]
		containerNameTag["_namespace_"] = info.Config.Labels[k8sPodNameSpaceLabel]
		containerNameTag["_pod_uid_"] = info.Config.Labels[k8sPodUUIDLabel]
		k8sInfo.ContainerName = info.Name
		k8sInfo.Pod = info.Config.Labels[k8sPodNameLabel]
		k8sInfo.Namespace = info.Config.Labels[k8sPodNameSpaceLabel]
		// the following method is couped with the CRI adapter, only the original docker container labels
		// would be added to the labels of the k8s info.
		k8sInfo.ExtractK8sLabels(info)
	} else {
		// 3. treat as normal container
		if strings.HasPrefix(info.Name, "/") {
			containerNameTag["_container_name_"] = info.Name[1:]
		} else {
			containerNameTag["_container_name_"] = info.Name
		}
	}
	if len(ip) > 0 {
		containerNameTag["_container_ip_"] = ip
	}

	did := &DockerInfoDetail{
		ContainerInfo:    info,
		ContainerNameTag: containerNameTag,
		K8SInfo:          &k8sInfo,
		ContainerIP:      ip,
		lastUpdateTime:   time.Now(),
	}
	did.FindAllEnvConfig(envConfigPrefix, selfConfigFlag)
	// @note for overlayfs only, some driver like nas, you can not see it in upper dir
	if info.GraphDriver != nil && info.GraphDriver.Data != nil {
		if rootPath, ok := did.ContainerInfo.GraphDriver.Data["UpperDir"]; ok {
			did.DefaultRootPath = rootPath
		}
	}
	// for cri-runtime
	if criRuntimeWrapper != nil && info.HostConfig != nil && len(did.DefaultRootPath) == 0 {
		did.DefaultRootPath = criRuntimeWrapper.lookupContainerRootfsAbsDir(info)
	}
	logger.Debugf(context.Background(), "container(id: %s, name: %s) default root path is %s", info.ID, info.Name, did.DefaultRootPath)
	return did
}

func getDockerCenterInstance() *DockerCenter {
	onceDocker.Do(func() {
		logger.Init()
		// load EnvTags first
		LoadEnvTags()
		dockerCenterInstance = &DockerCenter{}
		dockerCenterInstance.imageCache = make(map[string]string)
		dockerCenterInstance.containerMap = make(map[string]*DockerInfoDetail)
		if IsCRIRuntimeValid(containerdUnixSocket) {
			var err error
			criRuntimeWrapper, err = NewCRIRuntimeWrapper(dockerCenterInstance)
			if err != nil {
				logger.Errorf(context.Background(), "DOCKER_CENTER_ALARM", "[CRIRuntime] creare cri-runtime client error: %v", err)
				criRuntimeWrapper = nil
			} else {
				logger.Infof(context.Background(), "[CRIRuntime] create cri-runtime client successfully")
			}
		}
		if ok, err := util.PathExists(DefaultLogtailMountPath); err == nil {
			if !ok {
				logger.Info(context.Background(), "no docker mount path", "set empty")
				DefaultLogtailMountPath = ""
			}
		} else {
			logger.Warning(context.Background(), "check docker mount path error", err.Error())
		}
		var enableCriFinding = criRuntimeWrapper != nil
		var enableDocker = dockerCenterInstance.initClient() == nil
		var enableStatic = isStaticContainerInfoEnabled()
		containerFindingManager = NewContainerDiscoverManager(enableDocker, enableCriFinding, enableStatic)
		containerFindingManager.Init(3)
		containerFindingManager.TimerFetch()
		containerFindingManager.SyncContainers()
	})
	return dockerCenterInstance
}

func SetEnvConfigPrefix(prefix string) {
	envConfigPrefix = prefix
}

func (dc *DockerCenter) readStaticConfig(forceFlush bool) {
	containerInfo, removedIDs, changed, err := tryReadStaticContainerInfo()
	if err != nil {
		logger.Warning(context.Background(), "READ_STATIC_CONFIG_ALARM", "read static container info error", err)
	}
	if !dc.initStaticContainerInfoSuccess && len(containerInfo) > 0 {
		dc.initStaticContainerInfoSuccess = true
		forceFlush = true
	}
	if (forceFlush || len(removedIDs) > 0 || changed) && len(containerInfo) > 0 {
		logger.Info(context.Background(), "read static container info success, count", len(containerInfo), "removed", removedIDs)
		for _, info := range containerInfo {
			dockerInfoDetail := dockerCenterInstance.CreateInfoDetail(info, envConfigPrefix, false)
			dockerCenterInstance.updateContainer(info.ID, dockerInfoDetail)
		}
		dc.mergeK8sInfo()
	}

	if len(removedIDs) > 0 {
		for _, id := range removedIDs {
			dockerCenterInstance.markRemove(id)
		}
	}
}

func (dc *DockerCenter) flushStaticConfig() {
	for {
		dc.readStaticConfig(false)
		time.Sleep(time.Second)
	}
}

func (dc *DockerCenter) setLastError(err error, msg string) {
	dc.lastErrMu.Lock()
	dc.lastErr = err
	dc.lastErrMu.Unlock()
	if err != nil {
		logger.Warning(context.Background(), "DOCKER_CENTER_ALARM", "message", msg, "error found", err)
	} else {
		logger.Debug(context.Background(), "message", msg)
	}
}

func isMapLabelsMatch(includeLabel map[string]string,
	excludeLabel map[string]string,
	includeLabelRegex map[string]*regexp.Regexp,
	excludeLabelRegex map[string]*regexp.Regexp,
	labels map[string]string) bool {
	if len(includeLabel) != 0 || len(includeLabelRegex) != 0 {
		matchedFlag := false
		for key, val := range includeLabel {
			if dockerVal, ok := labels[key]; ok && (len(val) == 0 || dockerVal == val) {
				matchedFlag = true
				break
			}
		}
		// if matched, do not need check regex
		if !matchedFlag {
			for key, reg := range includeLabelRegex {
				if dockerVal, ok := labels[key]; ok && reg.MatchString(dockerVal) {
					matchedFlag = true
					break
				}
			}
		}

		if !matchedFlag {
			return false
		}
	}
	for key, val := range excludeLabel {
		if dockerVal, ok := labels[key]; ok && (len(val) == 0 || dockerVal == val) {
			return false
		}
	}
	for key, reg := range excludeLabelRegex {
		if dockerVal, ok := labels[key]; ok && reg.MatchString(dockerVal) {
			return false
		}
	}
	return true
}

func isContainerLabelMatch(includeLabel map[string]string,
	excludeLabel map[string]string,
	includeLabelRegex map[string]*regexp.Regexp,
	excludeLabelRegex map[string]*regexp.Regexp,
	info *DockerInfoDetail) bool {
	return isMapLabelsMatch(includeLabel, excludeLabel, includeLabelRegex, excludeLabelRegex, info.ContainerInfo.Config.Labels)
}

func isMathEnvItem(env string,
	staticEnv map[string]string,
	regexEnv map[string]*regexp.Regexp) bool {
	var envKey, envValue string
	splitArray := strings.SplitN(env, "=", 2)
	if len(splitArray) < 2 {
		envKey = splitArray[0]
	} else {
		envKey = splitArray[0]
		envValue = splitArray[1]
	}

	if len(staticEnv) > 0 {
		if value, ok := staticEnv[envKey]; ok && (len(value) == 0 || value == envValue) {
			return true
		}
	}

	if len(regexEnv) > 0 {
		if reg, ok := regexEnv[envKey]; ok && reg.MatchString(envValue) {
			return true
		}
	}
	return false
}

func isContainerEnvMatch(includeEnv map[string]string,
	excludeEnv map[string]string,
	includeEnvRegex map[string]*regexp.Regexp,
	excludeEnvRegex map[string]*regexp.Regexp,
	info *DockerInfoDetail) bool {

	if len(includeEnv) != 0 || len(includeEnvRegex) != 0 {
		matchFlag := false
		for _, env := range info.ContainerInfo.Config.Env {
			if isMathEnvItem(env, includeEnv, includeEnvRegex) {
				matchFlag = true
				break
			}
		}
		if !matchFlag {
			return false
		}
	}

	if len(excludeEnv) != 0 || len(excludeEnvRegex) != 0 {
		for _, env := range info.ContainerInfo.Config.Env {
			if isMathEnvItem(env, excludeEnv, excludeEnvRegex) {
				return false
			}
		}
	}

	return true
}

func (dc *DockerCenter) getAllAcceptedInfo(
	includeLabel map[string]string,
	excludeLabel map[string]string,
	includeLabelRegex map[string]*regexp.Regexp,
	excludeLabelRegex map[string]*regexp.Regexp,
	includeEnv map[string]string,
	excludeEnv map[string]string,
	includeEnvRegex map[string]*regexp.Regexp,
	excludeEnvRegex map[string]*regexp.Regexp,
	k8sFilter *K8SFilter,
) map[string]*DockerInfoDetail {
	containerMap := make(map[string]*DockerInfoDetail)
	dc.lock.RLock()
	defer dc.lock.RUnlock()
	for id, info := range dc.containerMap {
		if isContainerLabelMatch(includeLabel, excludeLabel, includeLabelRegex, excludeLabelRegex, info) &&
			isContainerEnvMatch(includeEnv, excludeEnv, includeEnvRegex, excludeEnvRegex, info) &&
			info.K8SInfo.IsMatch(k8sFilter) {
			containerMap[id] = info
		}
	}
	return containerMap
}

func (dc *DockerCenter) getAllAcceptedInfoV2(
	fullList map[string]bool,
	matchList map[string]*DockerInfoDetail,
	includeLabel map[string]string,
	excludeLabel map[string]string,
	includeLabelRegex map[string]*regexp.Regexp,
	excludeLabelRegex map[string]*regexp.Regexp,
	includeEnv map[string]string,
	excludeEnv map[string]string,
	includeEnvRegex map[string]*regexp.Regexp,
	excludeEnvRegex map[string]*regexp.Regexp,
	k8sFilter *K8SFilter,
) (int, int) {
	dc.lock.RLock()
	defer dc.lock.RUnlock()

	// Remove deleted containers from match list and full list.
	delCount := 0
	for id := range fullList {
		if _, exist := dc.containerMap[id]; !exist {
			delete(fullList, id)
			if _, matched := matchList[id]; matched {
				delete(matchList, id)
				delCount++
			}
		}
	}

	// Add new containers to full list and matched to match list.
	newCount := 0
	for id, info := range dc.containerMap {
		if _, exist := fullList[id]; !exist {
			fullList[id] = true
			if isContainerLabelMatch(includeLabel, excludeLabel, includeLabelRegex, excludeLabelRegex, info) &&
				isContainerEnvMatch(includeEnv, excludeEnv, includeEnvRegex, excludeEnvRegex, info) &&
				info.K8SInfo.IsMatch(k8sFilter) {
				newCount++
				matchList[id] = info
			}
		}
	}

	return newCount, delCount
}

func (dc *DockerCenter) getAllSpecificInfo(filter func(*DockerInfoDetail) bool) (infoList []*DockerInfoDetail) {
	dc.lock.RLock()
	defer dc.lock.RUnlock()
	for _, info := range dc.containerMap {
		if filter(info) {
			infoList = append(infoList, info)
		}
	}
	return infoList
}

func (dc *DockerCenter) processAllContainerInfo(processor func(*DockerInfoDetail)) {
	dc.lock.RLock()
	defer dc.lock.RUnlock()
	for _, info := range dc.containerMap {
		processor(info)
	}
}

func (dc *DockerCenter) getContainerDetail(id string) (containerDetail *DockerInfoDetail, ok bool) {
	dc.lock.RLock()
	defer dc.lock.RUnlock()
	containerDetail, ok = dc.containerMap[id]
	return
}

func (dc *DockerCenter) getLastUpdateMapTime() int64 {
	return atomic.LoadInt64(&dc.lastUpdateMapTime)
}

func (dc *DockerCenter) refreshLastUpdateMapTime() {
	atomic.StoreInt64(&dc.lastUpdateMapTime, time.Now().UnixNano())
}

func (dc *DockerCenter) updateContainers(containerMap map[string]*DockerInfoDetail) {
	dc.lock.Lock()
	defer dc.lock.Unlock()
	for key, container := range dc.containerMap {
		// check removed keys
		if _, ok := containerMap[key]; !ok {
			if !container.IsTimeout() {
				// not timeout, put to new map
				containerMap[key] = container
			}
		}
	}
	// switch to new container map
	dc.containerMap = containerMap
	dc.mergeK8sInfo()
	dc.refreshLastUpdateMapTime()
}

func (dc *DockerCenter) mergeK8sInfo() {
	k8sInfoMap := make(map[string][]*K8SInfo)
	for _, container := range dc.containerMap {
		if container.K8SInfo == nil {
			continue
		}
		key := container.K8SInfo.Namespace + "@" + container.K8SInfo.Pod
		k8sInfoMap[key] = append(k8sInfoMap[key], container.K8SInfo)
	}
	for key, k8sInfo := range k8sInfoMap {
		if len(k8sInfo) < 2 {
			logger.Debug(context.Background(), "k8s pod's container count < 2", key)
			continue
		}
		// @note we need test pod with many sidecar containers
		for i := 1; i < len(k8sInfo); i++ {
			k8sInfo[0].Merge(k8sInfo[i])
		}
		for i := 1; i < len(k8sInfo); i++ {
			k8sInfo[i].Merge(k8sInfo[0])
		}
	}
}

func (dc *DockerCenter) updateContainer(id string, container *DockerInfoDetail) {
	dc.lock.Lock()
	defer dc.lock.Unlock()
	if container.K8SInfo != nil {
		if _, ok := dc.containerMap[id]; !ok {
			for _, oldContainer := range dc.containerMap {
				if oldContainer.K8SInfo != nil && oldContainer.K8SInfo.IsSamePod(container.K8SInfo) {
					oldContainer.K8SInfo.Merge(container.K8SInfo)
				}
			}
		}
	}
	if logger.DebugFlag() {
		bytes, _ := json.Marshal(container)
		logger.Debug(context.Background(), "update container info", string(bytes))
	}
	dc.containerMap[id] = container
	dc.refreshLastUpdateMapTime()
}

func (dc *DockerCenter) fetchAll() error {
	containers, err := dc.client.ListContainers(docker.ListContainersOptions{})
	if err != nil {
		dc.setLastError(err, "list container error")
		return err
	}
	logger.Debug(context.Background(), "fetch all", containers)
	var containerMap = make(map[string]*DockerInfoDetail)
	hasInspectErr := false

	for _, container := range containers {
		var containerDetail *docker.Container
		for idx := 0; idx < 3; idx++ {
			if containerDetail, err = dc.client.InspectContainerWithOptions(docker.InspectContainerOptions{ID: container.ID}); err == nil {
				break
			}
			time.Sleep(time.Second * 5)
		}
		if err == nil {
			if time.Since(containerDetail.Created) < DefaultSyncContainersPeriod {
				containerMap[container.ID] = dc.CreateInfoDetail(containerDetail, envConfigPrefix, false)
				continue
			}
			exist := ContainerProcessAlive(containerDetail.State.Pid)
			if exist {
				containerMap[container.ID] = dc.CreateInfoDetail(containerDetail, envConfigPrefix, false)
				continue
			} else {
				logger.Debug(context.Background(), "find container", containerDetail.ID, "pid", containerDetail.State.Pid, "was already stopped")
			}
		} else {
			dc.setLastError(err, "inspect container error "+container.ID)
			hasInspectErr = true
		}
	}
	dc.updateContainers(containerMap)
	if !hasInspectErr {
		// Set at last to make sure update time in detail is less or equal to it.
		dc.lastFetchAllSuccessTime = time.Now()
	}
	return err
}

func (dc *DockerCenter) fetchOne(containerID string, tryFindSandbox bool) error {
	containerDetail, err := dc.client.InspectContainerWithOptions(docker.InspectContainerOptions{ID: containerID})
	if err != nil {
		dc.setLastError(err, "inspect container error "+containerID)
	} else {
		dc.updateContainer(containerID, dc.CreateInfoDetail(containerDetail, envConfigPrefix, false))
	}
	logger.Debug(context.Background(), "update container", containerID, "detail", containerDetail)
	if tryFindSandbox && containerDetail.Config != nil {
		if id := containerDetail.Config.Labels["io.kubernetes.sandbox.id"]; id != "" {
			containerDetail, err = dc.client.InspectContainerWithOptions(docker.InspectContainerOptions{ID: id})
			if err != nil {
				dc.setLastError(err, "inspect sandbox container error "+id)
			} else {
				dc.updateContainer(id, dc.CreateInfoDetail(containerDetail, envConfigPrefix, false))
				logger.Debug(context.Background(), "update sandbox container", id, "detail", containerDetail)
			}
		}
	}
	return err
}

func (dc *DockerCenter) markRemove(containerID string) {
	dc.lock.Lock()
	defer dc.lock.Unlock()
	if container, ok := dc.containerMap[containerID]; ok {
		container.deleteFlag = true
		container.lastUpdateTime = time.Now()
	}
}

func (dc *DockerCenter) cleanTimeoutContainer() {
	dc.lock.Lock()
	defer dc.lock.Unlock()
	hasDelete := false
	isFetchAllSuccessTimeout := time.Since(dc.lastFetchAllSuccessTime) > fetchAllSuccessTimeout
	for key, container := range dc.containerMap {
		// Comfirm to delete:
		// 1. If the container's update time is newer than the time of last success fetch all.
		// 2. The time of last success fetch all is too old.
		if container.IsTimeout() &&
			(isFetchAllSuccessTimeout ||
				(container.lastUpdateTime.Sub(dc.lastFetchAllSuccessTime) > time.Second)) {
			delete(dc.containerMap, key)
			logger.Debug(context.Background(), "delete container", key)
			hasDelete = true
		}
	}
	if hasDelete {
		dc.refreshLastUpdateMapTime()
	}
}

func (dc *DockerCenter) sweepCache() {
	// clear unuseful cache
	usedImageIDSet := make(map[string]bool)
	dc.lock.RLock()
	for _, container := range dc.containerMap {
		usedImageIDSet[container.ContainerInfo.Image] = true
	}
	dc.lock.RUnlock()

	dc.imageLock.Lock()

	for key := range dc.imageCache {
		if _, ok := usedImageIDSet[key]; !ok {
			delete(dc.imageCache, key)
		}
	}
	dc.imageLock.Unlock()
}

func dockerCenterRecover() {
	if err := recover(); err != nil {
		trace := make([]byte, 2048)
		runtime.Stack(trace, true)
		logger.Error(context.Background(), "PLUGIN_RUNTIME_ALARM", "docker center runtime error", err, "stack", string(trace))
	}
}

func (dc *DockerCenter) initClient() error {
	var err error
	if dc.client, err = docker.NewClientFromEnv(); err != nil {
		dc.setLastError(err, "init docker client from env error")
		return err
	}
	dc.client.SetTimeout(DockerCenterTimeout)
	return nil
}

func (dc *DockerCenter) eventListener() {
	errorCount := 0
	defer dockerCenterRecover()
	var err error
	for {
		logger.Info(context.Background(), "docker event listener", "start")
		events := make(chan *docker.APIEvents)
		err = dc.client.AddEventListener(events)
		if err != nil {
			break
		}
		timer := time.NewTimer(EventListenerTimeout)
	ForBlock:
		for {
			select {
			case event, ok := <-events:
				if !ok {
					logger.Info(context.Background(), "docker event listener", "stop")
					errorCount++
					break ForBlock
				}
				errorCount = 0
				switch event.Status {
				case "start", "restart":
					_ = dc.fetchOne(event.ID, false)
				case "rename":
					_ = dc.fetchOne(event.ID, false)
				case "die":
					dc.markRemove(event.ID)
				default:
				}
				logger.Debug(context.Background(), "event", *event)
				timer.Reset(EventListenerTimeout)
				dc.eventChanLock.Lock()
				if dc.eventChan != nil {
					// no block insert
					select {
					case dc.eventChan <- event:
					default:
						logger.Error(context.Background(), "DOCKER_EVENT_ALARM", "event queue is full, this event", *event)
					}
				}
				dc.eventChanLock.Unlock()
			case <-timer.C:
				errorCount = 0
				logger.Info(context.Background(), "no docker event in 1 hour", "remove and add event listener again")
				break ForBlock
			}
		}
		if err = dc.client.RemoveEventListener(events); err != nil {
			dc.setLastError(err, "remove event listener error")
		}
		if errorCount > 10 && criRuntimeWrapper != nil {
			logger.Info(context.Background(), "docker listen is error and cri runtime wrapper is valid", "skip docker listen")
			return
		}
		// if always error, sleep 300 secs
		if errorCount > 30 {
			time.Sleep(time.Duration(300) * time.Second)
		} else {
			time.Sleep(time.Duration(10) * time.Second)
		}
	}
	dc.setLastError(err, "docker event stream closed")
}
