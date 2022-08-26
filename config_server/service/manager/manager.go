package manager

import (
	agentManager "github.com/alibaba/ilogtail/config_server/service/manager/agent"
	configManager "github.com/alibaba/ilogtail/config_server/service/manager/config"
	"github.com/alibaba/ilogtail/config_server/service/model"
	"github.com/alibaba/ilogtail/config_server/service/setting"
)

var myAgentManager *agentManager.AgentManager
var myConfigManager *configManager.ConfigManager

func AgentManager() *agentManager.AgentManager {
	return myAgentManager
}

func ConfigManager() *configManager.ConfigManager {
	return myConfigManager
}

func init() {
	myAgentManager = new(agentManager.AgentManager)
	myAgentManager.AgentMessageList.Init()
	go myAgentManager.UpdateAgentMessage(setting.GetSetting().AgentUpdateInterval)

	myConfigManager = new(configManager.ConfigManager)
	myConfigManager.ConfigList = make(map[string]*model.Config)
	go myConfigManager.UpdateConfigList(setting.GetSetting().ConfigSyncInterval)
}
