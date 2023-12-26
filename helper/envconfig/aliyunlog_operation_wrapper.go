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

package envconfig

import (
	"context"
	"errors"
	"fmt"

	"github.com/alibabacloud-go/tea/tea"

	"strings"
	"sync"
	"time"

	aliyunlog "github.com/alibabacloud-go/sls-20201230/v5/client"

	"github.com/alibaba/ilogtail/pkg/config"
	"github.com/alibaba/ilogtail/pkg/flags"
	k8s_event "github.com/alibaba/ilogtail/pkg/helper/eventrecorder"
	"github.com/alibaba/ilogtail/pkg/logger"
	"github.com/alibaba/ilogtail/pkg/util"
)

// const MachineIDTypes
const (
	MachineIDTypeIP          = "ip"
	MachineIDTypeUserDefined = "userdefined"
)

type operationWrapper struct {
	logClient        *AliyunLogClient     // 日志客户端接口
	lock             sync.RWMutex         // 读写锁
	project          string               // project名称
	logstoreCacheMap map[string]time.Time // logStore缓存映射
	eventRecorder    *k8s_event.EventRecorder
}

// createDefaultK8SIndex 创建默认的K8S索引
// logstoreMode 用于判断是否开启字段统计。
func createDefaultK8SIndex(logstoreMode string) *aliyunlog.CreateIndexRequest {
	docValue := logstoreMode == StandardMode
	normalIndexKey := aliyunlog.KeysValue{
		CaseSensitive: tea.Bool(false),    // 是否大小写敏感
		Type:          tea.String("text"), // 类型为文本
		// 定义分词符
		Token:    tea.StringSlice([]string{" ", "\n", "\t", "\r", ",", ";", "[", "]", "{", "}", "(", ")", "&", "^", "*", "#", "@", "~", "=", "<", ">", "/", "\\", "?", ":", "'", "\""}),
		DocValue: &docValue, // 是否开启字段统计
	}
	return &aliyunlog.CreateIndexRequest{
		Line: &aliyunlog.CreateIndexRequestLine{ // 索引行
			CaseSensitive: tea.Bool(false),
			Token:         tea.StringSlice([]string{" ", "\n", "\t", "\r", ",", ";", "[", "]", "{", "}", "(", ")", "&", "^", "*", "#", "@", "~", "=", "<", ">", "/", "\\", "?", ":", "'", "\""}),
		},
		// 索引键
		Keys: map[string]*aliyunlog.KeysValue{
			"__tag__:__hostname__":     &normalIndexKey,
			"__tag__:__path__":         &normalIndexKey,
			"__tag__:_container_ip_":   &normalIndexKey,
			"__tag__:_container_name_": &normalIndexKey,
			"__tag__:_image_name_":     &normalIndexKey,
			"__tag__:_namespace_":      &normalIndexKey,
			"__tag__:_pod_name_":       &normalIndexKey,
			"__tag__:_pod_uid_":        &normalIndexKey,
			"_container_ip_":           &normalIndexKey,
			"_container_name_":         &normalIndexKey,
			"_image_name_":             &normalIndexKey,
			"_namespace_":              &normalIndexKey,
			"_pod_name_":               &normalIndexKey,
			"_pod_uid_":                &normalIndexKey,
			"_source_":                 &normalIndexKey,
		},
	}
}

func createClientInterface(endpoint, accessKeyID, accessKeySecret, stsToken string, shutdown <-chan struct{}) (*AliyunLogClient, error) {
	var logClient *AliyunLogClient
	var err error
	if *flags.AliCloudECSFlag {
		// 如果是阿里云ECS, 创建一个自动更新令牌的客户端
		logClient, err = CreateTokenAutoUpdateAliyunLogClient(endpoint, UpdateTokenFunction, shutdown, config.UserAgent)
		if err != nil {
			return nil, err
		}
	} else {
		// 如果不是阿里云ECS, 创建一个普通的客户端接口
		logClient, err = CreateNormalAliyunLogClient(endpoint, accessKeyID, accessKeySecret, stsToken, config.UserAgent)
		if err != nil {
			return nil, err
		}
	}
	return logClient, err
}

