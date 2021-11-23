## User Guide （中文）

[README  in English](https://github.com/aliyun/aliyun-log-go-sdk/blob/master/README_EN.md)

### 基本介绍

本项目是阿里云日志服务 （Log Service，简称SLS）API的Golang编程接口，提供了对于Log Service Rest API的封装和实现，帮助Golang开发人员更快编程使用阿里云日志服务。

本项目主要由3个部分组成：

1. 日志服务基础API封装和实现。
2. Golang Producer Library，用于向日志服务批量发送数据，详情参考[**Aliyun LOG Golang Producer 快速入门**](https://github.com/aliyun/aliyun-log-go-sdk/tree/master/producer)。
3. Golang Consumer Library，用于消费日志服务中的数据，详情参考[**Consumer Library**](https://github.com/aliyun/aliyun-log-go-sdk/tree/master/consumer)。

详细API接口以及含义请参考：https://help.aliyun.com/document_detail/29007.html

### 安装

```
go get -u github.com/aliyun/aliyun-log-go-sdk
```



### 快速入门

**前言:**   所有的使用样例都位于[example](https://github.com/aliyun/aliyun-log-go-sdk/tree/master/example)目录下，使用该目录下的所有样例前，请先在该目录下的[config.go](https://github.com/aliyun/aliyun-log-go-sdk/blob/master/example/util/config.go)文件中的**init函数**中配置您的 project, logstore等所需要的配置参数，example目录下的所有样例都会使用config.go文件中配置的参数。

1. **创建Client**

   参考 [config.go](https://github.com/aliyun/aliyun-log-go-sdk/blob/master/example/util/config.go) 文件

   ```go
   AccessKeyID = "your ak id"
   AccessKeySecret = "your ak secret"
   Endpoint = "your endpoint" // just like cn-hangzhou.log.aliyuncs.com
   Client = sls.CreateNormalInterface(Endpoint,AccessKeyID,AccessKeySecret,"")
   ```

2. **创建project**

   参考 [log_project.go](https://github.com/aliyun/aliyun-log-go-sdk/blob/master/example/project/log_project.go)文件

   ```go lgo l
   project, err := util.Client.CreateProject(ProjectName,"Project used for testing")
   if err != nil {
      fmt.Println(err)
   }
   fmt.Println(project)
   ```

3. **创建logstore**

   参考 [log_logstore.go](https://github.com/aliyun/aliyun-log-go-sdk/blob/master/example/logstore/log_logstore.go)

   ```go
   err := util.Client.CreateLogStore(ProjectName, LogStoreName,2,2,true,64)
   if err != nil {
      fmt.Println(err)
   }
   ```

4. **创建索引**

   参考[index_sample](https://github.com/aliyun/aliyun-log-go-sdk/blob/master/example/index/index_sample.go)

   ```go
   indexKeys := map[string]sls.IndexKey{
      "col_0": {
         Token:         []string{" "},
         CaseSensitive: false,
         Type:          "long",
      },
      "col_1": {
         Token:         []string{",", ":", " "},
         CaseSensitive: false,
         Type:          "text",
      },
   }
   index := sls.Index{
      Keys: indexKeys,
      Line: &sls.IndexLine{
         Token:         []string{",", ":", " "},
         CaseSensitive: false,
         IncludeKeys:   []string{},
         ExcludeKeys:   []string{},
      },
   }
   err = util.Client.CreateIndex(util.ProjectName, logstore_name, index)
   if err != nil {
      fmt.Printf("CreateIndex fail, err: %s", err)
      return
   }
   fmt.Println("CreateIndex success")
   ```

5. **写数据**

   参考[put_log.go](https://github.com/aliyun/aliyun-log-go-sdk/blob/master/example/loghub/put_log.go)

   这里展示了用sdk中原生的API接口去发送数据简单示例，但是我们不推荐用API直接向logstore写入数据，推荐使用SDK 中提供的[producer](https://github.com/aliyun/aliyun-log-go-sdk/tree/master/producer) 包向logstore 写入数据，自动压缩数据并且提供安全退出机制，不会使数据丢失。

   ```go
   logs := []*sls.Log{}
   for logIdx := 0; logIdx < 100; logIdx++ {
      content := []*sls.LogContent{}
      for colIdx := 0; colIdx < 10; colIdx++ {
         content = append(content, &sls.LogContent{
            Key:   proto.String(fmt.Sprintf("col_%d", colIdx)),
            Value: proto.String(fmt.Sprintf("loggroup idx: %d, log idx: %d, col idx: %d, value: %d", loggroupIdx, logIdx, colIdx, rand.Intn(10000000))),
         })
      }
      log := &sls.Log{
         Time:     proto.Uint32(uint32(time.Now().Unix())),
         Contents: content,
      }
      logs = append(logs, log)
   }
   loggroup := &sls.LogGroup{
      Topic:  proto.String(""),
      Source: proto.String("10.230.201.117"),
      Logs:   logs,
   }
   // PostLogStoreLogs API Ref: https://intl.aliyun.com/help/doc-detail/29026.htm
   for retryTimes := 0; retryTimes < 10; retryTimes++ {
      err := util.Client.PutLogs(util.ProjectName, util.LogStoreName, loggroup)
      if err == nil {
         fmt.Printf("PutLogs success, retry: %d\n", retryTimes)
         break
      } else {
         fmt.Printf("PutLogs fail, retry: %d, err: %s\n", retryTimes, err)
         //handle exception here, you can add retryable erorrCode, set appropriate put_retry
         if strings.Contains(err.Error(), sls.WRITE_QUOTA_EXCEED) || strings.Contains(err.Error(), sls.PROJECT_QUOTA_EXCEED) || strings.Contains(err.Error(), sls.SHARD_WRITE_QUOTA_EXCEED) {
            //mayby you should split shard
            time.Sleep(1000 * time.Millisecond)
         } else if strings.Contains(err.Error(), sls.INTERNAL_SERVER_ERROR) || strings.Contains(err.Error(), sls.SERVER_BUSY) {
            time.Sleep(200 * time.Millisecond)
         }
      }
   }
   ```

6.**读数据**

参考[pull_log.go](https://github.com/aliyun/aliyun-log-go-sdk/blob/master/example/loghub/pull_log.go)

这里展示了使用SDK中原生API接口调用去拉取数据的方式，我们不推荐使用这种方式去读取消费logstore中的数据，推荐使用SDK中 [consumer](https://github.com/aliyun/aliyun-log-go-sdk/tree/master/consumer) 消费组去拉取数据，消费组提供自动负载均衡以及失败重试等机制，并且会自动保存拉取断点，再次拉取不会拉取重复数据。

```go
shards, err := client.ListShards(project, logstore)

totalLogCount := 0
	for _, shard := range shards {
		fmt.Printf("[shard] %d begin\n", shard.ShardID)
		beginCursor, err := client.GetCursor(project, logstore, shard.ShardID, "begin")
		if err != nil {
			panic(err)
		}
		endCursor, err := client.GetCursor(project, logstore, shard.ShardID, "end")
		if err != nil {
			panic(err)
		}

		nextCursor := beginCursor
		for nextCursor != endCursor {
			gl, nc, err := client.PullLogs(project, logstore, shard.ShardID, nextCursor, endCursor, 10)
			if err != nil {
				fmt.Printf("pull log error : %s\n", err)
				time.Sleep(time.Second)
				continue
			}
			nextCursor = nc
			fmt.Printf("now count %d \n", totalLogCount)
			if gl != nil {
				for _, lg := range gl.LogGroups {
					for _, tag := range lg.LogTags {
						fmt.Printf("[tag] %s : %s\n", tag.GetKey(), tag.GetValue())
					}
					for _, log := range lg.Logs {
						totalLogCount++
						// print log
						for _, content := range log.Contents {
							fmt.Printf("[log] %s : %s\n", content.GetKey(), content.GetValue())
						}
					}
				}
			}
		}
```

7. **创建机器组**

   参考 [machine_group_sample.go](https://github.com/aliyun/aliyun-log-go-sdk/blob/master/example/machine_group/machine_group_sample.go) 

```go
attribute := sls.MachinGroupAttribute{
   ExternalName: "",
   TopicName:    "",
}
machineList := []string{"mac-user-defined-id-value"}
var machineGroup = &sls.MachineGroup{
   Name:          groupName,
   MachineIDType: "userdefined",
   MachineIDList: machineList,
   Attribute:     attribute,
}
err = util.Client.CreateMachineGroup(ProjectName, machineGroup)
if err != nil {
   fmt.Println(err)
}
```

8. **创建logtail 采集配置**

   logtail 采集配置，目前通过sdk 支持创建下列几种模式的采集配置，分别为 [完整正则](https://github.com/aliyun/aliyun-log-go-sdk/blob/master/example/logtail_config/log_config_common_regex.go)，[分隔符模式](https://github.com/aliyun/aliyun-log-go-sdk/blob/master/example/logtail_config/log_config_delimiter.go)，[json模式](https://github.com/aliyun/aliyun-log-go-sdk/blob/master/example/logtail_config/log_config_json.go) ，[插件模式](https://github.com/aliyun/aliyun-log-go-sdk/blob/master/example/logtail_config/log_config_plugin.go)，这里展示的完整正则模式的创建。

   ```go
   regexConfig := new(sls.RegexConfigInputDetail)
   regexConfig.DiscardUnmatch = false
   regexConfig.Key = []string{"logger", "time", "cluster", "hostname", "sr", "app", "workdir", "exe", "corepath", "signature", "backtrace"}
   regexConfig.Regex = "\\S*\\s+(\\S*)\\s+(\\S*\\s+\\S*)\\s+\\S*\\s+(\\S*)\\s+(\\S*)\\s+(\\S*)\\s+(\\S*)\\s+(\\S*)\\s+(\\S*)\\s+(\\S*)\\s+\\S*\\s+(\\S*)\\s*([^$]+)"
   regexConfig.TimeFormat = "%Y/%m/%d %H:%M:%S"
   regexConfig.LogBeginRegex = `INFO core_dump_info_data .*`
   regexConfig.LogPath = "/cloud/log/tianji/TianjiClient#/core_dump_manager"
   regexConfig.FilePattern = "core_dump_info_data.log*"
   regexConfig.MaxDepth = 0
   sls.InitRegexConfigInputDetail(regexConfig)
   outputDetail := sls.OutputDetail{
      ProjectName:  projectName,
      LogStoreName: logstore,
   }
   logConfig := &sls.LogConfig{
      Name:         configName,
      InputType:    "file",
      OutputType:   "LogService", // Now only supports LogService
      InputDetail:  regexConfig,
      OutputDetail: outputDetail,
   }
   err = util.Client.CreateConfig(projectName, logConfig)
   if err != nil {
      fmt.Println(err)
   }
   ```

9. **logtail配置到机器组**

   ```go
   err = util.Client.ApplyConfigToMachineGroup(ProjectName, confName, mgname)
   if err != nil {
      fmt.Println(err)
   }
   ```

   

# 开发者

### 更新protobuf工具

```
protoc -I=. -I=$GOPATH/src -I=$GOPATH/src/github.com/gogo/protobuf/protobuf --gofast_out=. log.proto
```