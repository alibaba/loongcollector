# 统一管控协议

本规范定义了 Agent 管控网络协议以及 iLogtail 和 ConfigServer 的预期行为。

1. 只要XxxConfigServer实现了协议，那么就可以管控Agent做Yyy事情。
2. 只要Agent实现了协议，那么任何XxxConfigServer就能过管控该Agent做Yyy事情。

## 管控协议

/Agent/Heartbeat?InstanceId=$instance\_id&WaitForChange=(true|false)

### HeartbeatRequest 消息

    message HeartbeatRequest {
        bytes request_id = 1;
        uint64 sequence_num = 2;                    // Increment every request, for server to check sync status
        uint64 capabilities = 3;                    // Bitmask of flags defined by AgentCapabilities enum
        bytes instance_id = 4;                      // Required, Agent's unique identification, consistent throughout the process lifecycle
        string agent_type = 5;                      // Required, Agent's type(ilogtail, ..)
        AgentAttributes attributes = 6;             // Agent's basic attributes
        repeated AgentGroupTag tags = 7;            // Agent's tags
        string running_status = 8;                  // Human readable running status
        int64 startup_time = 9;                     // Required, Agent's startup time
        repeated ConfigInfo pipeline_configs = 10;  // Information about the current PIPELINE_CONFIG held by the Agent
        repeated ConfigInfo instance_configs = 11;  // Information about the current AGENT_CONFIG held by the Agent
        repeated CommandInfo custom_commands = 12;  // Information about command history
        uint64 flags = 13;                          // Predefined command flag
        bytes opaque = 14;                          // Opaque data for extension
        // before 100 (inclusive) are reserved for future official fields
    }
    
    message AgentGroupTag {
        string name = 1;
        string value = 2;
    }

    enum ConfigStatus {
        // The value of status field is not set.
        UNSET = 0;
        // Agent is currently applying the remote config that it received earlier.
        APPLYING = 1;
        // Remote config was successfully applied by the Agent.
        APPLIED = 2;
        // Agent tried to apply the config received earlier, but it failed.
        // See error_message for more details.
        FAILED = 3;
    }

    // Define the Config information carried in the request
    message ConfigInfo {
        string name = 1;         // Required, Config's unique identification
        int64 version = 2;       // Required, Config's version number or hash code
        ConfigStatus status = 3; // Config's status
        string message = 4;      // Optional error message
        map<string, bytes> extra = 5; // Optional extra info
    }

    // Define the Command information carried in the request
    message CommandInfo {
        string type = 1;         // Command's type
        string name = 2;         // Required, Command's unique identification
        ConfigStatus status = 3; // Command's status
        string message = 4;      // Optional error message
        map<string, bytes> extra = 5;   // Optional extra info
    }

    // Define Agent's basic attributes
    message AgentAttributes {
        bytes version = 1;                 // Agent's version
        bytes ip = 2;                      // Agent's ip
        bytes hostname = 3;                // Agent's hostname
        map<string, bytes> extras = 100;   // Agent's other attributes
        // before 100 (inclusive) are reserved for future official fields
    }

    enum AgentCapabilities {
        // The capabilities field is unspecified.
        UnspecifiedAgentCapability = 0;
        // The Agent can accept pipeline configuration from the Server.
        AcceptsPipelineConfig          = 0x00000001;
        // The Agent can accept instance configuration from the Server.
        AcceptsInstanceConfig           = 0x00000002;
        // The Agent can accept custom command from the Server.
        AcceptsCustomCommand           = 0x00000004;

        // bits before 2^16 (inclusive) are reserved for future official fields
    }

    enum RequestFlags {
        RequestFlagsUnspecified = 0;

        // Flags is a bit mask. Values below define individual bits.

        // Must be set if this request contains full state
        FullState               = 0x00000001;
        // bits before 2^16 (inclusive) are reserved for future official fields
    }