// createAliyunLogOperationWrapper 保证project和机器组存在, 并初始化logStore缓存映射
func createAliyunLogOperationWrapper(project string, logClient *AliyunLogClient) (*operationWrapper, error) {
	var err error
	wrapper := &operationWrapper{
		logClient:     logClient,
		project:       project,
		eventRecorder: k8s_event.GetEventRecorder(),
	}
	logger.Info(context.Background(), "init aliyun log operation wrapper", "begin")
	// 确保project存在
	for i := 0; i < 1; i++ {
		err = wrapper.makesureProjectExist(nil, project)
		if err == nil {
			break
		}
		logger.Warning(context.Background(), "CREATE_PROJECT_ALARM", "create project error, project", project, "error", err)
		time.Sleep(time.Second * time.Duration(30))
	}

	// 确保机器组存在
	for i := 0; i < 3; i++ {
		err = wrapper.makesureMachineGroupExist(project, *flags.DefaultLogMachineGroup)
		if err == nil {
			break
		}
		logger.Warning(context.Background(), "CREATE_MACHINEGROUP_ALARM", "create machine group error, project", project, "error", err)
		time.Sleep(time.Second * time.Duration(30))
	}
	if err != nil {
		return nil, err
	}
	// 创建logStore缓存映射
	wrapper.logstoreCacheMap = make(map[string]time.Time)
	logger.Info(context.Background(), "init aliyun log operation wrapper", "done")
	return wrapper, nil
}

// logstoreCacheExists 检查给定的project和logStore是否在缓存中存在
// 并且其最后更新时间距离现在的时间小于缓存过期时间。
func (o *operationWrapper) logstoreCacheExists(project, logstore string) bool {
	o.lock.RLock()
	defer o.lock.RUnlock()
	// 检查project和logStore是否在缓存中
	if lastUpdateTime, ok := o.logstoreCacheMap[project+"@@"+logstore]; ok {
		// 检查最后更新时间是否在缓存过期时间内
		if time.Since(lastUpdateTime) < time.Second*time.Duration(*flags.LogResourceCacheExpireSec) {
			return true
		}
	}
	return false
}

// addLogstoreCache 将给定的project和logStore添加到缓存中。
func (o *operationWrapper) addLogstoreCache(project, logstore string) {
	o.lock.Lock()
	defer o.lock.Unlock()
	// 将project和logStore添加到缓存中
	o.logstoreCacheMap[project+"@@"+logstore] = time.Now()
}

// createProductLogstore 创建logStore
func (o *operationWrapper) createProductLogstore(config *AliyunLogConfigSpec, project, logstore, product, lang string, hotTTL int) error {
	logger.Info(context.Background(), "begin to create product logstore, project", project, "logstore", logstore, "product", product, "lang", lang)
	// 调用 CreateProductLogstore 函数创建logStore
	err := CreateProductLogstore(*flags.DefaultRegion, project, logstore, product, lang, hotTTL)

	annotations := GetAnnotationByObject(config, project, logstore, product, tea.StringValue(config.LogtailConfig.ConfigName), false)

	if err != nil {
		customErr := CustomErrorFromPopError(err)
		o.eventRecorder.SendErrorEventWithAnnotation(o.eventRecorder.GetObject(), GetAnnotationByError(annotations, customErr), k8s_event.CreateProductLogStore, "", fmt.Sprintf("create product log failed, error: %s", err.Error()))
		logger.Warning(context.Background(), "CREATE_PRODUCT_ALARM", "create product error, error", err)
		return err
	}

	o.eventRecorder.SendNormalEventWithAnnotation(o.eventRecorder.GetObject(), annotations, k8s_event.CreateProductLogStore, "create product log success")

	// 将logStore添加到缓存中
	o.addLogstoreCache(project, logstore)
	return nil
}

