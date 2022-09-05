// Code generated by protoc-gen-go. DO NOT EDIT.
// versions:
// 	protoc-gen-go v1.28.1
// 	protoc        v3.21.5
// source: model.proto

package ___

import (
	protoreflect "google.golang.org/protobuf/reflect/protoreflect"
	protoimpl "google.golang.org/protobuf/runtime/protoimpl"
	reflect "reflect"
	sync "sync"
)

const (
	// Verify that this generated code is sufficiently up-to-date.
	_ = protoimpl.EnforceVersion(20 - protoimpl.MinVersion)
	// Verify that runtime/protoimpl is sufficiently up-to-date.
	_ = protoimpl.EnforceVersion(protoimpl.MaxVersion - 20)
)

type Agent struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	AgentId      string            `protobuf:"bytes,1,opt,name=AgentId,json=instance_id,proto3" json:"AgentId,omitempty"`
	Ip           string            `protobuf:"bytes,2,opt,name=Ip,json=ip,proto3" json:"Ip,omitempty"`
	State        string            `protobuf:"bytes,3,opt,name=State,json=state,proto3" json:"State,omitempty"`
	Region       string            `protobuf:"bytes,4,opt,name=Region,json=region,proto3" json:"Region,omitempty"`
	StartUpAt    int64             `protobuf:"varint,5,opt,name=StartUpAt,json=startup_at,proto3" json:"StartUpAt,omitempty"`
	Env          string            `protobuf:"bytes,6,opt,name=Env,json=env,proto3" json:"Env,omitempty"`
	Version      string            `protobuf:"bytes,7,opt,name=Version,json=version,proto3" json:"Version,omitempty"`
	ConnectState string            `protobuf:"bytes,8,opt,name=ConnectState,json=connect_state,proto3" json:"ConnectState,omitempty"`
	Heartbeat    string            `protobuf:"bytes,9,opt,name=Heartbeat,json=heartbeat,proto3" json:"Heartbeat,omitempty"`
	Tag          map[string]string `protobuf:"bytes,10,rep,name=Tag,json=tags,proto3" json:"Tag,omitempty" protobuf_key:"bytes,1,opt,name=key,proto3" protobuf_val:"bytes,2,opt,name=value,proto3"`
	Status       map[string]string `protobuf:"bytes,11,rep,name=Status,json=status,proto3" json:"Status,omitempty" protobuf_key:"bytes,1,opt,name=key,proto3" protobuf_val:"bytes,2,opt,name=value,proto3"`
	Progress     map[string]string `protobuf:"bytes,12,rep,name=Progress,json=progress,proto3" json:"Progress,omitempty" protobuf_key:"bytes,1,opt,name=key,proto3" protobuf_val:"bytes,2,opt,name=value,proto3"`
}

func (x *Agent) Reset() {
	*x = Agent{}
	if protoimpl.UnsafeEnabled {
		mi := &file_model_proto_msgTypes[0]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *Agent) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*Agent) ProtoMessage() {}

func (x *Agent) ProtoReflect() protoreflect.Message {
	mi := &file_model_proto_msgTypes[0]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use Agent.ProtoReflect.Descriptor instead.
func (*Agent) Descriptor() ([]byte, []int) {
	return file_model_proto_rawDescGZIP(), []int{0}
}

func (x *Agent) GetAgentId() string {
	if x != nil {
		return x.AgentId
	}
	return ""
}

func (x *Agent) GetIp() string {
	if x != nil {
		return x.Ip
	}
	return ""
}

func (x *Agent) GetState() string {
	if x != nil {
		return x.State
	}
	return ""
}

func (x *Agent) GetRegion() string {
	if x != nil {
		return x.Region
	}
	return ""
}

func (x *Agent) GetStartUpAt() int64 {
	if x != nil {
		return x.StartUpAt
	}
	return 0
}

func (x *Agent) GetEnv() string {
	if x != nil {
		return x.Env
	}
	return ""
}

func (x *Agent) GetVersion() string {
	if x != nil {
		return x.Version
	}
	return ""
}

func (x *Agent) GetConnectState() string {
	if x != nil {
		return x.ConnectState
	}
	return ""
}

func (x *Agent) GetHeartbeat() string {
	if x != nil {
		return x.Heartbeat
	}
	return ""
}

func (x *Agent) GetTag() map[string]string {
	if x != nil {
		return x.Tag
	}
	return nil
}

func (x *Agent) GetStatus() map[string]string {
	if x != nil {
		return x.Status
	}
	return nil
}

func (x *Agent) GetProgress() map[string]string {
	if x != nil {
		return x.Progress
	}
	return nil
}

type AgentAlarm struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	AlarmKey     string `protobuf:"bytes,1,opt,name=AlarmKey,json=alarm_key,proto3" json:"AlarmKey,omitempty"`
	AlarmTime    string `protobuf:"bytes,2,opt,name=AlarmTime,json=alarm_time,proto3" json:"AlarmTime,omitempty"`
	AlarmType    string `protobuf:"bytes,3,opt,name=AlarmType,json=alarm_type,proto3" json:"AlarmType,omitempty"`
	AlarmMessage string `protobuf:"bytes,4,opt,name=AlarmMessage,json=alarm_message,proto3" json:"AlarmMessage,omitempty"`
}