### HeartbeatResponse 消息

    message HeartbeatResponse {
        bytes request_id = 1;  
        ServerErrorResponse error_response = 2;             // Set value indicates error
        uint64 capabilities = 3;                            // Bitmask of flags defined by ServerCapabilities enum

        repeated ConfigDetail pipeline_config_updates = 4;  // Agent's pipeline config update status
        repeated ConfigDetail instance_config_updates = 5;  // Agent's instance config update status
        repeated CommandDetail custom_command_updates = 6;  // Agent's commands updates
        uint64 flags = 7;                                   // Predefined command flag
        bytes opaque = 8;                                   // Opaque data for extension
    }
    
    message ConfigDetail {
        string name = 1;        // Required, Config's unique identification
        int64 version = 2;      // Required, Config's version number or hash code
        bytes detail = 3;       // Required, Config's detail
        map<string, bytes> extra = 4;   // Optional extra info
    }

    message CommandDetail {
        string type = 1;                // Required, Command type
        string name = 2;                // Required, Command name
        bytes detail = 3;               // Required, Command's detail
        int64 expire_time = 4;          // After which the command can be safely removed from history
        map<string, bytes> extra = 5;   // Optional extra info
    }

    enum ServerCapabilities {
        // The capabilities field is unspecified.
        UnspecifiedServerCapability        = 0;
        // The Server can remember agent attributes.
        RembersAttribute                   = 0x00000001;
        // The Server can remember pipeline config status.
        RembersPipelineConfigStatus        = 0x00000002;
        // The Server can remember instance config status.
        RembersInstanceConfigStatus        = 0x00000004;
        // The Server can remember custom command status.
        RembersCustomCommandStatus         = 0x00000008;

        // bits before 2^16 (inclusive) are reserved for future official fields
    }

    message ServerErrorResponse {
        int32 error_code = 1;        // None-zero value indicates error
        string error_message = 2;    // Error message
    }

    enum ResponseFlags {
        ResponseFlagsUnspecified = 0;

        // Flags is a bit mask. Values below define individual bits.

        // ReportFullState flag can be used by the Server if the Client did not include
        // some sub-message in the last AgentToServer message (which is an allowed
        // optimization) but the Server detects that it does not have it (e.g. was
        // restarted and lost state).
        ReportFullState           = 0x00000001;
        // FetchPipelineConfigDetail can be used by the Server to tell Agent to fetch config details by FetchConfig api,
        // HB response ConfigDetail will not contains details.
        FetchPipelineConfigDetail = 0x00000002;
        // like FetchPipelineConfigDetail, but for instance config.
        FetchInstanceConfigDetail = 0x00000004;
        // bits before 2^16 (inclusive) are reserved for future official fields
    }

### [Optional] FetchConfigRequest 消息

额外的 config 拉取接口，不通过心跳返回 config 详情。

    message FetchConfigRequest {
        bytes request_id = 1; 
        bytes instance_id = 2;                     // Agent's unique identification
        repeated ConfigInfo pipeline_configs = 3;  // Information about the current PIPELINE_CONFIG held by the Agent
        repeated ConfigInfo instance_configs = 4;  // Information about the current AGENT_CONFIG held by the Agent
        repeated CommandInfo custom_commands = 5;  // Information about command history
    }

### [Optional] FetchConfigResponse 消息

    message FetchConfigResponse {
        bytes request_id = 1;                     
        CommonResponse commonResponse = 2;
        repeated ConfigDetail pipeline_config_updates = 3;  // Agent's pipeline config with details
        repeated ConfigDetail instance_config_updates = 4;  // Agent's instance config with details
        repeated CommandDetail custom_command_updates = 5;  // Agent's commands details
    }

### [Optional] ReportStatusRequest 消息

额外的 config 应用状态上报接口，不依赖于等到下次心跳上报。适用于心跳和 config 状态服务拆分的实现。

    message ReportStatusRequest {
        bytes request_id = 1;
        bytes instance_id = 2;                     // Agent's unique identification
        repeated ConfigInfo pipeline_configs = 3;  // status about the current PIPELINE_CONFIG held by the Agent
        repeated ConfigInfo instance_configs = 4;  // status about the current AGENT_CONFIG held by the Agent
        repeated CommandInfo custom_commands = 5;  // status about command history
    }