// makesureLogstoreExist 确保logStore存在, 如果不存在就创建logstore
func (o *operationWrapper) makesureLogstoreExist(config *AliyunLogConfigSpec) error {
	project := o.project
	if len(config.Project) != 0 {
		project = config.Project
	}
	logstore := config.Logstore
	shardCount := 0
	if config.ShardCount != nil {
		shardCount = int(*config.ShardCount)
	}
	lifeCycle := 0
	if config.LifeCycle != nil {
		lifeCycle = int(*config.LifeCycle)
	}
	mode := StandardMode
	if len(config.LogstoreMode) != 0 {
		if config.LogstoreMode == QueryMode {
			mode = QueryMode
		}
	}

	product := config.ProductCode
	lang := config.ProductLang
	hotTTL := 0
	if config.LogstoreHotTTL != nil {
		hotTTL = int(*config.LogstoreHotTTL)
	}

	// 如果logStore已经存在于缓存中,则返回 nil
	if o.logstoreCacheExists(project, logstore) {
		return nil
	}

	if len(product) > 0 {
		if len(lang) == 0 {
			lang = "cn"
		}
		return o.createProductLogstore(config, project, logstore, product, lang, hotTTL)
	}

	// 如果logStore名称以 "audit-" 开头并且长度为 39, 例如audit-cfc281c9c4ca548638a1aaa765d8f220d
	if strings.HasPrefix(logstore, "audit-") && len(logstore) == 39 {
		return o.createProductLogstore(config, project, logstore, "k8s-audit", "cn", 0)
	}

	// 如果project名称不等于 o.project, 确保project存在
	if project != o.project {
		if err := o.makesureProjectExist(config, project); err != nil {
			return err
		}
	}
	var ok *aliyunlog.GetLogStoreResponse
	var err error
	// 检查logStore是否存在
	for i := 0; i < *flags.LogOperationMaxRetryTimes; i++ {
		if ok, err = o.logClient.GetClient().GetLogStore(&project, &logstore); err != nil {
			time.Sleep(time.Millisecond * 100)
			var clientError *tea.SDKError
			if errors.As(err, &clientError) && tea.StringValue(clientError.Code) == "LogStoreNotExist" {
				break
			}
		} else {
			break
		}
	}
	// 如果logStore已经存在
	if ok != nil && ok.Body != nil && err == nil {
		logger.Info(context.Background(), "logstore already exist, logstore", logstore)
		// 将logStore添加到缓存中
		o.addLogstoreCache(project, logstore)
		return nil
	}
	ttl := 90
	if lifeCycle > 0 {
		ttl = lifeCycle
	}
	if shardCount <= 0 {
		shardCount = 2
	}
	// @note max init shard count limit : 10
	if shardCount > 10 {
		shardCount = 10
	}
	logStore := &aliyunlog.CreateLogStoreRequest{
		LogstoreName:  tea.String(logstore),         // logStore的名称
		Ttl:           tea.Int32(int32(ttl)),        // logStore的生命周期
		ShardCount:    tea.Int32(int32(shardCount)), // Shard 分区个数
		AutoSplit:     tea.Bool(true),               // 是否自动分片
		MaxSplitShard: tea.Int32(32),                // 最大分片数量
		Mode:          tea.String(mode),             // 模式
	}
	if config.LogstoreHotTTL != nil {
		logStore.HotTtl = tea.Int32(*config.LogstoreHotTTL)
	}

	if len(config.LogstoreTelemetryType) > 0 {
		if MetricsTelemetryType == config.LogstoreTelemetryType {
			logStore.TelemetryType = tea.String(config.LogstoreTelemetryType)
		}
	}
	if config.LogstoreAppendMeta {
		logStore.AppendMeta = tea.Bool(config.LogstoreAppendMeta)
	}
	if config.LogstoreEnableTracking {
		logStore.EnableTracking = tea.Bool(config.LogstoreEnableTracking)
	}
	if config.LogstoreAutoSplit {
		logStore.AutoSplit = tea.Bool(config.LogstoreAutoSplit)
	}
	if config.LogstoreMaxSplitShard != nil {
		logStore.MaxSplitShard = tea.Int32(*config.LogstoreMaxSplitShard)
	}
	if tea.BoolValue(config.LogstoreEncryptConf.Enable) {
		logStore.EncryptConf = &config.LogstoreEncryptConf
	}
	// 创建logStore
	for i := 0; i < *flags.LogOperationMaxRetryTimes; i++ {
		_, err = o.logClient.GetClient().CreateLogStore(&project, logStore)
		if err != nil {
			time.Sleep(time.Millisecond * 100)
		} else {
			// 将logStore添加到缓存中
			o.addLogstoreCache(project, logstore)
			logger.Info(context.Background(), "create logstore success, logstore", logstore)
			break
		}
	}
	annotations := GetAnnotationByObject(config, project, logstore, "", tea.StringValue(config.LogtailConfig.ConfigName), false)
	if err != nil {
		customErr := CustomErrorFromSlsSDKError(err)
		o.eventRecorder.SendErrorEventWithAnnotation(o.eventRecorder.GetObject(), GetAnnotationByError(annotations, customErr), k8s_event.CreateLogstore, "", fmt.Sprintf("create logstore failed, error: %s", err.Error()))
		return err
	}

	o.eventRecorder.SendNormalEventWithAnnotation(o.eventRecorder.GetObject(), annotations, k8s_event.CreateLogstore, "create logstore success")
	// 创建logStore成功后,等待 1 秒
	time.Sleep(time.Second)
	// 使用默认的 k8s 索引
	index := createDefaultK8SIndex(mode)
	// 创建索引
	for i := 0; i < *flags.LogOperationMaxRetryTimes; i++ {
		_, err = o.logClient.GetClient().CreateIndex(&project, &logstore, index)
		if err != nil {
			time.Sleep(time.Millisecond * 100)
		} else {
			break
		}
	}
	// create index always return nil
	if err == nil {
		logger.Info(context.Background(), "create index done, logstore", logstore)
	} else {
		logger.Warning(context.Background(), "CREATE_INDEX_ALARM", "create index done, logstore", logstore, "error", err)
	}
	return nil
}