func (x *AgentAlarm) Reset() {
	*x = AgentAlarm{}
	if protoimpl.UnsafeEnabled {
		mi := &file_model_proto_msgTypes[1]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *AgentAlarm) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*AgentAlarm) ProtoMessage() {}

func (x *AgentAlarm) ProtoReflect() protoreflect.Message {
	mi := &file_model_proto_msgTypes[1]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use AgentAlarm.ProtoReflect.Descriptor instead.
func (*AgentAlarm) Descriptor() ([]byte, []int) {
	return file_model_proto_rawDescGZIP(), []int{1}
}

func (x *AgentAlarm) GetAlarmKey() string {
	if x != nil {
		return x.AlarmKey
	}
	return ""
}

func (x *AgentAlarm) GetAlarmTime() string {
	if x != nil {
		return x.AlarmTime
	}
	return ""
}

func (x *AgentAlarm) GetAlarmType() string {
	if x != nil {
		return x.AlarmType
	}
	return ""
}

func (x *AgentAlarm) GetAlarmMessage() string {
	if x != nil {
		return x.AlarmMessage
	}
	return ""
}

type AgentStatus struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	AgentId  string            `protobuf:"bytes,1,opt,name=AgentId,json=instance_id,proto3" json:"AgentId,omitempty"`
	Status   map[string]string `protobuf:"bytes,2,rep,name=Status,json=status,proto3" json:"Status,omitempty" protobuf_key:"bytes,1,opt,name=key,proto3" protobuf_val:"bytes,2,opt,name=value,proto3"`
	Progress map[string]string `protobuf:"bytes,3,rep,name=Progress,json=progress,proto3" json:"Progress,omitempty" protobuf_key:"bytes,1,opt,name=key,proto3" protobuf_val:"bytes,2,opt,name=value,proto3"`
}

func (x *AgentStatus) Reset() {
	*x = AgentStatus{}
	if protoimpl.UnsafeEnabled {
		mi := &file_model_proto_msgTypes[2]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *AgentStatus) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*AgentStatus) ProtoMessage() {}

func (x *AgentStatus) ProtoReflect() protoreflect.Message {
	mi := &file_model_proto_msgTypes[2]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use AgentStatus.ProtoReflect.Descriptor instead.
func (*AgentStatus) Descriptor() ([]byte, []int) {
	return file_model_proto_rawDescGZIP(), []int{2}
}

func (x *AgentStatus) GetAgentId() string {
	if x != nil {
		return x.AgentId
	}
	return ""
}

func (x *AgentStatus) GetStatus() map[string]string {
	if x != nil {
		return x.Status
	}
	return nil
}

func (x *AgentStatus) GetProgress() map[string]string {
	if x != nil {
		return x.Progress
	}
	return nil
}

type AgentGroup struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Name           string           `protobuf:"bytes,1,opt,name=Name,json=name,proto3" json:"Name,omitempty"`
	Description    string           `protobuf:"bytes,2,opt,name=Description,json=description,proto3" json:"Description,omitempty"`
	Tag            string           `protobuf:"bytes,3,opt,name=Tag,json=tag,proto3" json:"Tag,omitempty"`
	AppliedConfigs map[string]int64 `protobuf:"bytes,4,rep,name=AppliedConfigs,json=applied_configs,proto3" json:"AppliedConfigs,omitempty" protobuf_key:"bytes,1,opt,name=key,proto3" protobuf_val:"varint,2,opt,name=value,proto3"`
	Version        int32            `protobuf:"varint,5,opt,name=Version,json=version,proto3" json:"Version,omitempty"`
}

func (x *AgentGroup) Reset() {
	*x = AgentGroup{}
	if protoimpl.UnsafeEnabled {
		mi := &file_model_proto_msgTypes[3]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *AgentGroup) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*AgentGroup) ProtoMessage() {}

