package k8smeta

import (
	"fmt"
	"strings"

	"k8s.io/apimachinery/pkg/runtime/schema"
)

// CustomResourceCollectorConfig describes one third-party API resource collected via a dynamic informer.
// Use MetaManager.RegisterCustomResourceCollector after GetMetaManagerInstance and before or after Init
// (late registration will receive the REST config stored at Init).
//
// YAML-friendly field names match JSON tags when used in pipeline configs.
type CustomResourceCollectorConfig struct {
	// EntityType is required: internal cache key and K8sMetaEvent.ResourceType (e.g. customresource/argoproj.io/workflow).
	// It drives __entity_type__, __entity_id__, and pod->{EntityType} links — set explicitly in pipeline config.
	EntityType string `json:"EntityType,omitempty"`

	APIGroup   string `json:"APIGroup,omitempty"`
	APIVersion string `json:"APIVersion,omitempty"`
	Resource   string `json:"Resource,omitempty"` // plural resource name
	Kind       string `json:"Kind,omitempty"`     // Kubernetes kind, for ownerReferences matching and export

	// PodLink, if set, registers a Pod → this CR link generator (link type: PodLinkTypeForEntity(EntityType)).
	PodLink *PodToCustomResourceLinkConfig `json:"PodLink,omitempty"`
	// CollectEntity registers entity collection (K8sMetaEvent stream) for this CR.
	CollectEntity bool `json:"CollectEntity,omitempty"`
	// Entity2PodRelation is __relation_type__ on entity_link logs (custom resource → Pod). Required when Pod link export is enabled together with PodLink.
	Entity2PodRelation string `json:"Entity2PodRelation,omitempty"`
	// Namespace2EntityRelation is __relation_type__ on entity_link logs (Namespace → this namespaced CR). Export when CollectEntity, Namespace input, and this string are all set. Cluster-scoped CRs are skipped.
	Namespace2EntityRelation string `json:"Namespace2EntityRelation,omitempty"`

	// EnableLabels, if true, exports full labels on entity logs. Ignores ServiceK8sMeta.EnableLabels. Default false.
	EnableLabels bool `json:"EnableLabels,omitempty"`
	// EnableAnnotations, if true, exports full annotations on entity logs. Ignores ServiceK8sMeta.EnableAnnotations. Default false.
	EnableAnnotations bool `json:"EnableAnnotations,omitempty"`
}

// PodToCustomResourceLinkConfig resolves which Workflow-like object a Pod belongs to.
type PodToCustomResourceLinkConfig struct {
	// OwnerKind matches Pod ownerReferences[].Kind (e.g. Workflow).
	OwnerKind string `json:"OwnerKind,omitempty"`
	// OwnerAPIGroupContains is matched as substring of ownerReferences[].APIVersion (empty => use collector APIGroup).
	OwnerAPIGroupContains string `json:"OwnerAPIGroupContains,omitempty"`
	// PodLabelKey fallback when no matching ownerRef (e.g. workflows.argoproj.io/workflow).
	PodLabelKey string `json:"PodLabelKey,omitempty"`
}

// PodLinkTypeForEntity returns the link ResourceType for RegisterSendFunc (e.g. pod->customresource/...).
func PodLinkTypeForEntity(entityType string) string {
	return POD + LINK_SPLIT_CHARACTER + entityType
}

// NamespaceLinkTypeForEntity is the link ResourceType for Namespace → namespaced CR (e.g. argo.workflow->namespace).
func NamespaceLinkTypeForEntity(entityType string) string {
	return entityType + LINK_SPLIT_CHARACTER + NAMESPACE
}

// DefaultEntityType returns the conventional type string customresource/<lower(group)>/<lower(kind)>.
// It does not apply automatically; EntityType must still be set on the config (Normalize requires it).
func DefaultEntityType(apiGroup, kind string) string {
	return fmt.Sprintf("customresource/%s/%s", strings.ToLower(strings.TrimSpace(apiGroup)), strings.ToLower(strings.TrimSpace(kind)))
}

// ToGVR returns the GroupVersionResource for the dynamic informer.
func (c *CustomResourceCollectorConfig) ToGVR() schema.GroupVersionResource {
	return schema.GroupVersionResource{
		Group:    c.APIGroup,
		Version:  c.APIVersion,
		Resource: c.Resource,
	}
}

// Normalize validates and fills PodLink defaults. EntityType must be non-empty. Call before RegisterCustomResourceCollector.
func (c *CustomResourceCollectorConfig) Normalize() error {
	c.APIGroup = strings.TrimSpace(c.APIGroup)
	c.APIVersion = strings.TrimSpace(c.APIVersion)
	c.Resource = strings.TrimSpace(c.Resource)
	c.Kind = strings.TrimSpace(c.Kind)
	c.EntityType = strings.TrimSpace(c.EntityType)
	c.Namespace2EntityRelation = strings.TrimSpace(c.Namespace2EntityRelation)

	if c.APIGroup == "" || c.APIVersion == "" || c.Resource == "" || c.Kind == "" {
		return fmt.Errorf("custom resource collector: APIGroup, APIVersion, Resource, and Kind are required")
	}
	if c.EntityType == "" {
		return fmt.Errorf("custom resource collector: EntityType is required")
	}
	if pl := c.PodLink; pl != nil {
		pl.OwnerKind = strings.TrimSpace(pl.OwnerKind)
		pl.OwnerAPIGroupContains = strings.TrimSpace(pl.OwnerAPIGroupContains)
		pl.PodLabelKey = strings.TrimSpace(pl.PodLabelKey)
		if pl.OwnerKind == "" {
			pl.OwnerKind = c.Kind
		}
		if pl.OwnerAPIGroupContains == "" {
			pl.OwnerAPIGroupContains = c.APIGroup
		}
	}
	return nil
}
