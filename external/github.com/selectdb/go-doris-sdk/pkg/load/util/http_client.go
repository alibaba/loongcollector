package util

import (
	"crypto/tls"
	"net/http"
	"sync"
	"time"
)

var (
	client *http.Client
	once   sync.Once
)

func GetHttpClient() *http.Client {
	once.Do(func() {
		client = buildHttpClient()
	})
	return client
}

func buildHttpClient() *http.Client {

	transport := &http.Transport{
		MaxIdleConnsPerHost: 30, // 每个主机保持的空闲连接数，用于连接复用以减少建立连接的开销
		MaxConnsPerHost:     50, // 每个主机的最大总连接数(活跃+空闲)，控制并发数量，超出会排队等待
		MaxIdleConns:        50, // 全局最大空闲连接数

		// TLS configuration for Doris HTTP endpoints
		TLSClientConfig: &tls.Config{
			InsecureSkipVerify: true, // Allow insecure connections for Doris HTTP endpoints
		},
	}

	client := &http.Client{
		Transport: transport,
		Timeout:   120 * time.Second, // Total request timeout
	}

	return client
}
