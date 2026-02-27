package kubernetesmetav2

import (
	"encoding/json"
	"testing"

	"github.com/stretchr/testify/assert"
	app "k8s.io/api/apps/v1"
	v1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"

	"github.com/alibaba/ilogtail/pkg/helper/k8smeta"
	"github.com/alibaba/ilogtail/pkg/models"
	"github.com/alibaba/ilogtail/pkg/pipeline"
)

func TestEnableLabelsAndAnnotationsForPod(t *testing.T) {
	tests := []struct {
		name                string
		enableLabels        bool
		enableAnnotations   bool
		expectedLabels      bool
		expectedAnnotations bool
	}{
		{
			name:                "both disabled",
			enableLabels:        false,
			enableAnnotations:   false,
			expectedLabels:      false,
			expectedAnnotations: false,
		},
		{
			name:                "only labels enabled",
			enableLabels:        true,
			enableAnnotations:   false,
			expectedLabels:      true,
			expectedAnnotations: false,
		},
		{
			name:                "only annotations enabled",
			enableLabels:        false,
			enableAnnotations:   true,
			expectedLabels:      false,
			expectedAnnotations: true,
		},
		{
			name:                "both enabled",
			enableLabels:        true,
			enableAnnotations:   true,
			expectedLabels:      true,
			expectedAnnotations: true,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			obj := &v1.Pod{
				ObjectMeta: metav1.ObjectMeta{
					Name:      "test-pod",
					Namespace: "test-ns",
					UID:       "test-uid",
					Labels: map[string]string{
						"app":     "nginx",
						"version": "v1",
					},
					Annotations: map[string]string{
						"description": "test pod",
						"owner":       "team-a",
					},
				},
				Spec: v1.PodSpec{
					Containers: []v1.Container{
						{
							Name:  "nginx",
							Image: "nginx:latest",
						},
					},
				},
				Status: v1.PodStatus{
					Phase: v1.PodRunning,
					PodIP: "10.0.0.1",
				},
			}

			objWrapper := &k8smeta.ObjectWrapper{
				Raw: obj,
			}

			collector := &metaCollector{
				serviceK8sMeta: &ServiceK8sMeta{
					Interval:          10,
					EnableLabels:      tt.enableLabels,
					EnableAnnotations: tt.enableAnnotations,
				},
			}

			events := collector.processPodEntity(objWrapper, "create")
			assert.NotNil(t, events)
			assert.Len(t, events, 1)

			log := events[0].(*models.Log)

			// Check if labels are present based on configuration
			if tt.expectedLabels {
				labelsValue := log.Contents.Get("labels")
				assert.NotEmpty(t, labelsValue)

				// Convert interface{} to string first
				labelsStr, ok := labelsValue.(string)
				assert.True(t, ok, "labels should be string type")

				var labels map[string]string
				err := json.Unmarshal([]byte(labelsStr), &labels)
				assert.NoError(t, err)
				assert.Equal(t, "nginx", labels["app"])
				assert.Equal(t, "v1", labels["version"])
			} else {
				labelsValue := log.Contents.Get("labels")
				assert.Empty(t, labelsValue)
			}

			// Check if annotations are present based on configuration
			if tt.expectedAnnotations {
				annotationsValue := log.Contents.Get("annotations")
				assert.NotEmpty(t, annotationsValue)

				// Convert interface{} to string first
				annotationsStr, ok := annotationsValue.(string)
				assert.True(t, ok, "annotations should be string type")

				var annotations map[string]string
				err := json.Unmarshal([]byte(annotationsStr), &annotations)
				assert.NoError(t, err)
				assert.Equal(t, "test pod", annotations["description"])
				assert.Equal(t, "team-a", annotations["owner"])
			} else {
				annotationsValue := log.Contents.Get("annotations")
				assert.Empty(t, annotationsValue)
			}
		})
	}
}

