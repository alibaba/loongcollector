package kubernetesmetav2

const (
	entityDomainFieldName       = "__domain__"
	entityTypeFieldName         = "__entity_type__"
	entityIDFieldName           = "__entity_id__"
	entityMethodFieldName       = "__method__"
	entityClusterIDFieldName    = "cluster_id"
	entityKindFieldName         = "kind"
	entityNameFieldName         = "name"
	entityCreationTimeFieldName = "create_time"

	entityFirstObservedTimeFieldName = "__first_observed_time__"
	entityLastObservedTimeFieldName  = "__last_observed_time__"
	entityKeepAliveSecondsFieldName  = "__keep_alive_seconds__"

	entityCategoryFieldName   = "__category__"
	defaultEntityCategory     = "entity"
	defaultEntityLinkCategory = "entity_link"

	entityLinkSrcDomainFieldName      = "__src_domain__"
	entityLinkSrcEntityTypeFieldName  = "__src_entity_type__"
	entityLinkSrcEntityIDFieldName    = "__src_entity_id__"
	entityLinkDestDomainFieldName     = "__dest_domain__"
	entityLinkDestEntityTypeFieldName = "__dest_entity_type__"
	entityLinkDestEntityIDFieldName   = "__dest_entity_id__"
	entityLinkRelationTypeFieldName   = "__relation_type__"
)

var DomainEntityTypePrefix = map[string]string{
	"acs":   "acs.ack.cluster.",
	"infra": "infra.k8s.cluster.",
}