// makesureProjectExist 函数确保在阿里云日志服务中存在指定的project。
// 如果project不存在,该函数将尝试创建它。
func (o *operationWrapper) makesureProjectExist(config *AliyunLogConfigSpec, project string) error {
	var ok *aliyunlog.GetProjectResponse
	var err error

	// 检查project是否存在
	for i := 0; i < *flags.LogOperationMaxRetryTimes; i++ {
		if ok, err = o.logClient.GetClient().GetProject(&project); err != nil {
			var clientError *tea.SDKError
			if errors.As(err, &clientError) {
				if tea.StringValue(clientError.Code) == "ProjectNotExist" {
					ok = nil
					break
				} else if tea.StringValue(clientError.Code) == "Unauthorized" {
					// The project does not belong to you.
					return fmt.Errorf("GetProject error, project : %s, Endpoint : %s, error : %s", project, *o.logClient.GetClient().Endpoint, tea.StringValue(clientError.Message))
				}
			}
			time.Sleep(time.Millisecond * 1000)
		} else {
			break
		}
	}
	// 如果project存在,返回 nil
	if ok != nil && ok.Body != nil && err == nil {
		return nil
	}
	// 如果project不存在, 创建project
	createProjectRequest := aliyunlog.CreateProjectRequest{
		Description: tea.String("k8s log project, created by alibaba ilogtail"),
		ProjectName: tea.String(project),
	}
	for i := 0; i < *flags.LogOperationMaxRetryTimes; i++ {
		_, err = o.logClient.GetClient().CreateProject(&createProjectRequest)
		if err != nil {
			var clientError *tea.SDKError
			if errors.As(err, &clientError) && tea.StringValue(clientError.Code) == "ProjectAlreadyExist" {
				err = nil
				break
			}
			time.Sleep(time.Millisecond * 1000)
		} else {
			break
		}
	}
	configName := ""
	logstore := ""
	if config != nil {
		configName = tea.StringValue(config.LogtailConfig.ConfigName)
		logstore = config.Logstore
	}
	annotations := GetAnnotationByObject(config, project, logstore, "", configName, false)
	if err != nil {
		customErr := CustomErrorFromSlsSDKError(err)
		o.eventRecorder.SendErrorEventWithAnnotation(o.eventRecorder.GetObject(), GetAnnotationByError(annotations, customErr), k8s_event.CreateProject, "", fmt.Sprintf("create project failed, error: %s", err.Error()))
	} else {
		o.eventRecorder.SendNormalEventWithAnnotation(o.eventRecorder.GetObject(), annotations, k8s_event.CreateProject, "create project success")
	}
	return err
}

// makesureMachineGroupExist 函数确保在阿里云日志服务中存在指定的机器组。
// 如果机器组不存在,该函数将尝试创建它。
// project 是project的名称,machineGroup 是要检查或创建的机器组的名称。
func (o *operationWrapper) makesureMachineGroupExist(project, machineGroup string) error {
	var ok *aliyunlog.GetMachineGroupResponse
	var err error

	// 检查机器组是否存在
	for i := 0; i < *flags.LogOperationMaxRetryTimes; i++ {
		if ok, err = o.logClient.GetClient().GetMachineGroup(&project, &machineGroup); err != nil {
			var clientError *tea.SDKError
			if errors.As(err, &clientError) {
				if tea.StringValue(clientError.Code) == "MachineGroupNotExist" {
					ok, err = nil, nil
					break
				} else if tea.StringValue(clientError.Code) == "MachineGroupAlreadyExist" {
					return nil
				}
			}
			time.Sleep(time.Millisecond * 100)
		} else {
			break
		}
	}
	if ok != nil && ok.Body != nil && err == nil {
		return nil
	}
	if err != nil {
		return fmt.Errorf("GetMachineGroup error, project : %s, machineGroup : %s, Endpoint : %s, error : %s", project, machineGroup, *o.logClient.GetClient().Endpoint, err.Error())
	}

	// 如果机器组不存在,创建一个新的机器组
	createMachineGroupRequest := aliyunlog.CreateMachineGroupRequest{
		GroupAttribute:      nil,
		GroupName:           tea.String(machineGroup),
		GroupType:           tea.String(""),
		MachineIdentifyType: tea.String(MachineIDTypeUserDefined),
		MachineList:         tea.StringSlice([]string{machineGroup}),
	}
	// 创建机器组
	for i := 0; i < *flags.LogOperationMaxRetryTimes; i++ {
		_, err = o.logClient.GetClient().CreateMachineGroup(&project, &createMachineGroupRequest)
		if err != nil {
			time.Sleep(time.Millisecond * 100)
		} else {
			// 给机器组加tag
			for j := 0; j < *flags.LogOperationMaxRetryTimes; j++ {
				err = o.TagMachineGroup(project, machineGroup, SlsMachinegroupDeployModeKey, SlsMachinegroupDeployModeDeamonset)
				if err != nil {
					time.Sleep(time.Millisecond * 100)
				} else {
					break
				}
			}
			break
		}
	}

	return err
}