func TestEnableLabelsAndAnnotationsForService(t *testing.T) {
	tests := []struct {
		name                string
		enableLabels        bool
		enableAnnotations   bool
		expectedLabels      bool
		expectedAnnotations bool
	}{
		{
			name:                "both disabled",
			enableLabels:        false,
			enableAnnotations:   false,
			expectedLabels:      false,
			expectedAnnotations: false,
		},
		{
			name:                "only labels enabled",
			enableLabels:        true,
			enableAnnotations:   false,
			expectedLabels:      true,
			expectedAnnotations: false,
		},
		{
			name:                "only annotations enabled",
			enableLabels:        false,
			enableAnnotations:   true,
			expectedLabels:      false,
			expectedAnnotations: true,
		},
		{
			name:                "both enabled",
			enableLabels:        true,
			enableAnnotations:   true,
			expectedLabels:      true,
			expectedAnnotations: true,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			obj := &v1.Service{
				ObjectMeta: metav1.ObjectMeta{
					Name:      "test-service",
					Namespace: "test-ns",
					UID:       "test-uid",
					Labels: map[string]string{
						"app":  "web",
						"tier": "frontend",
					},
					Annotations: map[string]string{
						"description": "test service",
						"monitoring":  "enabled",
					},
				},
				Spec: v1.ServiceSpec{
					Type: v1.ServiceTypeClusterIP,
					Ports: []v1.ServicePort{
						{
							Port: 80,
						},
					},
				},
			}

			objWrapper := &k8smeta.ObjectWrapper{
				Raw: obj,
			}

			collector := &metaCollector{
				serviceK8sMeta: &ServiceK8sMeta{
					Interval:          10,
					EnableLabels:      tt.enableLabels,
					EnableAnnotations: tt.enableAnnotations,
				},
			}

			events := collector.processServiceEntity(objWrapper, "create")
			assert.NotNil(t, events)
			assert.Len(t, events, 1)

			log := events[0].(*models.Log)

			// Check if labels are present based on configuration
			if tt.expectedLabels {
				labelsValue := log.Contents.Get("labels")
				assert.NotEmpty(t, labelsValue)

				// Convert interface{} to string first
				labelsStr, ok := labelsValue.(string)
				assert.True(t, ok, "labels should be string type")

				var labels map[string]string
				err := json.Unmarshal([]byte(labelsStr), &labels)
				assert.NoError(t, err)
				assert.Equal(t, "web", labels["app"])
				assert.Equal(t, "frontend", labels["tier"])
			} else {
				labelsValue := log.Contents.Get("labels")
				assert.Empty(t, labelsValue)
			}

			// Check if annotations are present based on configuration
			if tt.expectedAnnotations {
				annotationsValue := log.Contents.Get("annotations")
				assert.NotEmpty(t, annotationsValue)

				// Convert interface{} to string first
				annotationsStr, ok := annotationsValue.(string)
				assert.True(t, ok, "annotations should be string type")

				var annotations map[string]string
				err := json.Unmarshal([]byte(annotationsStr), &annotations)
				assert.NoError(t, err)
				assert.Equal(t, "test service", annotations["description"])
				assert.Equal(t, "enabled", annotations["monitoring"])
			} else {
				annotationsValue := log.Contents.Get("annotations")
				assert.Empty(t, annotationsValue)
			}
		})
	}
}

func TestEnableLabelsAndAnnotationsForNode(t *testing.T) {
	tests := []struct {
		name                string
		enableLabels        bool
		enableAnnotations   bool
		expectedLabels      bool
		expectedAnnotations bool
	}{
		{
			name:                "both disabled",
			enableLabels:        false,
			enableAnnotations:   false,
			expectedLabels:      false,
			expectedAnnotations: false,
		},
		{
			name:                "only labels enabled",
			enableLabels:        true,
			enableAnnotations:   false,
			expectedLabels:      true,
			expectedAnnotations: false,
		},
		{
			name:                "only annotations enabled",
			enableLabels:        false,
			enableAnnotations:   true,
			expectedLabels:      false,
			expectedAnnotations: true,
		},
		{
			name:                "both enabled",
			enableLabels:        true,
			enableAnnotations:   true,
			expectedLabels:      true,
			expectedAnnotations: true,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			obj := &v1.Node{
				ObjectMeta: metav1.ObjectMeta{
					Name: "test-node",
					UID:  "test-uid",
					Labels: map[string]string{
						"node-role": "worker",
						"zone":      "us-west-1",
					},
					Annotations: map[string]string{
						"description": "test worker node",
						"maintenance": "scheduled",
					},
				},
				Status: v1.NodeStatus{
					Conditions: []v1.NodeCondition{
						{
							Type:   v1.NodeReady,
							Status: v1.ConditionTrue,
						},
					},
					Addresses: []v1.NodeAddress{
						{
							Type:    v1.NodeInternalIP,
							Address: "10.0.0.10",
						},
					},
				},
			}

			objWrapper := &k8smeta.ObjectWrapper{
				Raw: obj,
			}

			collector := &metaCollector{
				serviceK8sMeta: &ServiceK8sMeta{
					Interval:          10,
					EnableLabels:      tt.enableLabels,
					EnableAnnotations: tt.enableAnnotations,
				},
			}

			events := collector.processNodeEntity(objWrapper, "create")
			assert.NotNil(t, events)
			assert.Len(t, events, 2) // Node entity + infra server link

			log := events[0].(*models.Log)

			// Check if labels are present based on configuration
			if tt.expectedLabels {
				labelsValue := log.Contents.Get("labels")
				assert.NotEmpty(t, labelsValue)

				// Convert interface{} to string first
				labelsStr, ok := labelsValue.(string)
				assert.True(t, ok, "labels should be string type")

				var labels map[string]string
				err := json.Unmarshal([]byte(labelsStr), &labels)
				assert.NoError(t, err)
				assert.Equal(t, "worker", labels["node-role"])
				assert.Equal(t, "us-west-1", labels["zone"])
			} else {
				labelsValue := log.Contents.Get("labels")
				assert.Empty(t, labelsValue)
			}

			// Check if annotations are present based on configuration
			if tt.expectedAnnotations {
				annotationsValue := log.Contents.Get("annotations")
				assert.NotEmpty(t, annotationsValue)

				// Convert interface{} to string first
				annotationsStr, ok := annotationsValue.(string)
				assert.True(t, ok, "annotations should be string type")

				var annotations map[string]string
				err := json.Unmarshal([]byte(annotationsStr), &annotations)
				assert.NoError(t, err)
				assert.Equal(t, "test worker node", annotations["description"])
				assert.Equal(t, "scheduled", annotations["maintenance"])
			} else {
				annotationsValue := log.Contents.Get("annotations")
				assert.Empty(t, annotationsValue)
			}
		})
	}
}

