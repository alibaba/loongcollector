package kubernetesmetav2

const (
	// should keep same with EntityConstants.cpp
	entityDomainFieldName        = "__domain__"
	entityTypeFieldName          = "__entity_type__"
	entityIDFieldName            = "__entity_id__"
	entityMethodFieldName        = "__method__"
	entityClusterIDFieldName     = "cluster_id"
	entityClusterNameFieldName   = "cluster_name"
	entityClusterRegionFieldName = "region_id"
	entityKindFieldName          = "kind"
	entityNameFieldName          = "name"
	entityCreationTimeFieldName  = "create_time"

	entityFirstObservedTimeFieldName = "__first_observed_time__"
	entityLastObservedTimeFieldName  = "__last_observed_time__"
	entityKeepAliveSecondsFieldName  = "__keep_alive_seconds__"

	entityCategoryFieldName      = "__category__"
	entityCategorySelfMetricName = "category"
	defaultEntityCategory        = "entity"
	defaultEntityLinkCategory    = "entity_link"

	entityLinkSrcDomainFieldName      = "__src_domain__"
	entityLinkSrcEntityTypeFieldName  = "__src_entity_type__"
	entityLinkSrcEntityIDFieldName    = "__src_entity_id__"
	entityLinkDestDomainFieldName     = "__dest_domain__"
	entityLinkDestEntityTypeFieldName = "__dest_entity_type__"
	entityLinkDestEntityIDFieldName   = "__dest_entity_id__"
	entityLinkRelationTypeFieldName   = "__relation_type__"
)

const (
	k8sDomain  = "k8s"
	ackCluster = "ack"
	oneCluster = "one"
	asiCluster = "asi"

	clusterTypeName   = "cluster"
	containerTypeName = "container"
)
