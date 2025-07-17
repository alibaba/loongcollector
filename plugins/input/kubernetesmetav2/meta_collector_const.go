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
	entityServerIdFieldName     = "server_id"
	entityServerRegionFieldName = "region_id"
	entityServerIPFieldName     = "ip"
	entityServerSourceFieldName = "source"
	crossDomainSameAs           = "same_as"
)

const (
	k8sDomain      = "k8s"
	infraDomain    = "infra"
	acsDomain      = "acs"
	infraServer    = "infra.server"
	k8sNode        = "k8s.node"
	acsEcsInstance = "acs.ecs.instance"
	acsAckCluster  = "acs.ack.cluster"
	ackCluster     = "ack"
	oneCluster     = "one"
	asiCluster     = "asi"

	AliyunCloudProvider = "alibaba_cloud"

	clusterTypeName   = "cluster"
	containerTypeName = "container"

	aliyunInstanceIDLabel = "alibabacloud.com/ecs-instance-id"
	aliyunNodeIDLabel     = "alibabacloud.com/nodepool-id"
	topologyRegion        = "topology.kubernetes.io/region" //aliyun、aws、zure、gce

	aliyunSource = "aliyun"
	awsSource    = "aws"
	azureSource  = "azure"
	gceSource    = "gce"
	customSource = "custom"
)