func (x *AgentGroup) ProtoReflect() protoreflect.Message {
	mi := &file_model_proto_msgTypes[3]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use AgentGroup.ProtoReflect.Descriptor instead.
func (*AgentGroup) Descriptor() ([]byte, []int) {
	return file_model_proto_rawDescGZIP(), []int{3}
}

func (x *AgentGroup) GetName() string {
	if x != nil {
		return x.Name
	}
	return ""
}

func (x *AgentGroup) GetDescription() string {
	if x != nil {
		return x.Description
	}
	return ""
}

func (x *AgentGroup) GetTag() string {
	if x != nil {
		return x.Tag
	}
	return ""
}

func (x *AgentGroup) GetAppliedConfigs() map[string]int64 {
	if x != nil {
		return x.AppliedConfigs
	}
	return nil
}

func (x *AgentGroup) GetVersion() int32 {
	if x != nil {
		return x.Version
	}
	return 0
}

type Config struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	Name        string `protobuf:"bytes,1,opt,name=Name,json=name,proto3" json:"Name,omitempty"`
	Content     string `protobuf:"bytes,2,opt,name=Content,json=content,proto3" json:"Content,omitempty"`
	Version     int32  `protobuf:"varint,3,opt,name=Version,json=version,proto3" json:"Version,omitempty"`
	Description string `protobuf:"bytes,4,opt,name=Description,json=description,proto3" json:"Description,omitempty"`
	DelTag      bool   `protobuf:"varint,5,opt,name=DelTag,json=delete_tag,proto3" json:"DelTag,omitempty"`
}

func (x *Config) Reset() {
	*x = Config{}
	if protoimpl.UnsafeEnabled {
		mi := &file_model_proto_msgTypes[4]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *Config) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*Config) ProtoMessage() {}

