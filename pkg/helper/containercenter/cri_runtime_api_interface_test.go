// Copyright 2025 iLogtail Authors
// Licensed under Apache License, Version 2.0 (the "License")

package containercenter

import (
	"context"
	"fmt"
	"net"
	"sync"
	"testing"
	"time"

	"github.com/stretchr/testify/assert"
	"google.golang.org/grpc"
	"google.golang.org/grpc/test/bufconn"
	"google.golang.org/protobuf/runtime/protoimpl"
)

// 测试服务端实现
type testRuntimeServiceServer struct {
	mu sync.Mutex

	// 控制返回值
	versionResp *CriVersionResponse
}

func (s *testRuntimeServiceServer) Version(ctx context.Context) (*CriVersionResponse, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	if s.versionResp.RuntimeAPIVersion == "v1" {
		return s.versionResp, nil
	} else if s.versionResp.RuntimeAPIVersion == "v1alpha2" {
		return s.versionResp, nil
	}
	return nil, fmt.Errorf("failed to initialize RuntimeServiceClient")
}

func (s *testRuntimeServiceServer) ListContainers(ctx context.Context) (*CriListContainersResponse, error) {
	return nil, nil
}

func (s *testRuntimeServiceServer) ContainerStatus(ctx context.Context, containerID string, verbose bool) (*CriContainerStatusResponse, error) {
	return nil, nil
}

func (s *testRuntimeServiceServer) ListPodSandbox(ctx context.Context) (*CriListPodSandboxResponse, error) {
	return nil, nil
}

func (s *testRuntimeServiceServer) PodSandboxStatus(ctx context.Context, sandboxID string, verbose bool) (*CriPodSandboxStatusResponse, error) {
	return nil, nil
}

func (m *CriVersionResponse) Descriptor() ([]byte, []int) {
	return protoimpl.X.CompressGZIP([]byte{
		0x0a, 0x0a, 0x74, 0x65, 0x73, 0x74, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x12, 0x04, 0x74, 0x65,
		0x73, 0x74, 0x22, 0xa6, 0x01, 0x0a, 0x12, 0x43, 0x72, 0x69, 0x56, 0x65, 0x72, 0x73, 0x69, 0x6f,
		0x6e, 0x52, 0x65, 0x73, 0x70, 0x6f, 0x6e, 0x73, 0x65, 0x12, 0x18, 0x0a, 0x07, 0x56, 0x65, 0x72,
		0x73, 0x69, 0x6f, 0x6e, 0x18, 0x01, 0x20, 0x01, 0x28, 0x09, 0x52, 0x07, 0x56, 0x65, 0x72, 0x73,
		0x69, 0x6f, 0x6e, 0x12, 0x20, 0x0a, 0x0b, 0x52, 0x75, 0x6e, 0x74, 0x69, 0x6d, 0x65, 0x4e, 0x61,
		0x6d, 0x65, 0x18, 0x02, 0x20, 0x01, 0x28, 0x09, 0x52, 0x0b, 0x52, 0x75, 0x6e, 0x74, 0x69, 0x6d,
		0x65, 0x4e, 0x61, 0x6d, 0x65, 0x12, 0x26, 0x0a, 0x0e, 0x52, 0x75, 0x6e, 0x74, 0x69, 0x6d, 0x65,
		0x56, 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0x18, 0x03, 0x20, 0x01, 0x28, 0x09, 0x52, 0x0e, 0x52,
		0x75, 0x6e, 0x74, 0x69, 0x6d, 0x65, 0x56, 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0x12, 0x2c, 0x0a,
		0x11, 0x52, 0x75, 0x6e, 0x74, 0x69, 0x6d, 0x65, 0x41, 0x50, 0x49, 0x56, 0x65, 0x72, 0x73, 0x69,
		0x6f, 0x6e, 0x18, 0x04, 0x20, 0x01, 0x28, 0x09, 0x52, 0x11, 0x52, 0x75, 0x6e, 0x74, 0x69, 0x6d,
		0x65, 0x41, 0x50, 0x49, 0x56, 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0x42, 0x08, 0x5a, 0x06, 0x2e,
		0x2f, 0x74, 0x65, 0x73, 0x74, 0x62, 0x06, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x33,
	}), []int{0}
}

func (m *CriVersionResponse) Reset() {
	*m = CriVersionResponse{}
}

func (m *CriVersionResponse) String() string {
	return fmt.Sprintf("{Version: %s, RuntimeName: %s, RuntimeVersion: %s, RuntimeAPIVersion: %s}",
		m.Version, m.RuntimeName, m.RuntimeVersion, m.RuntimeAPIVersion)
}

func (m *CriVersionResponse) ProtoMessage() {}