// updateConfig 更新配置
func (o *operationWrapper) updateConfig(config *AliyunLogConfigSpec) error {
	return o.updateConfigInner(config)
}

// checkFileConfigChanged 函数用于检查文件配置是否发生了变化
func checkFileConfigChanged(filePaths, includeEnv, includeLabel string, serverInput map[string]interface{}) bool {
	var ok bool

	// 判断FilePaths是否存在, 且长度是否为0
	if _, ok = serverInput["FilePaths"]; !ok {
		return true
	}
	if len(serverInput["FilePaths"].([]interface{})) == 0 {
		return true
	}
	serverFilePath, _ := util.InterfaceToString(serverInput["FilePaths"].([]interface{})[0])
	// 把/**/ 替换为/ 后再进行比较
	for strings.Contains(serverFilePath, "/**/") {
		serverFilePath = strings.ReplaceAll(serverFilePath, "/**/", "/")
		serverFilePath = strings.ReplaceAll(serverFilePath, "//", "/")
	}
	for strings.Contains(filePaths, "/**/") {
		filePaths = strings.ReplaceAll(filePaths, "/**/", "/")
		filePaths = strings.ReplaceAll(filePaths, "//", "/")
	}
	if serverFilePath != filePaths {
		return true
	}

	// 判断ContainerFilters是否存在
	if _, ok = serverInput["ContainerFilters"]; !ok {
		return true
	}
	var containerFilters map[string]interface{}
	// 判断ContainerFilters类型是否正确
	if containerFilters, ok = serverInput["ContainerFilters"].(map[string]interface{}); !ok {
		return true
	}

	serverIncludeEnv, _ := util.InterfaceToJSONString(containerFilters["IncludeEnv"])
	if includeEnv != serverIncludeEnv {
		return true
	}

	serverIncludeLabel, _ := util.InterfaceToJSONString(containerFilters["IncludeContainerLabel"])

	return includeLabel != serverIncludeLabel
}

// UnTagLogtailConfig 移除 Logtail 配置所有的标签
func (o *operationWrapper) UnTagLogtailConfig(project string, logtailConfig string) error {
	var err error

	untagResourcesRequest := aliyunlog.UntagResourcesRequest{
		All:          tea.Bool(true),
		ResourceId:   tea.StringSlice([]string{project + "#" + logtailConfig}),
		ResourceType: tea.String(TagLogtailConfig),
	}
	// 移除标签
	for i := 0; i < *flags.LogOperationMaxRetryTimes; i++ {
		_, err = o.logClient.GetClient().UntagResources(&untagResourcesRequest)
		if err == nil {
			return nil
		}
		time.Sleep(time.Millisecond * 100)
	}
	return err
}

// TagLogtailConfig 给 Logtail 配置添加标签
func (o *operationWrapper) TagLogtailConfig(project string, logtailConfig string, tags map[string]string) error {
	var err error

	// 在创建之前删除所有标签
	err = o.UnTagLogtailConfig(project, logtailConfig)
	if err != nil {
		return err
	}

	tagResourcesRequest := aliyunlog.TagResourcesRequest{ResourceType: tea.String(TagLogtailConfig),
		ResourceId: tea.StringSlice([]string{project + "#" + logtailConfig}),
		Tags:       []*aliyunlog.TagResourcesRequestTags{},
	}
	// 遍历所有标签
	for k, v := range tags {
		tag := aliyunlog.TagResourcesRequestTags{Key: tea.String(k), Value: tea.String(v)}
		tagResourcesRequest.Tags = append(tagResourcesRequest.Tags, &tag)
	}
	// 添加标签
	for i := 0; i < *flags.LogOperationMaxRetryTimes; i++ {
		_, err = o.logClient.GetClient().TagResources(&tagResourcesRequest)
		if err == nil {
			return nil
		}
		time.Sleep(time.Millisecond * 100)
	}
	return err
}

