// Code generated by protoc-gen-go-grpc. DO NOT EDIT.

package v3

import (
	context "context"
	v3 "github.com/alibaba/ilogtail/plugins/input/skywalkingv3/skywalking/network/common/v3"
	grpc "google.golang.org/grpc"
	codes "google.golang.org/grpc/codes"
	status "google.golang.org/grpc/status"
)

// This is a compile-time assertion to ensure that this generated file
// is compatible with the grpc package it is being compiled against.
// Requires gRPC-Go v1.32.0 or later.
const _ = grpc.SupportPackageIsVersion7

// ConfigurationDiscoveryServiceClient is the client API for ConfigurationDiscoveryService service.
//
// For semantics around ctx use and closing/ending streaming RPCs, please refer to https://pkg.go.dev/google.golang.org/grpc/?tab=doc#ClientConn.NewStream.
type ConfigurationDiscoveryServiceClient interface {
	// fetchConfigurations service requests the latest configuration.
	// Expect command of Commands is:
	//    command: CDS # meaning ConfigurationDiscoveryService's response
	//    args: Include string key and string value pair.
	//          The key depends on the agent implementation.
	//          The value is the latest value in String value. The watcher of key owner takes the responsibility to convert it to the correct type or format.
	//          One reserved key is `UUID`. The value would help reducing the traffic load between agent and OAP if there is no change.
	// Commands could be empty if no change detected based on ConfigurationSyncRequest.
	FetchConfigurations(ctx context.Context, in *ConfigurationSyncRequest, opts ...grpc.CallOption) (*v3.Commands, error)
}

type configurationDiscoveryServiceClient struct {
	cc grpc.ClientConnInterface
}

func NewConfigurationDiscoveryServiceClient(cc grpc.ClientConnInterface) ConfigurationDiscoveryServiceClient {
	return &configurationDiscoveryServiceClient{cc}
}

func (c *configurationDiscoveryServiceClient) FetchConfigurations(ctx context.Context, in *ConfigurationSyncRequest, opts ...grpc.CallOption) (*v3.Commands, error) {
	out := new(v3.Commands)
	err := c.cc.Invoke(ctx, "/skywalking.v3.ConfigurationDiscoveryService/fetchConfigurations", in, out, opts...)
	if err != nil {
		return nil, err
	}
	return out, nil
}

// ConfigurationDiscoveryServiceServer is the server API for ConfigurationDiscoveryService service.
// All implementations should embed UnimplementedConfigurationDiscoveryServiceServer
// for forward compatibility
type ConfigurationDiscoveryServiceServer interface {
	// fetchConfigurations service requests the latest configuration.
	// Expect command of Commands is:
	//    command: CDS # meaning ConfigurationDiscoveryService's response
	//    args: Include string key and string value pair.
	//          The key depends on the agent implementation.
	//          The value is the latest value in String value. The watcher of key owner takes the responsibility to convert it to the correct type or format.
	//          One reserved key is `UUID`. The value would help reducing the traffic load between agent and OAP if there is no change.
	// Commands could be empty if no change detected based on ConfigurationSyncRequest.
	FetchConfigurations(context.Context, *ConfigurationSyncRequest) (*v3.Commands, error)
}

// UnimplementedConfigurationDiscoveryServiceServer should be embedded to have forward compatible implementations.
type UnimplementedConfigurationDiscoveryServiceServer struct {
}

func (UnimplementedConfigurationDiscoveryServiceServer) FetchConfigurations(context.Context, *ConfigurationSyncRequest) (*v3.Commands, error) {
	return nil, status.Errorf(codes.Unimplemented, "method FetchConfigurations not implemented")
}

// UnsafeConfigurationDiscoveryServiceServer may be embedded to opt out of forward compatibility for this service.
// Use of this interface is not recommended, as added methods to ConfigurationDiscoveryServiceServer will
// result in compilation errors.
type UnsafeConfigurationDiscoveryServiceServer interface {
	mustEmbedUnimplementedConfigurationDiscoveryServiceServer()
}

func RegisterConfigurationDiscoveryServiceServer(s grpc.ServiceRegistrar, srv ConfigurationDiscoveryServiceServer) {
	s.RegisterService(&ConfigurationDiscoveryService_ServiceDesc, srv)
}

func _ConfigurationDiscoveryService_FetchConfigurations_Handler(srv interface{}, ctx context.Context, dec func(interface{}) error, interceptor grpc.UnaryServerInterceptor) (interface{}, error) {
	in := new(ConfigurationSyncRequest)
	if err := dec(in); err != nil {
		return nil, err
	}
	if interceptor == nil {
		return srv.(ConfigurationDiscoveryServiceServer).FetchConfigurations(ctx, in)
	}
	info := &grpc.UnaryServerInfo{
		Server:     srv,
		FullMethod: "/skywalking.v3.ConfigurationDiscoveryService/fetchConfigurations",
	}
	handler := func(ctx context.Context, req interface{}) (interface{}, error) {
		return srv.(ConfigurationDiscoveryServiceServer).FetchConfigurations(ctx, req.(*ConfigurationSyncRequest))
	}
	return interceptor(ctx, in, info, handler)
}

// ConfigurationDiscoveryService_ServiceDesc is the grpc.ServiceDesc for ConfigurationDiscoveryService service.
// It's only intended for direct use with grpc.RegisterService,
// and not to be introspected or modified (even as a copy)
var ConfigurationDiscoveryService_ServiceDesc = grpc.ServiceDesc{
	ServiceName: "skywalking.v3.ConfigurationDiscoveryService",
	HandlerType: (*ConfigurationDiscoveryServiceServer)(nil),
	Methods: []grpc.MethodDesc{
		{
			MethodName: "fetchConfigurations",
			Handler:    _ConfigurationDiscoveryService_FetchConfigurations_Handler,
		},
	},
	Streams:  []grpc.StreamDesc{},
	Metadata: "language-agent/ConfigurationDiscoveryService.proto",
}