### [Optional] ReportStatusResponse 消息

    message ReportStatusResponse {
        bytes request_id = 1;
        CommonResponse commonResponse = 2;
    }

## 行为规范

对于管控协议来说 iLogtail 的预期行为是确定性的，对于实现本管控协议的其他 Agent 其具体行为可自行确定，但语义应保持一致。Server 端定义了可选的行为的不同实现，此时对于这些差异 Agent 侧在实现时必须都考虑到且做好兼容。这样，Agent只需要实现一个CommonConfigProvider就可以受任意符合此协议规范的ConfigServer管控。

### 能力报告

Client：应当通过capbilitiies上报Agent自身的能力，这样如果老的客户端接入新的ConfigServer，ConfigServer便知道客户端不具备某项能力，从而不会向其发送不支持的配置或命令而得不到状态汇报导致无限循环。

Server：应当通过capbilitiies上报Server自身的能力，这样如果新的客户端接入老的ConfigServer，Agent便知道服务端不具备某项能力，从而不会被其响应所误导，如其不具备记忆Attributes能力，那么Attributes字段无论如何都不应该在心跳中被省略。

### 注册

Client：Agent启动后第一次向Server汇报全量信息，request字段应填尽填。request\_id、sequence\_num、capabilities、instance\_id、agent\_type、startup\_time为必填字段。

Server：Server根据上报的信息返回响应。pipeline\_config\_updates、instance\_config\_updates中包含agent需要同步的配置，updates中必然包含name和version，是否包含detail取决于server端实现, 如果不包含则需要通过 FetchConfig 拉取。custom\_command_updates包含要求agent执行的命令command中必然包含type、name和expire\_time。

Server是否保存Client信息取决于Server实现，如果服务端找不到或保存的sequence\_num + 1 ≠ 心跳的sequence\_num，那么就立刻返回并且flags中必须设置ReportFullStatus标识位。

Server根据agent\_type + attributes 查询进程配置，根据ip和tags查询机器组和关联采集配置。