// TagMachineGroup 给机器组添加标签
func (o *operationWrapper) TagMachineGroup(project, machineGroup, tagKey, tagValue string) error {
	tagResourcesRequest := aliyunlog.TagResourcesRequest{
		ResourceType: tea.String(TagMachinegroup),
		ResourceId:   tea.StringSlice([]string{project + "#" + machineGroup}),
		Tags:         []*aliyunlog.TagResourcesRequestTags{{Key: tea.String(tagKey), Value: tea.String(tagValue)}},
	}
	var err error
	// 添加标签
	for i := 0; i < *flags.LogOperationMaxRetryTimes; i++ {
		_, err = o.logClient.GetClient().TagResources(&tagResourcesRequest)
		if err == nil {
			return nil
		}
		time.Sleep(time.Millisecond * 100)
	}
	return err
}

// nolint:govet,ineffassign,gocritic
// updateConfigInner 更新配置
func (o *operationWrapper) updateConfigInner(config *AliyunLogConfigSpec) error {
	project := o.project
	if len(config.Project) != 0 {
		project = config.Project
	}
	logstore := config.Logstore
	// 确保logStore存在
	err := o.makesureLogstoreExist(config)
	if err != nil {
		return fmt.Errorf("create logconfig error when update config, config : %s, error : %s", tea.StringValue(config.LogtailConfig.ConfigName), err.Error())
	}
	logger.Info(context.Background(), "create or update config", tea.StringValue(config.LogtailConfig.ConfigName))

	ok := false

	// 获取服务端配置
	var serverConfig *aliyunlog.LogtailPipelineConfig
	var getLogtailPipelineConfigResponse *aliyunlog.GetLogtailPipelineConfigResponse
	for i := 0; i < *flags.LogOperationMaxRetryTimes; i++ {
		getLogtailPipelineConfigResponse, err = o.logClient.GetClient().GetLogtailPipelineConfig(&project, config.LogtailConfig.ConfigName)
		if err != nil {
			var slsErr *tea.SDKError
			if errors.As(err, &slsErr) && tea.StringValue(slsErr.Code) == "ConfigNotExist" {
				ok = false
				break
			}
		} else {
			ok = true
			serverConfig = getLogtailPipelineConfigResponse.Body
			break
		}
	}

	logger.Info(context.Background(), "get config", tea.StringValue(config.LogtailConfig.ConfigName), "result", ok)

	// 如果服务端配置存在
	if ok && serverConfig != nil {
		// 如果配置为简单配置
		if config.SimpleConfig {
			needUpdate := false
			// 服务端配置的inputs为空时, 强制更新服务端config
			if serverConfig.Inputs == nil || len(serverConfig.Inputs) == 0 {
				updateLogtailPipelineConfigRequest := aliyunlog.UpdateLogtailPipelineConfigRequest{
					Aggregators: config.LogtailConfig.Aggregators,
					ConfigName:  config.LogtailConfig.ConfigName,
					Flushers: []map[string]interface{}{
						{
							"Logstore": logstore,
							"Type":     "flusher_sls",
						},
					},
					Global:     config.LogtailConfig.Global,
					Inputs:     config.LogtailConfig.Inputs,
					LogSample:  config.LogtailConfig.LogSample,
					Processors: config.LogtailConfig.Processors,
				}
				for i := 0; i < *flags.LogOperationMaxRetryTimes; i++ {
					_, err = o.logClient.GetClient().UpdateLogtailPipelineConfig(&project, config.LogtailConfig.ConfigName, &updateLogtailPipelineConfigRequest)
					if err == nil {
						needUpdate = true
						break
					}
				}
				annotations := GetAnnotationByObject(config, project, logstore, "", tea.StringValue(config.LogtailConfig.ConfigName), true)
				if err != nil {
					customErr := CustomErrorFromSlsSDKError(err)
					o.eventRecorder.SendErrorEventWithAnnotation(o.eventRecorder.GetObject(), GetAnnotationByError(annotations, customErr), k8s_event.UpdateConfig, "", fmt.Sprintf("update config failed, error: %s", err.Error()))
				} else {
					o.eventRecorder.SendNormalEventWithAnnotation(o.eventRecorder.GetObject(), annotations, k8s_event.UpdateConfig, "update config success")
				}
			} else if config.LogtailConfig.Inputs[0]["Type"].(string) == "input_file" && serverConfig.Inputs[0]["Type"].(string) == "input_file" {
				// 如果配置的输入类型为"input_file",并且serverConfig的输入类型也为"input_file"
				// 则检查env配置的FilePaths、includeEnv、includeLabel是否和服务端配置相同, 如果不同则更新服务端配置

				// 获取FilePaths、includeEnv、includeLabel的值
				filePaths, _ := util.InterfaceToString(config.LogtailConfig.Inputs[0]["FilePaths"].([]string)[0])
				includeEnv, _ := util.InterfaceToJSONString(config.LogtailConfig.Inputs[0]["ContainerFilters"].(map[string]map[string]interface{})["IncludeEnv"])
				includeLabel, _ := util.InterfaceToJSONString(config.LogtailConfig.Inputs[0]["ContainerFilters"].(map[string]map[string]interface{})["IncludeContainerLabel"])

				if len(filePaths) > 0 {
					if checkFileConfigChanged(filePaths, includeEnv, includeLabel, serverConfig.Inputs[0]) {
						updateLogtailPipelineConfigRequest := aliyunlog.UpdateLogtailPipelineConfigRequest{
							Aggregators: serverConfig.Aggregators,
							ConfigName:  serverConfig.ConfigName,
							Flushers:    serverConfig.Flushers,
							Global:      serverConfig.Global,
							Inputs:      config.LogtailConfig.Inputs,
							LogSample:   serverConfig.LogSample,
							Processors:  serverConfig.Processors,
						}
						for i := 0; i < *flags.LogOperationMaxRetryTimes; i++ {
							_, err = o.logClient.GetClient().UpdateLogtailPipelineConfig(&project, config.LogtailConfig.ConfigName, &updateLogtailPipelineConfigRequest)
							if err == nil {
								needUpdate = true
								break
							}
						}

						annotations := GetAnnotationByObject(config, project, logstore, "", tea.StringValue(config.LogtailConfig.ConfigName), true)
						if err != nil {
							customErr := CustomErrorFromSlsSDKError(err)
							o.eventRecorder.SendErrorEventWithAnnotation(o.eventRecorder.GetObject(), GetAnnotationByError(annotations, customErr), k8s_event.UpdateConfig, "", fmt.Sprintf("update config failed, error: %s", err.Error()))
						} else {
							o.eventRecorder.SendNormalEventWithAnnotation(o.eventRecorder.GetObject(), annotations, k8s_event.UpdateConfig, "update config success")
						}

					}
				}
			} else if config.LogtailConfig.Inputs[0]["Type"].(string) != serverConfig.Inputs[0]["Type"].(string) && ok {
				// 如果配置的输入类型和serverConfig的输入类型不一致, 强制更新
				logger.Info(context.Background(), "config input type change from", serverConfig.Inputs[0]["Type"].(string),
					"to", config.LogtailConfig.Inputs[0]["Type"].(string), "force update")
				updateLogtailPipelineConfigRequest := aliyunlog.UpdateLogtailPipelineConfigRequest{
					Aggregators: config.LogtailConfig.Aggregators,
					ConfigName:  config.LogtailConfig.ConfigName,
					Flushers: []map[string]interface{}{
						{
							"Logstore": logstore,
							"Type":     "flusher_sls",
						},
					},
					Global:     config.LogtailConfig.Global,
					Inputs:     config.LogtailConfig.Inputs,
					LogSample:  config.LogtailConfig.LogSample,
					Processors: config.LogtailConfig.Processors,
				}
				for i := 0; i < *flags.LogOperationMaxRetryTimes; i++ {
					_, err = o.logClient.GetClient().UpdateLogtailPipelineConfig(&project, config.LogtailConfig.ConfigName, &updateLogtailPipelineConfigRequest)
					if err == nil {
						needUpdate = true
						break
					}
				}
				annotations := GetAnnotationByObject(config, project, logstore, "", tea.StringValue(config.LogtailConfig.ConfigName), true)
				if err != nil {
					customErr := CustomErrorFromSlsSDKError(err)
					o.eventRecorder.SendErrorEventWithAnnotation(o.eventRecorder.GetObject(), GetAnnotationByError(annotations, customErr), k8s_event.UpdateConfig, "", fmt.Sprintf("update config failed, error: %s", err.Error()))
				} else {
					o.eventRecorder.SendNormalEventWithAnnotation(o.eventRecorder.GetObject(), annotations, k8s_event.UpdateConfig, "update config success")
				}
			}
			if needUpdate {
				logger.Info(context.Background(), "config updated, server config", *serverConfig, "local config", *config)
			} else {
				logger.Debug(context.Background(), "config not changed", "skip update")
			}
		}

	} else {
		// 如果配置不存在, 创建新的配置
		createLogtailPipelineConfigRequest := aliyunlog.CreateLogtailPipelineConfigRequest{
			Aggregators: config.LogtailConfig.Aggregators,
			ConfigName:  config.LogtailConfig.ConfigName,
			Flushers: []map[string]interface{}{
				{
					"Logstore": logstore,
					"Type":     "flusher_sls",
				},
			},
			Global:     config.LogtailConfig.Global,
			Inputs:     config.LogtailConfig.Inputs,
			LogSample:  config.LogtailConfig.LogSample,
			Processors: config.LogtailConfig.Processors,
		}
		for i := 0; i < *flags.LogOperationMaxRetryTimes; i++ {
			_, err = o.logClient.GetClient().CreateLogtailPipelineConfig(&project, &createLogtailPipelineConfigRequest)
			if err == nil {
				break
			}
		}
		annotations := GetAnnotationByObject(config, project, logstore, "", tea.StringValue(config.LogtailConfig.ConfigName), true)
		if err != nil {
			customErr := CustomErrorFromSlsSDKError(err)
			o.eventRecorder.SendErrorEventWithAnnotation(o.eventRecorder.GetObject(), GetAnnotationByError(annotations, customErr), k8s_event.CreateConfig, "", fmt.Sprintf("create config failed, error: %s", err.Error()))
		} else {
			o.eventRecorder.SendNormalEventWithAnnotation(o.eventRecorder.GetObject(), annotations, k8s_event.CreateConfig, "create config success")
		}
		logger.Info(context.Background(), "config created, config", *config)
	}
	if err != nil {
		return fmt.Errorf("UpdateConfig error, config : %s, error : %s", tea.StringValue(config.LogtailConfig.ConfigName), err.Error())
	}
	logger.Debug(context.Background(), "create or update config success", tea.StringValue(config.LogtailConfig.ConfigName))

	// 为Logtail配置添加标签
	logtailConfigTags := map[string]string{}
	if config.ConfigTags != nil {
		for k, v := range config.ConfigTags {
			logtailConfigTags[k] = v
		}
	}
	logtailConfigTags[SlsLogtailChannalKey] = SlsLogtailChannalEnv
	err = o.TagLogtailConfig(project, tea.StringValue(config.LogtailConfig.ConfigName), logtailConfigTags)
	annotations := GetAnnotationByObject(config, project, logstore, "", tea.StringValue(config.LogtailConfig.ConfigName), true)
	if err != nil {
		o.eventRecorder.SendErrorEventWithAnnotation(o.eventRecorder.GetObject(), GetAnnotationByError(annotations, CustomErrorFromSlsSDKError(err)), k8s_event.CreateTag, "", fmt.Sprintf("tag config %s error :%s", tea.StringValue(config.LogtailConfig.ConfigName), err.Error()))
	} else {
		o.eventRecorder.SendNormalEventWithAnnotation(o.eventRecorder.GetObject(), annotations, k8s_event.CreateTag, fmt.Sprintf("tag config %s success", tea.StringValue(config.LogtailConfig.ConfigName)))
	}

	var machineGroup string
	// 如果配置中有机器组,取第一个机器组
	if len(config.MachineGroups) > 0 {
		machineGroup = config.MachineGroups[0]
	} else {
		// 如果配置中没有机器组,使用默认的机器组
		machineGroup = *flags.DefaultLogMachineGroup
	}
	// 确保机器组存在
	err = o.makesureMachineGroupExist(project, machineGroup)
	if err != nil {
		return fmt.Errorf("makesureMachineGroupExist error, config : %s, machineGroup : %s, error : %s", tea.StringValue(config.LogtailConfig.ConfigName), machineGroup, err.Error())
	}

	// 将配置应用到机器组
	for i := 0; i < *flags.LogOperationMaxRetryTimes; i++ {
		_, err = o.logClient.GetClient().ApplyConfigToMachineGroup(&project, &machineGroup, config.LogtailConfig.ConfigName)
		if err == nil {
			break
		}
	}
	if err != nil {
		return fmt.Errorf("ApplyConfigToMachineGroup error, config : %s, machine group : %s, error : %s", tea.StringValue(config.LogtailConfig.ConfigName), machineGroup, err.Error())
	}
	logger.Info(context.Background(), "apply config to machine group success", tea.StringValue(config.LogtailConfig.ConfigName), "group", machineGroup)
	return nil
}