func (x *Config) ProtoReflect() protoreflect.Message {
	mi := &file_model_proto_msgTypes[4]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use Config.ProtoReflect.Descriptor instead.
func (*Config) Descriptor() ([]byte, []int) {
	return file_model_proto_rawDescGZIP(), []int{4}
}

func (x *Config) GetName() string {
	if x != nil {
		return x.Name
	}
	return ""
}

func (x *Config) GetContent() string {
	if x != nil {
		return x.Content
	}
	return ""
}

func (x *Config) GetVersion() int32 {
	if x != nil {
		return x.Version
	}
	return 0
}

func (x *Config) GetDescription() string {
	if x != nil {
		return x.Description
	}
	return ""
}

func (x *Config) GetDelTag() bool {
	if x != nil {
		return x.DelTag
	}
	return false
}

type CheckResult struct {
	state         protoimpl.MessageState
	sizeCache     protoimpl.SizeCache
	unknownFields protoimpl.UnknownFields

	ConfigName string  `protobuf:"bytes,1,opt,name=ConfigName,json=config_name,proto3" json:"ConfigName,omitempty"`
	Msg        string  `protobuf:"bytes,2,opt,name=Msg,json=message,proto3" json:"Msg,omitempty"`
	Data       *Config `protobuf:"bytes,3,opt,name=Data,json=detail,proto3" json:"Data,omitempty"`
}

func (x *CheckResult) Reset() {
	*x = CheckResult{}
	if protoimpl.UnsafeEnabled {
		mi := &file_model_proto_msgTypes[5]
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		ms.StoreMessageInfo(mi)
	}
}

func (x *CheckResult) String() string {
	return protoimpl.X.MessageStringOf(x)
}

func (*CheckResult) ProtoMessage() {}

func (x *CheckResult) ProtoReflect() protoreflect.Message {
	mi := &file_model_proto_msgTypes[5]
	if protoimpl.UnsafeEnabled && x != nil {
		ms := protoimpl.X.MessageStateOf(protoimpl.Pointer(x))
		if ms.LoadMessageInfo() == nil {
			ms.StoreMessageInfo(mi)
		}
		return ms
	}
	return mi.MessageOf(x)
}

// Deprecated: Use CheckResult.ProtoReflect.Descriptor instead.
func (*CheckResult) Descriptor() ([]byte, []int) {
	return file_model_proto_rawDescGZIP(), []int{5}
}

func (x *CheckResult) GetConfigName() string {
	if x != nil {
		return x.ConfigName
	}
	return ""
}

func (x *CheckResult) GetMsg() string {
	if x != nil {
		return x.Msg
	}
	return ""
}

func (x *CheckResult) GetData() *Config {
	if x != nil {
		return x.Data
	}
	return nil
}

var File_model_proto protoreflect.FileDescriptor

var file_model_proto_rawDesc = []byte{
	0x0a, 0x0b, 0x6d, 0x6f, 0x64, 0x65, 0x6c, 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x12, 0x04, 0x68,
	0x74, 0x74, 0x70, 0x22, 0xb2, 0x04, 0x0a, 0x05, 0x41, 0x67, 0x65, 0x6e, 0x74, 0x12, 0x1c, 0x0a,
	0x07, 0x41, 0x67, 0x65, 0x6e, 0x74, 0x49, 0x64, 0x18, 0x01, 0x20, 0x01, 0x28, 0x09, 0x52, 0x0b,
	0x69, 0x6e, 0x73, 0x74, 0x61, 0x6e, 0x63, 0x65, 0x5f, 0x69, 0x64, 0x12, 0x0e, 0x0a, 0x02, 0x49,
	0x70, 0x18, 0x02, 0x20, 0x01, 0x28, 0x09, 0x52, 0x02, 0x69, 0x70, 0x12, 0x14, 0x0a, 0x05, 0x53,
	0x74, 0x61, 0x74, 0x65, 0x18, 0x03, 0x20, 0x01, 0x28, 0x09, 0x52, 0x05, 0x73, 0x74, 0x61, 0x74,
	0x65, 0x12, 0x16, 0x0a, 0x06, 0x52, 0x65, 0x67, 0x69, 0x6f, 0x6e, 0x18, 0x04, 0x20, 0x01, 0x28,
	0x09, 0x52, 0x06, 0x72, 0x65, 0x67, 0x69, 0x6f, 0x6e, 0x12, 0x1d, 0x0a, 0x09, 0x53, 0x74, 0x61,
	0x72, 0x74, 0x55, 0x70, 0x41, 0x74, 0x18, 0x05, 0x20, 0x01, 0x28, 0x03, 0x52, 0x0a, 0x73, 0x74,
	0x61, 0x72, 0x74, 0x75, 0x70, 0x5f, 0x61, 0x74, 0x12, 0x10, 0x0a, 0x03, 0x45, 0x6e, 0x76, 0x18,
	0x06, 0x20, 0x01, 0x28, 0x09, 0x52, 0x03, 0x65, 0x6e, 0x76, 0x12, 0x18, 0x0a, 0x07, 0x56, 0x65,
	0x72, 0x73, 0x69, 0x6f, 0x6e, 0x18, 0x07, 0x20, 0x01, 0x28, 0x09, 0x52, 0x07, 0x76, 0x65, 0x72,
	0x73, 0x69, 0x6f, 0x6e, 0x12, 0x23, 0x0a, 0x0c, 0x43, 0x6f, 0x6e, 0x6e, 0x65, 0x63, 0x74, 0x53,
	0x74, 0x61, 0x74, 0x65, 0x18, 0x08, 0x20, 0x01, 0x28, 0x09, 0x52, 0x0d, 0x63, 0x6f, 0x6e, 0x6e,
	0x65, 0x63, 0x74, 0x5f, 0x73, 0x74, 0x61, 0x74, 0x65, 0x12, 0x1c, 0x0a, 0x09, 0x48, 0x65, 0x61,
	0x72, 0x74, 0x62, 0x65, 0x61, 0x74, 0x18, 0x09, 0x20, 0x01, 0x28, 0x09, 0x52, 0x09, 0x68, 0x65,
	0x61, 0x72, 0x74, 0x62, 0x65, 0x61, 0x74, 0x12, 0x27, 0x0a, 0x03, 0x54, 0x61, 0x67, 0x18, 0x0a,
	0x20, 0x03, 0x28, 0x0b, 0x32, 0x14, 0x2e, 0x68, 0x74, 0x74, 0x70, 0x2e, 0x41, 0x67, 0x65, 0x6e,
	0x74, 0x2e, 0x54, 0x61, 0x67, 0x45, 0x6e, 0x74, 0x72, 0x79, 0x52, 0x04, 0x74, 0x61, 0x67, 0x73,
	0x12, 0x2f, 0x0a, 0x06, 0x53, 0x74, 0x61, 0x74, 0x75, 0x73, 0x18, 0x0b, 0x20, 0x03, 0x28, 0x0b,
	0x32, 0x17, 0x2e, 0x68, 0x74, 0x74, 0x70, 0x2e, 0x41, 0x67, 0x65, 0x6e, 0x74, 0x2e, 0x53, 0x74,
	0x61, 0x74, 0x75, 0x73, 0x45, 0x6e, 0x74, 0x72, 0x79, 0x52, 0x06, 0x73, 0x74, 0x61, 0x74, 0x75,
	0x73, 0x12, 0x35, 0x0a, 0x08, 0x50, 0x72, 0x6f, 0x67, 0x72, 0x65, 0x73, 0x73, 0x18, 0x0c, 0x20,
	0x03, 0x28, 0x0b, 0x32, 0x19, 0x2e, 0x68, 0x74, 0x74, 0x70, 0x2e, 0x41, 0x67, 0x65, 0x6e, 0x74,
	0x2e, 0x50, 0x72, 0x6f, 0x67, 0x72, 0x65, 0x73, 0x73, 0x45, 0x6e, 0x74, 0x72, 0x79, 0x52, 0x08,
	0x70, 0x72, 0x6f, 0x67, 0x72, 0x65, 0x73, 0x73, 0x1a, 0x36, 0x0a, 0x08, 0x54, 0x61, 0x67, 0x45,
	0x6e, 0x74, 0x72, 0x79, 0x12, 0x10, 0x0a, 0x03, 0x6b, 0x65, 0x79, 0x18, 0x01, 0x20, 0x01, 0x28,
	0x09, 0x52, 0x03, 0x6b, 0x65, 0x79, 0x12, 0x14, 0x0a, 0x05, 0x76, 0x61, 0x6c, 0x75, 0x65, 0x18,
	0x02, 0x20, 0x01, 0x28, 0x09, 0x52, 0x05, 0x76, 0x61, 0x6c, 0x75, 0x65, 0x3a, 0x02, 0x38, 0x01,
	0x1a, 0x39, 0x0a, 0x0b, 0x53, 0x74, 0x61, 0x74, 0x75, 0x73, 0x45, 0x6e, 0x74, 0x72, 0x79, 0x12,
	0x10, 0x0a, 0x03, 0x6b, 0x65, 0x79, 0x18, 0x01, 0x20, 0x01, 0x28, 0x09, 0x52, 0x03, 0x6b, 0x65,
	0x79, 0x12, 0x14, 0x0a, 0x05, 0x76, 0x61, 0x6c, 0x75, 0x65, 0x18, 0x02, 0x20, 0x01, 0x28, 0x09,
	0x52, 0x05, 0x76, 0x61, 0x6c, 0x75, 0x65, 0x3a, 0x02, 0x38, 0x01, 0x1a, 0x3b, 0x0a, 0x0d, 0x50,
	0x72, 0x6f, 0x67, 0x72, 0x65, 0x73, 0x73, 0x45, 0x6e, 0x74, 0x72, 0x79, 0x12, 0x10, 0x0a, 0x03,
	0x6b, 0x65, 0x79, 0x18, 0x01, 0x20, 0x01, 0x28, 0x09, 0x52, 0x03, 0x6b, 0x65, 0x79, 0x12, 0x14,
	0x0a, 0x05, 0x76, 0x61, 0x6c, 0x75, 0x65, 0x18, 0x02, 0x20, 0x01, 0x28, 0x09, 0x52, 0x05, 0x76,
	0x61, 0x6c, 0x75, 0x65, 0x3a, 0x02, 0x38, 0x01, 0x22, 0x8c, 0x01, 0x0a, 0x0a, 0x41, 0x67, 0x65,
	0x6e, 0x74, 0x41, 0x6c, 0x61, 0x72, 0x6d, 0x12, 0x1b, 0x0a, 0x08, 0x41, 0x6c, 0x61, 0x72, 0x6d,
	0x4b, 0x65, 0x79, 0x18, 0x01, 0x20, 0x01, 0x28, 0x09, 0x52, 0x09, 0x61, 0x6c, 0x61, 0x72, 0x6d,
	0x5f, 0x6b, 0x65, 0x79, 0x12, 0x1d, 0x0a, 0x09, 0x41, 0x6c, 0x61, 0x72, 0x6d, 0x54, 0x69, 0x6d,
	0x65, 0x18, 0x02, 0x20, 0x01, 0x28, 0x09, 0x52, 0x0a, 0x61, 0x6c, 0x61, 0x72, 0x6d, 0x5f, 0x74,
	0x69, 0x6d, 0x65, 0x12, 0x1d, 0x0a, 0x09, 0x41, 0x6c, 0x61, 0x72, 0x6d, 0x54, 0x79, 0x70, 0x65,
	0x18, 0x03, 0x20, 0x01, 0x28, 0x09, 0x52, 0x0a, 0x61, 0x6c, 0x61, 0x72, 0x6d, 0x5f, 0x74, 0x79,
	0x70, 0x65, 0x12, 0x23, 0x0a, 0x0c, 0x41, 0x6c, 0x61, 0x72, 0x6d, 0x4d, 0x65, 0x73, 0x73, 0x61,
	0x67, 0x65, 0x18, 0x04, 0x20, 0x01, 0x28, 0x09, 0x52, 0x0d, 0x61, 0x6c, 0x61, 0x72, 0x6d, 0x5f,
	0x6d, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 0x22, 0x97, 0x02, 0x0a, 0x0b, 0x41, 0x67, 0x65, 0x6e,
	0x74, 0x53, 0x74, 0x61, 0x74, 0x75, 0x73, 0x12, 0x1c, 0x0a, 0x07, 0x41, 0x67, 0x65, 0x6e, 0x74,
	0x49, 0x64, 0x18, 0x01, 0x20, 0x01, 0x28, 0x09, 0x52, 0x0b, 0x69, 0x6e, 0x73, 0x74, 0x61, 0x6e,
	0x63, 0x65, 0x5f, 0x69, 0x64, 0x12, 0x35, 0x0a, 0x06, 0x53, 0x74, 0x61, 0x74, 0x75, 0x73, 0x18,
	0x02, 0x20, 0x03, 0x28, 0x0b, 0x32, 0x1d, 0x2e, 0x68, 0x74, 0x74, 0x70, 0x2e, 0x41, 0x67, 0x65,
	0x6e, 0x74, 0x53, 0x74, 0x61, 0x74, 0x75, 0x73, 0x2e, 0x53, 0x74, 0x61, 0x74, 0x75, 0x73, 0x45,
	0x6e, 0x74, 0x72, 0x79, 0x52, 0x06, 0x73, 0x74, 0x61, 0x74, 0x75, 0x73, 0x12, 0x3b, 0x0a, 0x08,
	0x50, 0x72, 0x6f, 0x67, 0x72, 0x65, 0x73, 0x73, 0x18, 0x03, 0x20, 0x03, 0x28, 0x0b, 0x32, 0x1f,
	0x2e, 0x68, 0x74, 0x74, 0x70, 0x2e, 0x41, 0x67, 0x65, 0x6e, 0x74, 0x53, 0x74, 0x61, 0x74, 0x75,
	0x73, 0x2e, 0x50, 0x72, 0x6f, 0x67, 0x72, 0x65, 0x73, 0x73, 0x45, 0x6e, 0x74, 0x72, 0x79, 0x52,
	0x08, 0x70, 0x72, 0x6f, 0x67, 0x72, 0x65, 0x73, 0x73, 0x1a, 0x39, 0x0a, 0x0b, 0x53, 0x74, 0x61,
	0x74, 0x75, 0x73, 0x45, 0x6e, 0x74, 0x72, 0x79, 0x12, 0x10, 0x0a, 0x03, 0x6b, 0x65, 0x79, 0x18,
	0x01, 0x20, 0x01, 0x28, 0x09, 0x52, 0x03, 0x6b, 0x65, 0x79, 0x12, 0x14, 0x0a, 0x05, 0x76, 0x61,
	0x6c, 0x75, 0x65, 0x18, 0x02, 0x20, 0x01, 0x28, 0x09, 0x52, 0x05, 0x76, 0x61, 0x6c, 0x75, 0x65,
	0x3a, 0x02, 0x38, 0x01, 0x1a, 0x3b, 0x0a, 0x0d, 0x50, 0x72, 0x6f, 0x67, 0x72, 0x65, 0x73, 0x73,
	0x45, 0x6e, 0x74, 0x72, 0x79, 0x12, 0x10, 0x0a, 0x03, 0x6b, 0x65, 0x79, 0x18, 0x01, 0x20, 0x01,
	0x28, 0x09, 0x52, 0x03, 0x6b, 0x65, 0x79, 0x12, 0x14, 0x0a, 0x05, 0x76, 0x61, 0x6c, 0x75, 0x65,
	0x18, 0x02, 0x20, 0x01, 0x28, 0x09, 0x52, 0x05, 0x76, 0x61, 0x6c, 0x75, 0x65, 0x3a, 0x02, 0x38,
	0x01, 0x22, 0x80, 0x02, 0x0a, 0x0a, 0x41, 0x67, 0x65, 0x6e, 0x74, 0x47, 0x72, 0x6f, 0x75, 0x70,
	0x12, 0x12, 0x0a, 0x04, 0x4e, 0x61, 0x6d, 0x65, 0x18, 0x01, 0x20, 0x01, 0x28, 0x09, 0x52, 0x04,
	0x6e, 0x61, 0x6d, 0x65, 0x12, 0x20, 0x0a, 0x0b, 0x44, 0x65, 0x73, 0x63, 0x72, 0x69, 0x70, 0x74,
	0x69, 0x6f, 0x6e, 0x18, 0x02, 0x20, 0x01, 0x28, 0x09, 0x52, 0x0b, 0x64, 0x65, 0x73, 0x63, 0x72,
	0x69, 0x70, 0x74, 0x69, 0x6f, 0x6e, 0x12, 0x10, 0x0a, 0x03, 0x54, 0x61, 0x67, 0x18, 0x03, 0x20,
	0x01, 0x28, 0x09, 0x52, 0x03, 0x74, 0x61, 0x67, 0x12, 0x4d, 0x0a, 0x0e, 0x41, 0x70, 0x70, 0x6c,
	0x69, 0x65, 0x64, 0x43, 0x6f, 0x6e, 0x66, 0x69, 0x67, 0x73, 0x18, 0x04, 0x20, 0x03, 0x28, 0x0b,
	0x32, 0x24, 0x2e, 0x68, 0x74, 0x74, 0x70, 0x2e, 0x41, 0x67, 0x65, 0x6e, 0x74, 0x47, 0x72, 0x6f,
	0x75, 0x70, 0x2e, 0x41, 0x70, 0x70, 0x6c, 0x69, 0x65, 0x64, 0x43, 0x6f, 0x6e, 0x66, 0x69, 0x67,
	0x73, 0x45, 0x6e, 0x74, 0x72, 0x79, 0x52, 0x0f, 0x61, 0x70, 0x70, 0x6c, 0x69, 0x65, 0x64, 0x5f,
	0x63, 0x6f, 0x6e, 0x66, 0x69, 0x67, 0x73, 0x12, 0x18, 0x0a, 0x07, 0x56, 0x65, 0x72, 0x73, 0x69,
	0x6f, 0x6e, 0x18, 0x05, 0x20, 0x01, 0x28, 0x05, 0x52, 0x07, 0x76, 0x65, 0x72, 0x73, 0x69, 0x6f,
	0x6e, 0x1a, 0x41, 0x0a, 0x13, 0x41, 0x70, 0x70, 0x6c, 0x69, 0x65, 0x64, 0x43, 0x6f, 0x6e, 0x66,
	0x69, 0x67, 0x73, 0x45, 0x6e, 0x74, 0x72, 0x79, 0x12, 0x10, 0x0a, 0x03, 0x6b, 0x65, 0x79, 0x18,
	0x01, 0x20, 0x01, 0x28, 0x09, 0x52, 0x03, 0x6b, 0x65, 0x79, 0x12, 0x14, 0x0a, 0x05, 0x76, 0x61,
	0x6c, 0x75, 0x65, 0x18, 0x02, 0x20, 0x01, 0x28, 0x03, 0x52, 0x05, 0x76, 0x61, 0x6c, 0x75, 0x65,
	0x3a, 0x02, 0x38, 0x01, 0x22, 0x8e, 0x01, 0x0a, 0x06, 0x43, 0x6f, 0x6e, 0x66, 0x69, 0x67, 0x12,
	0x12, 0x0a, 0x04, 0x4e, 0x61, 0x6d, 0x65, 0x18, 0x01, 0x20, 0x01, 0x28, 0x09, 0x52, 0x04, 0x6e,
	0x61, 0x6d, 0x65, 0x12, 0x18, 0x0a, 0x07, 0x43, 0x6f, 0x6e, 0x74, 0x65, 0x6e, 0x74, 0x18, 0x02,
	0x20, 0x01, 0x28, 0x09, 0x52, 0x07, 0x63, 0x6f, 0x6e, 0x74, 0x65, 0x6e, 0x74, 0x12, 0x18, 0x0a,
	0x07, 0x56, 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0x18, 0x03, 0x20, 0x01, 0x28, 0x05, 0x52, 0x07,
	0x76, 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0x12, 0x20, 0x0a, 0x0b, 0x44, 0x65, 0x73, 0x63, 0x72,
	0x69, 0x70, 0x74, 0x69, 0x6f, 0x6e, 0x18, 0x04, 0x20, 0x01, 0x28, 0x09, 0x52, 0x0b, 0x64, 0x65,
	0x73, 0x63, 0x72, 0x69, 0x70, 0x74, 0x69, 0x6f, 0x6e, 0x12, 0x1a, 0x0a, 0x06, 0x44, 0x65, 0x6c,
	0x54, 0x61, 0x67, 0x18, 0x05, 0x20, 0x01, 0x28, 0x08, 0x52, 0x0a, 0x64, 0x65, 0x6c, 0x65, 0x74,
	0x65, 0x5f, 0x74, 0x61, 0x67, 0x22, 0x68, 0x0a, 0x0b, 0x43, 0x68, 0x65, 0x63, 0x6b, 0x52, 0x65,
	0x73, 0x75, 0x6c, 0x74, 0x12, 0x1f, 0x0a, 0x0a, 0x43, 0x6f, 0x6e, 0x66, 0x69, 0x67, 0x4e, 0x61,
	0x6d, 0x65, 0x18, 0x01, 0x20, 0x01, 0x28, 0x09, 0x52, 0x0b, 0x63, 0x6f, 0x6e, 0x66, 0x69, 0x67,
	0x5f, 0x6e, 0x61, 0x6d, 0x65, 0x12, 0x14, 0x0a, 0x03, 0x4d, 0x73, 0x67, 0x18, 0x02, 0x20, 0x01,
	0x28, 0x09, 0x52, 0x07, 0x6d, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65, 0x12, 0x22, 0x0a, 0x04, 0x44,
	0x61, 0x74, 0x61, 0x18, 0x03, 0x20, 0x01, 0x28, 0x0b, 0x32, 0x0c, 0x2e, 0x68, 0x74, 0x74, 0x70,
	0x2e, 0x43, 0x6f, 0x6e, 0x66, 0x69, 0x67, 0x52, 0x06, 0x64, 0x65, 0x74, 0x61, 0x69, 0x6c, 0x42,
	0x04, 0x5a, 0x02, 0x2e, 0x2e, 0x62, 0x06, 0x70, 0x72, 0x6f, 0x74, 0x6f, 0x33,
}

var (
	file_model_proto_rawDescOnce sync.Once
	file_model_proto_rawDescData = file_model_proto_rawDesc
)

func file_model_proto_rawDescGZIP() []byte {
	file_model_proto_rawDescOnce.Do(func() {
		file_model_proto_rawDescData = protoimpl.X.CompressGZIP(file_model_proto_rawDescData)
	})
	return file_model_proto_rawDescData
}

var file_model_proto_msgTypes = make([]protoimpl.MessageInfo, 12)
var file_model_proto_goTypes = []interface{}{
	(*Agent)(nil),       // 0: http.Agent
	(*AgentAlarm)(nil),  // 1: http.AgentAlarm
	(*AgentStatus)(nil), // 2: http.AgentStatus
	(*AgentGroup)(nil),  // 3: http.AgentGroup
	(*Config)(nil),      // 4: http.Config
	(*CheckResult)(nil), // 5: http.CheckResult
	nil,                 // 6: http.Agent.TagEntry
	nil,                 // 7: http.Agent.StatusEntry
	nil,                 // 8: http.Agent.ProgressEntry
	nil,                 // 9: http.AgentStatus.StatusEntry
	nil,                 // 10: http.AgentStatus.ProgressEntry
	nil,                 // 11: http.AgentGroup.AppliedConfigsEntry
}
var file_model_proto_depIdxs = []int32{
	6,  // 0: http.Agent.Tag:type_name -> http.Agent.TagEntry
	7,  // 1: http.Agent.Status:type_name -> http.Agent.StatusEntry
	8,  // 2: http.Agent.Progress:type_name -> http.Agent.ProgressEntry
	9,  // 3: http.AgentStatus.Status:type_name -> http.AgentStatus.StatusEntry
	10, // 4: http.AgentStatus.Progress:type_name -> http.AgentStatus.ProgressEntry
	11, // 5: http.AgentGroup.AppliedConfigs:type_name -> http.AgentGroup.AppliedConfigsEntry
	4,  // 6: http.CheckResult.Data:type_name -> http.Config
	7,  // [7:7] is the sub-list for method output_type
	7,  // [7:7] is the sub-list for method input_type
	7,  // [7:7] is the sub-list for extension type_name
	7,  // [7:7] is the sub-list for extension extendee
	0,  // [0:7] is the sub-list for field type_name
}

func init() { file_model_proto_init() }
func file_model_proto_init() {
	if File_model_proto != nil {
		return
	}
	if !protoimpl.UnsafeEnabled {
		file_model_proto_msgTypes[0].Exporter = func(v interface{}, i int) interface{} {
			switch v := v.(*Agent); i {
			case 0:
				return &v.state
			case 1:
				return &v.sizeCache
			case 2:
				return &v.unknownFields
			default:
				return nil
			}
		}
		file_model_proto_msgTypes[1].Exporter = func(v interface{}, i int) interface{} {
			switch v := v.(*AgentAlarm); i {
			case 0:
				return &v.state
			case 1:
				return &v.sizeCache
			case 2:
				return &v.unknownFields
			default:
				return nil
			}
		}
		file_model_proto_msgTypes[2].Exporter = func(v interface{}, i int) interface{} {
			switch v := v.(*AgentStatus); i {
			case 0:
				return &v.state
			case 1:
				return &v.sizeCache
			case 2:
				return &v.unknownFields
			default:
				return nil
			}
		}
		file_model_proto_msgTypes[3].Exporter = func(v interface{}, i int) interface{} {
			switch v := v.(*AgentGroup); i {
			case 0:
				return &v.state
			case 1:
				return &v.sizeCache
			case 2:
				return &v.unknownFields
			default:
				return nil
			}
		}
		file_model_proto_msgTypes[4].Exporter = func(v interface{}, i int) interface{} {
			switch v := v.(*Config); i {
			case 0:
				return &v.state
			case 1:
				return &v.sizeCache
			case 2:
				return &v.unknownFields
			default:
				return nil
			}
		}
		file_model_proto_msgTypes[5].Exporter = func(v interface{}, i int) interface{} {
			switch v := v.(*CheckResult); i {
			case 0:
				return &v.state
			case 1:
				return &v.sizeCache
			case 2:
				return &v.unknownFields
			default:
				return nil
			}
		}
	}
	type x struct{}
	out := protoimpl.TypeBuilder{
		File: protoimpl.DescBuilder{
			GoPackagePath: reflect.TypeOf(x{}).PkgPath(),
			RawDescriptor: file_model_proto_rawDesc,
			NumEnums:      0,
			NumMessages:   12,
			NumExtensions: 0,
			NumServices:   0,
		},
		GoTypes:           file_model_proto_goTypes,
		DependencyIndexes: file_model_proto_depIdxs,
		MessageInfos:      file_model_proto_msgTypes,
	}.Build()
	File_model_proto = out.File
	file_model_proto_rawDesc = nil
	file_model_proto_goTypes = nil
	file_model_proto_depIdxs = nil
}
