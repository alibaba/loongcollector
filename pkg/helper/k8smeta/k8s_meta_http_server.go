package k8smeta

import (
	"context"
	"encoding/json"
	"net/http"
	"os"
	"strconv"
	"strings"
	"time"

	v1 "k8s.io/api/core/v1"

	"github.com/alibaba/ilogtail/pkg/logger"
)

type requestBody struct {
	Keys []string `json:"keys"`
}

type metadataHandler struct {
	metaManager *MetaManager
}

func newMetadataHandler() *metadataHandler {
	metadataHandler := &metadataHandler{
		metaManager: GetMetaManagerInstance(),
	}
	return metadataHandler
}

func (m *metadataHandler) K8sServerRun(stopCh <-chan struct{}) error {
	defer panicRecover()
	portEnv := os.Getenv("KUBERNETES_METADATA_PORT")
	if len(portEnv) == 0 {
		portEnv = "9000"
	}
	port, err := strconv.Atoi(portEnv)
	if err != nil {
		port = 9000
	}
	server := &http.Server{ //nolint:gosec
		Addr: ":" + strconv.Itoa(port),
	}
	mux := http.NewServeMux()

	// TODO: add port in ip endpoint
	mux.HandleFunc("/metadata/ip", m.handlePodMetaByUniqueID)
	mux.HandleFunc("/metadata/containerid", m.handlePodMetaByUniqueID)
	mux.HandleFunc("/metadata/host", m.handlePodMetaByHostIP)
	server.Handler = mux
	for {
		if m.metaManager.IsReady() {
			break
		}
		time.Sleep(1 * time.Second)
		logger.Warning(context.Background(), "K8S_META_SERVER_WAIT", "waiting for k8s meta manager to be ready")
	}
	logger.Info(context.Background(), "k8s meta server", "started", "port", port)
	go func() {
		defer panicRecover()
		_ = server.ListenAndServe()
	}()
	<-stopCh
	return nil
}

func (m *metadataHandler) handlePodMetaByUniqueID(w http.ResponseWriter, r *http.Request) {
	defer r.Body.Close()
	var rBody requestBody
	// Decode the JSON data into the struct
	err := json.NewDecoder(r.Body).Decode(&rBody)
	if err != nil {
		http.Error(w, "Error parsing JSON: "+err.Error(), http.StatusBadRequest)
		return
	}

	// Get the metadata
	metadata := make(map[string]*PodMetadata)
	objs := m.metaManager.PodCache.Get(rBody.Keys)
	for key, obj := range objs {
		podMetadata := convertObj2PodMetadata(obj)
		if len(podMetadata) > 1 {
			logger.Warning(context.Background(), "Multiple pods found for unique ID", key)
		}
		if len(podMetadata) > 0 {
			metadata[key] = podMetadata[0]
		}
	}
	// Convert metadata to JSON
	metadataJSON, err := json.Marshal(metadata)
	if err != nil {
		http.Error(w, "Error converting metadata to JSON: "+err.Error(), http.StatusInternalServerError)
		return
	}
	// Set the response content type to application/json
	w.Header().Set("Content-Type", "application/json")
	// Write the metadata JSON to the response body
	_, err = w.Write(metadataJSON)
	if err != nil {
		http.Error(w, "Error writing response: "+err.Error(), http.StatusInternalServerError)
		return
	}
}

func (m *metadataHandler) handlePodMetaByHostIP(w http.ResponseWriter, r *http.Request) {
	defer r.Body.Close()
	var rBody requestBody
	// Decode the JSON data into the struct
	err := json.NewDecoder(r.Body).Decode(&rBody)
	if err != nil {
		http.Error(w, "Error parsing JSON: "+err.Error(), http.StatusBadRequest)
		return
	}

	// Get the metadata
	metadata := make(map[string]*PodMetadata)
	objs := m.metaManager.PodCache.Get(rBody.Keys)
	for _, obj := range objs {
		podMetadata := convertObj2PodMetadata(obj)
		for i, meta := range podMetadata {
			pod := obj[i].Raw.(*v1.Pod)
			metadata[pod.Status.PodIP] = meta
		}
	}
	// Convert metadata to JSON
	metadataJSON, err := json.Marshal(metadata)
	if err != nil {
		http.Error(w, "Error converting metadata to JSON: "+err.Error(), http.StatusInternalServerError)
		return
	}
	// Set the response content type to application/json
	w.Header().Set("Content-Type", "application/json")
	// Write the metadata JSON to the response body
	_, err = w.Write(metadataJSON)
	if err != nil {
		http.Error(w, "Error writing response: "+err.Error(), http.StatusInternalServerError)
		return
	}
}

func convertObj2PodMetadata(objs []*ObjectWrapper) []*PodMetadata {
	result := make([]*PodMetadata, 0)
	for _, obj := range objs {
		pod := obj.Raw.(*v1.Pod)
		images := make(map[string]string)
		for _, container := range pod.Spec.Containers {
			images[container.Name] = container.Image
		}
		envs := make(map[string]string)
		for _, container := range pod.Spec.Containers {
			for _, env := range container.Env {
				envs[env.Name] = env.Value
			}
		}
		podMetadata := &PodMetadata{
			Namespace: pod.Namespace,
			Labels:    pod.Labels,
			Images:    images,
			Envs:      envs,
			IsDeleted: false,
		}
		if len(pod.GetOwnerReferences()) == 0 {
			podMetadata.WorkloadName = ""
			podMetadata.WorkloadKind = ""
			logger.Warning(context.Background(), "Pod has no owner", pod.Name)
		} else {
			podMetadata.WorkloadName = pod.GetOwnerReferences()[0].Name
			podMetadata.WorkloadKind = strings.ToLower(pod.GetOwnerReferences()[0].Kind)
		}
		result = append(result, podMetadata)
	}
	return result
}