func TestEnableLabelsAndAnnotationsForConfigMap(t *testing.T) {
	tests := []struct {
		name                string
		enableLabels        bool
		enableAnnotations   bool
		expectedLabels      bool
		expectedAnnotations bool
	}{
		{
			name:                "both disabled",
			enableLabels:        false,
			enableAnnotations:   false,
			expectedLabels:      false,
			expectedAnnotations: false,
		},
		{
			name:                "only labels enabled",
			enableLabels:        true,
			enableAnnotations:   false,
			expectedLabels:      true,
			expectedAnnotations: false,
		},
		{
			name:                "only annotations enabled",
			enableLabels:        false,
			enableAnnotations:   true,
			expectedLabels:      false,
			expectedAnnotations: true,
		},
		{
			name:                "both enabled",
			enableLabels:        true,
			enableAnnotations:   true,
			expectedLabels:      true,
			expectedAnnotations: true,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			obj := &v1.ConfigMap{
				ObjectMeta: metav1.ObjectMeta{
					Name:      "test-config",
					Namespace: "test-ns",
					UID:       "test-uid",
					Labels: map[string]string{
						"app": "config",
						"env": "prod",
					},
					Annotations: map[string]string{
						"description": "test config map",
						"version":     "1.0.0",
					},
				},
				Data: map[string]string{
					"key1": "value1",
					"key2": "value2",
				},
			}

			objWrapper := &k8smeta.ObjectWrapper{
				Raw: obj,
			}

			collector := &metaCollector{
				serviceK8sMeta: &ServiceK8sMeta{
					Interval:          10,
					EnableLabels:      tt.enableLabels,
					EnableAnnotations: tt.enableAnnotations,
				},
			}

			events := collector.processConfigMapEntity(objWrapper, "create")
			assert.NotNil(t, events)
			assert.Len(t, events, 1)

			log := events[0].(*models.Log)

			// Check if labels are present based on configuration
			if tt.expectedLabels {
				labelsValue := log.Contents.Get("labels")
				assert.NotEmpty(t, labelsValue)

				// Convert interface{} to string first
				labelsStr, ok := labelsValue.(string)
				assert.True(t, ok, "labels should be string type")

				var labels map[string]string
				err := json.Unmarshal([]byte(labelsStr), &labels)
				assert.NoError(t, err)
				assert.Equal(t, "config", labels["app"])
				assert.Equal(t, "prod", labels["env"])
			} else {
				labelsValue := log.Contents.Get("labels")
				assert.Empty(t, labelsValue)
			}

			// Check if annotations are present based on configuration
			if tt.expectedAnnotations {
				annotationsValue := log.Contents.Get("annotations")
				assert.NotEmpty(t, annotationsValue)

				// Convert interface{} to string first
				annotationsStr, ok := annotationsValue.(string)
				assert.True(t, ok, "annotations should be string type")

				var annotations map[string]string
				err := json.Unmarshal([]byte(annotationsStr), &annotations)
				assert.NoError(t, err)
				assert.Equal(t, "test config map", annotations["description"])
				assert.Equal(t, "1.0.0", annotations["version"])
			} else {
				annotationsValue := log.Contents.Get("annotations")
				assert.Empty(t, annotationsValue)
			}
		})
	}
}

