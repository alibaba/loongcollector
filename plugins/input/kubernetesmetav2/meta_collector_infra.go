package kubernetesmetav2

import (
	"strconv"
	"strings"
	"time"

	"github.com/alibaba/ilogtail/pkg/helper/k8smeta"
	"github.com/alibaba/ilogtail/pkg/models"
	v1 "k8s.io/api/core/v1"
)

func (m *metaCollector) processInfraServerLink(data *k8smeta.ObjectWrapper, obj *v1.Node, method, serverId string) *models.Log {
	// generate infra.server entity from k8s.node
	logInfraLink := &models.Log{}
	logInfraLink.Contents = models.NewLogContents()
	logInfraLink.Timestamp = uint64(time.Now().Unix())
	logInfraLink.Contents.Add(entityLinkRelationTypeFieldName, crossDomainSameAs) //same as

	logInfraLink.Contents.Add(entityLinkSrcDomainFieldName, m.serviceK8sMeta.domain) // e.g. scr is k8s.node
	logInfraLink.Contents.Add(entityLinkSrcEntityTypeFieldName, m.genEntityTypeKey(obj.Kind))
	logInfraLink.Contents.Add(entityLinkSrcEntityIDFieldName, m.genKey(obj.Kind, "", obj.Name))

	logInfraLink.Contents.Add(entityLinkDestDomainFieldName, infraDomain) // dest is infra.server
	logInfraLink.Contents.Add(entityLinkDestEntityTypeFieldName, infraServer)
	logInfraLink.Contents.Add(entityLinkDestEntityIDFieldName, m.genOtherKey(serverId)) // dest key id
	logInfraLink.Contents.Add(entityMethodFieldName, method)

	logInfraLink.Contents.Add(entityFirstObservedTimeFieldName, strconv.FormatInt(data.FirstObservedTime, 10))
	logInfraLink.Contents.Add(entityLastObservedTimeFieldName, strconv.FormatInt(data.LastObservedTime, 10))
	logInfraLink.Contents.Add(entityKeepAliveSecondsFieldName, strconv.FormatInt(int64(m.serviceK8sMeta.Interval*2), 10))
	logInfraLink.Contents.Add(entityCategoryFieldName, defaultEntityLinkCategory)
	return logInfraLink
}

func (m *metaCollector) generateInfraServerKeyId(nodeObj *v1.Node) string {

	serverId := nodeObj.Name

	// (1) replace server_id by provider_id
	if nodeObj.Spec.ProviderID != "" {
		// if node belong to azure, aws, gce, return providerID as infra.server id
		if strings.HasPrefix(nodeObj.Spec.ProviderID, "azure") || strings.HasPrefix(nodeObj.Spec.ProviderID, "aws") || strings.HasPrefix(nodeObj.Spec.ProviderID, "gce") {
			serverId = nodeObj.Spec.ProviderID
		}
	}

	// (2) if aliyunInstanceIDLabel exist in labels, return aliyunInstanceIDLabel value
	if nodeObj.Labels != nil && nodeObj.Labels[aliyunInstanceIDLabel] != "" {
		serverId = nodeObj.Labels[aliyunInstanceIDLabel]
		return serverId
	}

	// (3) if node status has host name filed, using hostname instead
	if nodeObj.Status.Addresses != nil && len(nodeObj.Status.Addresses) > 0 {
		for _, addr := range nodeObj.Status.Addresses {
			if addr.Type == v1.NodeHostName {
				serverId = addr.Address
				break
			}
		}
	}
	// (4) Ensure the value not empty
	return serverId
}
