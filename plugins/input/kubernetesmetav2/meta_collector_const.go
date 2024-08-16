package kubernetesmetav2

const (
	entityDomainFieldName = "__domain__"
	entityTypeFieldName   = "__entity_type__"
	entityIDFieldName     = "__entity_id__"
	entityMethodFieldName = "__method__"

	entityFirstObservedTimeFieldName = "__first_observed_time__"
	entityLastObservedTimeFieldName  = "__last_observed_time__"
	entityKeepAliveSecondsFieldName  = "__keep_alive_seconds__"
	entityNamespaceFieldName         = "namespace"
	entityNameFieldName              = "name"

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