// 创建测试服务端
func createTestServer(t *testing.T, serviceName string) (*testRuntimeServiceServer, *bufconn.Listener) {
	listener := bufconn.Listen(1024 * 1024)
	server := grpc.NewServer()

	testServer := &testRuntimeServiceServer{}
	serviceDesc := grpc.ServiceDesc{
		ServiceName: serviceName,
		HandlerType: (*RuntimeService)(nil),
		Methods: []grpc.MethodDesc{
			{
				MethodName: "Version",
				Handler: func(srv interface{}, ctx context.Context, dec func(interface{}) error, interceptor grpc.UnaryServerInterceptor) (interface{}, error) {
					return srv.(*testRuntimeServiceServer).Version(ctx)
				},
			},
			{
				MethodName: "ListContainers",
				Handler: func(srv interface{}, ctx context.Context, dec func(interface{}) error, interceptor grpc.UnaryServerInterceptor) (interface{}, error) {
					return srv.(*testRuntimeServiceServer).ListContainers(ctx)
				},
			},
			{
				MethodName: "ContainerStatus",
				Handler: func(srv interface{}, ctx context.Context, dec func(interface{}) error, interceptor grpc.UnaryServerInterceptor) (interface{}, error) {
					return srv.(*testRuntimeServiceServer).ContainerStatus(ctx, "", false)
				},
			},
			{
				MethodName: "ListPodSandbox",
				Handler: func(srv interface{}, ctx context.Context, dec func(interface{}) error, interceptor grpc.UnaryServerInterceptor) (interface{}, error) {
					return srv.(*testRuntimeServiceServer).ListPodSandbox(ctx)
				},
			},
			{
				MethodName: "PodSandboxStatus",
				Handler: func(srv interface{}, ctx context.Context, dec func(interface{}) error, interceptor grpc.UnaryServerInterceptor) (interface{}, error) {
					return srv.(*testRuntimeServiceServer).PodSandboxStatus(ctx, "", false)
				},
			},
		},
		Streams:  []grpc.StreamDesc{},
		Metadata: "manual",
	}

	server.RegisterService(&serviceDesc, testServer)

	go func() {
		if err := server.Serve(listener); err != nil {
			t.Errorf("Server serve error: %v", err)
		}
	}()

	t.Cleanup(func() {
		server.Stop()
	})

	return testServer, listener
}

func TestNewRuntimeServiceClient(t *testing.T) {
	// 准备测试数据
	v1Resp := &CriVersionResponse{
		Version:           "0.1.0",
		RuntimeName:       "containerd",
		RuntimeVersion:    "v2.0.5",
		RuntimeAPIVersion: "v1",
	}

	v1alpha2Resp := &CriVersionResponse{
		Version:           "0.1.0",
		RuntimeName:       "containerd",
		RuntimeVersion:    "v1.6.0",
		RuntimeAPIVersion: "v1alpha2",
	}

	invalidResp := &CriVersionResponse{
		Version:           "0.1.0",
		RuntimeName:       "containerd",
		RuntimeVersion:    "v0.0.0",
		RuntimeAPIVersion: "invalid",
	}

	tests := []struct {
		name            string
		serviceName     string
		expectError     bool
		expectedVersion string
		setupServer     func(*testRuntimeServiceServer) // 自定义服务端配置
	}{
		{
			name:            "V1_Success",
			serviceName:     "runtime.v1.RuntimeService",
			expectError:     false,
			expectedVersion: "v1",
			setupServer: func(s *testRuntimeServiceServer) {
				s.versionResp = v1Resp
			},
		},
		{
			name:            "V1Alpha2_Success",
			serviceName:     "runtime.v1alpha2.RuntimeService",
			expectError:     false,
			expectedVersion: "v1alpha2",
			setupServer: func(s *testRuntimeServiceServer) {
				s.versionResp = v1alpha2Resp
			},
		},
		{
			name:            "Both_Failed",
			serviceName:     "invalid.service.name",
			expectError:     true,
			expectedVersion: "invalid",
			setupServer: func(s *testRuntimeServiceServer) {
				s.versionResp = invalidResp
			},
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			// 创建测试服务端
			testServer, listener := createTestServer(t, tt.serviceName)

			// 应用自定义配置
			if tt.setupServer != nil {
				tt.setupServer(testServer)
			}

			// 替换地址解析函数
			getAddressAndDialer = func(socket string) (string, func(string, time.Duration) (net.Conn, error), error) {
				return "bufnet", func(addr string, timeout time.Duration) (net.Conn, error) {
					return listener.Dial()
				}, nil
			}

			// 替换拨号函数
			grpcDialContext = func(ctx context.Context, addr string, opts ...grpc.DialOption) (*grpc.ClientConn, error) {
				return grpc.DialContext(ctx, "bufnet", opts...)
			}

			// 执行测试
			client, err := NewRuntimeServiceClient(time.Second, 1024*1024)

			// 验证结果
			if tt.expectError {
				assert.Error(t, err)
				assert.Nil(t, client)
			} else {
				assert.NoError(t, err)
				assert.NotNil(t, client)
				assert.Equal(t, tt.expectedVersion, client.info.RuntimeAPIVersion)
			}

			// 清理
			if client != nil {
				client.Close()
			}
		})
	}
}
