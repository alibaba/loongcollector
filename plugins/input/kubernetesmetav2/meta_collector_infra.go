package kubernetesmetav2

import (
	"strconv"
	"strings"
	"time"

	v1 "k8s.io/api/core/v1"

	"github.com/alibaba/ilogtail/pkg/helper/k8smeta"
	"github.com/alibaba/ilogtail/pkg/models"
)

func (m *metaCollector) processInfraServerLink(data *k8smeta.ObjectWrapper, obj *v1.Node, method, serverID string) *models.Log {
	// generate infra.server entity from k8s.node
	logInfraLink := &models.Log{}
	logInfraLink.Contents = models.NewLogContents()
	logInfraLink.Timestamp = uint64(time.Now().Unix())
	logInfraLink.Contents.Add(entityLinkRelationTypeFieldName, crossDomainSameAs) // same as

	logInfraLink.Contents.Add(entityLinkSrcDomainFieldName, m.serviceK8sMeta.domain) // e.g. scr is k8s.node
	logInfraLink.Contents.Add(entityLinkSrcEntityTypeFieldName, m.genEntityTypeKey(obj.Kind))
	logInfraLink.Contents.Add(entityLinkSrcEntityIDFieldName, m.genKey(obj.Kind, "", obj.Name))

	logInfraLink.Contents.Add(entityLinkDestDomainFieldName, infraDomain) // dest is infra.server
	logInfraLink.Contents.Add(entityLinkDestEntityTypeFieldName, infraServer)
	logInfraLink.Contents.Add(entityLinkDestEntityIDFieldName, m.genOtherKey(serverID)) // dest key id
	logInfraLink.Contents.Add(entityMethodFieldName, method)

	logInfraLink.Contents.Add(entityFirstObservedTimeFieldName, strconv.FormatInt(data.FirstObservedTime, 10))
	logInfraLink.Contents.Add(entityLastObservedTimeFieldName, strconv.FormatInt(data.LastObservedTime, 10))
	logInfraLink.Contents.Add(entityKeepAliveSecondsFieldName, strconv.FormatInt(int64(m.serviceK8sMeta.Interval*2), 10))
	logInfraLink.Contents.Add(entityCategoryFieldName, defaultEntityLinkCategory)
	return logInfraLink
}

func (m *metaCollector) generateInfraServerKeyID(nodeObj *v1.Node) string {

	serverID := nodeObj.Name

	// (1) if aliyunInstanceIDLabel exist in labels, return aliyunInstanceIDLabel value
	if nodeObj.Labels != nil {
		for label, value := range nodeObj.Labels {
			if strings.Contains(label, aliyunInstanceIDLabel) {
				return value
			}
		}
	}

	// (2) if node status has host name filed, using hostname instead
	if nodeObj.Status.Addresses != nil && len(nodeObj.Status.Addresses) > 0 {
		for _, addr := range nodeObj.Status.Addresses {
			if addr.Type == v1.NodeHostName {
				serverID = addr.Address
				return serverID
			}
		}
	}

	// (3) replace server_id by provider_id
	if nodeObj.Spec.ProviderID != "" {
		serverID = nodeObj.Spec.ProviderID
	}

	// (4) Ensure the value not empty
	return serverID
}