![image](https://github.com/alibaba/ilogtail/assets/1827594/05799ac2-9249-49ed-8088-3b927821ac73)

### 心跳（心跳压缩）

Client：若接收到的响应中没有ReportFullStatus，且client的属性、配置状态、命令状态在上次上报后没有变化，那么可以只填instance\_id、sequence\_num，sequence\_num每次请求+1。若有ReportStatus或任何属性、配置状态变化或Server不支持属性、配置状态记忆能力，则必须完整上报状态。

Server：同注册

允许心跳压缩

![image](https://github.com/alibaba/ilogtail/assets/1827594/ad22bcfa-b14a-41b5-b4b8-4956bb065bf7)

不允许心跳压缩

![image](https://github.com/alibaba/ilogtail/assets/1827594/35a823fd-6b8e-499f-951e-2231f3319420)

### 进程配置

可选两种实现：
1. 在心跳中完成进程配置的状态上报与同步。

    Server的注册/心跳响应中有instance\_config\_updates.detail，client 直接从response中获得detail，应用成功后下次心跳需要上报完整状态。
    
2. 在心跳中完成进程配置的基础信息同步，通过额外的接口完成进程配置的拉取。

    Server的响应不包含detail, 只包含要更新的进程配置 name 和 version。client 比较本地的配置和 version 判断需要更新后，根据 instance_config_updates 的信息构造 FetchConfigRequest 后进行一次额外拉取。FetchConfigRequest 至少需要包括 name 和 version。

    心跳 response flag 需要设置 FetchInstanceConfigDetail.

Client获取到多个进程配置时，自动合并，若产生冲突默认行为是未定义。

### 采集配置

可选两种实现：
1. 在心跳中完成采集配置的状态上报与同步。

    Server的注册/心跳响应中有pipeline\_config\_updates.detail, Client 直接从response中获得detail，应用成功后下次心跳需要上报完整状态。

2. 在心跳中完成采集配置的基础信息同步，通过额外的接口完成进程配置的拉取。

    Server的响应不包含detail, 只包含要更新的采集配置 name 和 version。client 比较本地的配置和 version 判断需要更新后，根据 pipeline_config_updates 的信息构造 FetchConfigRequest 后进行一次额外拉取。FetchConfigRequest 至少需要包括 name 和 version。

    心跳 response flag 需要设置 FetchPipelineConfigDetail.

客户端以下2种实现

实现1：直接将Detail返回在心跳响应中（FetchPipelineConfigDetail flag is unset）

![image](https://github.com/alibaba/ilogtail/assets/1827594/be645615-dd99-42dd-9deb-681e9a4069bb)

实现2：仅返回配置名和版本，Detail使用单独请求获取（FetchPipelineConfigDetail flag is set）

![image](https://github.com/alibaba/ilogtail/assets/1827594/c409c35c-2a81-4927-bfd2-7fb321ef1ca8)

### 配置状态上报

不管是进程配置还是采集配置，下发的配置需要上报应用状态。

对于 Client：这个版本的配置状态上报中修改了version的定义，-1仍然表示删除，0作为保留值，其他值都是合法version，只要version不同Client都应该视为配置更新。此外参考OpAMP增加了配置应用状态上报的字段，能反应出下发的配置是否生效。

对于 Server：这些信息是Agent状态的一部分，可选保存。与通过Event上报可观测信息不同的是，作为状态信息没有时间属性，用户可通过接口可获取即刻状态，而不需要选择时间窗口合并事件。

同进程配置和采集配置，上报配置状态也有两种可选实现：
1. 在心跳 request 中将配置最新状态带上。

    在心跳中将进程配置和采集配置的最新版本和状态一起上报。另外按照心跳协议的定义，配置状态变更后，要求在心跳一定要上报配置最新状态，如果相较于上一次心跳配置状态无变化，则不要求。

2. 通过 ReportStatus 接口上报。适合对状态更新实时性要求比较高，或对心跳服务、配置服务、状态服务拆分以减少故障半径的实现。

    通过 ReportStatus 额外接口去上报，能够在一定程度上减少心跳服务的复杂度，有利于状态服务和心跳服务的拆分。ReportStatus 接口不用等到下一次心跳，在配置状态发生变化即可上报。

### 心跳配置拉取/上报与额外接口拉取/上报选择
配置状态上报的方式应该和配置拉取方式配套使用：
1. 如果进程配置和采集配置都通过心跳下发，状态配置也仅应该通过心跳上报。
2. 如果进程配置和采集配置都通过 FetchConfig 接口拉取，状态上报也应该通过 ReportStatus 上报。

虽然其他的组合方式也能完成配置的下发和状态上报，但会导致服务拆分和服务职责不清晰的问题。如无特殊要求，建议通过心跳完成配置下发和上报。

通过额外接口拉取配置和状态上报流程：
![image](https://github.com/user-attachments/assets/07224fe6-9454-4fcb-9c56-2a46eee33f0b)

### 预定义命令

Client: 通过request的flag传递，定义了FullStatus，表明本条信息为全量状态

Server: 通过response的flag传递，定义了ReportFullStatus，表明要求Client上报全量状态信息

### 自定义命令

Client: 为了防止服务端重复下发命令以及感知命令执行结果，在command expire前，Client始终应具备向服务端上报command执行状态的能力，实际是否上报取决于心跳压缩机制。在expire\_time超过后，client不应该再上报超时的command状态。

Server: 如果上报+已知的Agent状态中，缺少应下发的custom\_command\_updates（通过name识别），那么server应该在响应中下发缺少的custom\_command\_updates。

### 异常处理

Server: 服务端正常返回时HeartbeatResponse中的code应始终设置为0，而当服务端异常时，必须将HeartbeatResponse中的code设置为非0，HeartbeatResponse中的message应包含错误信息，此时Response中的其他字段必须为空。

Client: 当HeartbeatResponse中的code为0时，Agent应该正常处理下发的配置。当HeartbeatResponse中的code不为0时，Agent必须忽略除code和message外的其他字段，并择机重试。

### 辅助信息
在command\_info, command\_detail, config\_info, config\_detail中，都预留了extra字段，可以用于传递一些额外的用户自定义的辅助信息。\

注意：extra字段仅作传递辅助信息使用，不会对管控行为造成任何影响。