func TestEnableLabelsAndAnnotationsForDeployment(t *testing.T) {
	tests := []struct {
		name                string
		enableLabels        bool
		enableAnnotations   bool
		expectedLabels      bool
		expectedAnnotations bool
	}{
		{
			name:                "both disabled",
			enableLabels:        false,
			enableAnnotations:   false,
			expectedLabels:      false,
			expectedAnnotations: false,
		},
		{
			name:                "only labels enabled",
			enableLabels:        true,
			enableAnnotations:   false,
			expectedLabels:      true,
			expectedAnnotations: false,
		},
		{
			name:                "only annotations enabled",
			enableLabels:        false,
			enableAnnotations:   true,
			expectedLabels:      false,
			expectedAnnotations: true,
		},
		{
			name:                "both enabled",
			enableLabels:        true,
			enableAnnotations:   true,
			expectedLabels:      true,
			expectedAnnotations: true,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			obj := &app.Deployment{
				ObjectMeta: metav1.ObjectMeta{
					Name:      "test-deployment",
					Namespace: "test-ns",
					UID:       "test-uid",
					Labels: map[string]string{
						"app":     "web",
						"version": "v2",
					},
					Annotations: map[string]string{
						"description": "test deployment",
						"strategy":    "rolling",
					},
				},
				Spec: app.DeploymentSpec{
					Replicas: func() *int32 { i := int32(3); return &i }(),
					Selector: &metav1.LabelSelector{
						MatchLabels: map[string]string{
							"app": "web",
						},
					},
					Template: v1.PodTemplateSpec{
						Spec: v1.PodSpec{
							Containers: []v1.Container{
								{
									Name:  "web",
									Image: "nginx:latest",
								},
							},
						},
					},
				},
			}

			objWrapper := &k8smeta.ObjectWrapper{
				Raw: obj,
			}

			collector := &metaCollector{
				serviceK8sMeta: &ServiceK8sMeta{
					Interval:          10,
					EnableLabels:      tt.enableLabels,
					EnableAnnotations: tt.enableAnnotations,
				},
			}

			events := collector.processDeploymentEntity(objWrapper, "create")
			assert.NotNil(t, events)
			assert.Len(t, events, 1)

			log := events[0].(*models.Log)

			// Check if labels are present based on configuration
			if tt.expectedLabels {
				labelsValue := log.Contents.Get("labels")
				assert.NotEmpty(t, labelsValue)

				// Convert interface{} to string first
				labelsStr, ok := labelsValue.(string)
				assert.True(t, ok, "labels should be string type")

				var labels map[string]string
				err := json.Unmarshal([]byte(labelsStr), &labels)
				assert.NoError(t, err)
				assert.Equal(t, "web", labels["app"])
				assert.Equal(t, "v2", labels["version"])
			} else {
				labelsValue := log.Contents.Get("labels")
				assert.Empty(t, labelsValue)
			}

			// Check if annotations are present based on configuration
			if tt.expectedAnnotations {
				annotationsValue := log.Contents.Get("annotations")
				assert.NotEmpty(t, annotationsValue)

				// Convert interface{} to string first
				annotationsStr, ok := annotationsValue.(string)
				assert.True(t, ok, "annotations should be string type")

				var annotations map[string]string
				err := json.Unmarshal([]byte(annotationsStr), &annotations)
				assert.NoError(t, err)
				assert.Equal(t, "test deployment", annotations["description"])
				assert.Equal(t, "rolling", annotations["strategy"])
			} else {
				annotationsValue := log.Contents.Get("annotations")
				assert.Empty(t, annotationsValue)
			}
		})
	}
}

func TestServiceK8sMetaDefaultValues(t *testing.T) {
	// Test that the default values are correctly set
	service := &ServiceK8sMeta{}

	// These should be false by default
	assert.False(t, service.EnableLabels)
	assert.False(t, service.EnableAnnotations)
}

func TestServiceK8sMetaInitFunction(t *testing.T) {
	// Test that the init function sets the correct default values
	service := pipeline.ServiceInputs["service_kubernetes_meta"]()

	// Cast to the correct type
	if k8sMeta, ok := service.(*ServiceK8sMeta); ok {
		assert.False(t, k8sMeta.EnableLabels)
		assert.False(t, k8sMeta.EnableAnnotations)
	} else {
		t.Fatal("Failed to cast service to ServiceK8sMeta")
	}
}
