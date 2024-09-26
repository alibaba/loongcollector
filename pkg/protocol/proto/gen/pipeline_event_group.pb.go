// Code generated by protoc-gen-gogo. DO NOT EDIT.
// source: pipeline_event_group.proto

package protocol

import (
	fmt "fmt"
	proto "github.com/gogo/protobuf/proto"
	io "io"
	math "math"
	math_bits "math/bits"
)

// Reference imports to suppress errors if they are not otherwise used.
var _ = proto.Marshal
var _ = fmt.Errorf
var _ = math.Inf

// This is a compile-time assertion to ensure that this generated file
// is compatible with the proto package it is being compiled against.
// A compilation error at this line likely means your copy of the
// proto package needs to be updated.
const _ = proto.GoGoProtoPackageIsVersion3 // please upgrade the proto package

type PipelineEventGroup struct {
	Metadata map[string][]byte `protobuf:"bytes,1,rep,name=Metadata,proto3" json:"Metadata,omitempty" protobuf_key:"bytes,1,opt,name=key,proto3" protobuf_val:"bytes,2,opt,name=value,proto3"`
	Tags     map[string][]byte `protobuf:"bytes,2,rep,name=Tags,proto3" json:"Tags,omitempty" protobuf_key:"bytes,1,opt,name=key,proto3" protobuf_val:"bytes,2,opt,name=value,proto3"`
	// Types that are valid to be assigned to PipelineEvents:
	//	*PipelineEventGroup_Logs
	//	*PipelineEventGroup_Metrics
	//	*PipelineEventGroup_Spans
	PipelineEvents isPipelineEventGroup_PipelineEvents `protobuf_oneof:"PipelineEvents"`
}

func (m *PipelineEventGroup) Reset()         { *m = PipelineEventGroup{} }
func (m *PipelineEventGroup) String() string { return proto.CompactTextString(m) }
func (*PipelineEventGroup) ProtoMessage()    {}
func (*PipelineEventGroup) Descriptor() ([]byte, []int) {
	return fileDescriptor_4ac730b25d5b901f, []int{0}
}
func (m *PipelineEventGroup) XXX_Unmarshal(b []byte) error {
	return m.Unmarshal(b)
}
func (m *PipelineEventGroup) XXX_Marshal(b []byte, deterministic bool) ([]byte, error) {
	if deterministic {
		return xxx_messageInfo_PipelineEventGroup.Marshal(b, m, deterministic)
	} else {
		b = b[:cap(b)]
		n, err := m.MarshalToSizedBuffer(b)
		if err != nil {
			return nil, err
		}
		return b[:n], nil
	}
}
func (m *PipelineEventGroup) XXX_Merge(src proto.Message) {
	xxx_messageInfo_PipelineEventGroup.Merge(m, src)
}
func (m *PipelineEventGroup) XXX_Size() int {
	return m.Size()
}
func (m *PipelineEventGroup) XXX_DiscardUnknown() {
	xxx_messageInfo_PipelineEventGroup.DiscardUnknown(m)
}

var xxx_messageInfo_PipelineEventGroup proto.InternalMessageInfo

type isPipelineEventGroup_PipelineEvents interface {
	isPipelineEventGroup_PipelineEvents()
	MarshalTo([]byte) (int, error)
	Size() int
}

type PipelineEventGroup_Logs struct {
	Logs *PipelineEventGroup_LogEvents `protobuf:"bytes,3,opt,name=Logs,proto3,oneof" json:"Logs,omitempty"`
}
type PipelineEventGroup_Metrics struct {
	Metrics *PipelineEventGroup_MetricEvents `protobuf:"bytes,4,opt,name=Metrics,proto3,oneof" json:"Metrics,omitempty"`
}
type PipelineEventGroup_Spans struct {
	Spans *PipelineEventGroup_SpanEvents `protobuf:"bytes,5,opt,name=Spans,proto3,oneof" json:"Spans,omitempty"`
}

func (*PipelineEventGroup_Logs) isPipelineEventGroup_PipelineEvents()    {}
func (*PipelineEventGroup_Metrics) isPipelineEventGroup_PipelineEvents() {}
func (*PipelineEventGroup_Spans) isPipelineEventGroup_PipelineEvents()   {}

func (m *PipelineEventGroup) GetPipelineEvents() isPipelineEventGroup_PipelineEvents {
	if m != nil {
		return m.PipelineEvents
	}
	return nil
}

func (m *PipelineEventGroup) GetMetadata() map[string][]byte {
	if m != nil {
		return m.Metadata
	}
	return nil
}

func (m *PipelineEventGroup) GetTags() map[string][]byte {
	if m != nil {
		return m.Tags
	}
	return nil
}

func (m *PipelineEventGroup) GetLogs() *PipelineEventGroup_LogEvents {
	if x, ok := m.GetPipelineEvents().(*PipelineEventGroup_Logs); ok {
		return x.Logs
	}
	return nil
}

func (m *PipelineEventGroup) GetMetrics() *PipelineEventGroup_MetricEvents {
	if x, ok := m.GetPipelineEvents().(*PipelineEventGroup_Metrics); ok {
		return x.Metrics
	}
	return nil
}

func (m *PipelineEventGroup) GetSpans() *PipelineEventGroup_SpanEvents {
	if x, ok := m.GetPipelineEvents().(*PipelineEventGroup_Spans); ok {
		return x.Spans
	}
	return nil
}

// XXX_OneofWrappers is for the internal use of the proto package.
func (*PipelineEventGroup) XXX_OneofWrappers() []interface{} {
	return []interface{}{
		(*PipelineEventGroup_Logs)(nil),
		(*PipelineEventGroup_Metrics)(nil),
		(*PipelineEventGroup_Spans)(nil),
	}
}

type PipelineEventGroup_LogEvents struct {
	Array []*LogEvent `protobuf:"bytes,1,rep,name=Array,proto3" json:"Array,omitempty"`
}

func (m *PipelineEventGroup_LogEvents) Reset()         { *m = PipelineEventGroup_LogEvents{} }
func (m *PipelineEventGroup_LogEvents) String() string { return proto.CompactTextString(m) }
func (*PipelineEventGroup_LogEvents) ProtoMessage()    {}
func (*PipelineEventGroup_LogEvents) Descriptor() ([]byte, []int) {
	return fileDescriptor_4ac730b25d5b901f, []int{0, 2}
}
func (m *PipelineEventGroup_LogEvents) XXX_Unmarshal(b []byte) error {
	return m.Unmarshal(b)
}
func (m *PipelineEventGroup_LogEvents) XXX_Marshal(b []byte, deterministic bool) ([]byte, error) {
	if deterministic {
		return xxx_messageInfo_PipelineEventGroup_LogEvents.Marshal(b, m, deterministic)
	} else {
		b = b[:cap(b)]
		n, err := m.MarshalToSizedBuffer(b)
		if err != nil {
			return nil, err
		}
		return b[:n], nil
	}
}
func (m *PipelineEventGroup_LogEvents) XXX_Merge(src proto.Message) {
	xxx_messageInfo_PipelineEventGroup_LogEvents.Merge(m, src)
}
func (m *PipelineEventGroup_LogEvents) XXX_Size() int {
	return m.Size()
}
func (m *PipelineEventGroup_LogEvents) XXX_DiscardUnknown() {
	xxx_messageInfo_PipelineEventGroup_LogEvents.DiscardUnknown(m)
}

var xxx_messageInfo_PipelineEventGroup_LogEvents proto.InternalMessageInfo

func (m *PipelineEventGroup_LogEvents) GetArray() []*LogEvent {
	if m != nil {
		return m.Array
	}
	return nil
}

type PipelineEventGroup_MetricEvents struct {
	Array []*MetricEvent `protobuf:"bytes,1,rep,name=Array,proto3" json:"Array,omitempty"`
}

func (m *PipelineEventGroup_MetricEvents) Reset()         { *m = PipelineEventGroup_MetricEvents{} }
func (m *PipelineEventGroup_MetricEvents) String() string { return proto.CompactTextString(m) }
func (*PipelineEventGroup_MetricEvents) ProtoMessage()    {}
func (*PipelineEventGroup_MetricEvents) Descriptor() ([]byte, []int) {
	return fileDescriptor_4ac730b25d5b901f, []int{0, 3}
}
func (m *PipelineEventGroup_MetricEvents) XXX_Unmarshal(b []byte) error {
	return m.Unmarshal(b)
}
func (m *PipelineEventGroup_MetricEvents) XXX_Marshal(b []byte, deterministic bool) ([]byte, error) {
	if deterministic {
		return xxx_messageInfo_PipelineEventGroup_MetricEvents.Marshal(b, m, deterministic)
	} else {
		b = b[:cap(b)]
		n, err := m.MarshalToSizedBuffer(b)
		if err != nil {
			return nil, err
		}
		return b[:n], nil
	}
}
func (m *PipelineEventGroup_MetricEvents) XXX_Merge(src proto.Message) {
	xxx_messageInfo_PipelineEventGroup_MetricEvents.Merge(m, src)
}
func (m *PipelineEventGroup_MetricEvents) XXX_Size() int {
	return m.Size()
}
func (m *PipelineEventGroup_MetricEvents) XXX_DiscardUnknown() {
	xxx_messageInfo_PipelineEventGroup_MetricEvents.DiscardUnknown(m)
}

var xxx_messageInfo_PipelineEventGroup_MetricEvents proto.InternalMessageInfo

func (m *PipelineEventGroup_MetricEvents) GetArray() []*MetricEvent {
	if m != nil {
		return m.Array
	}
	return nil
}

type PipelineEventGroup_SpanEvents struct {
	Array []*SpanEvent `protobuf:"bytes,1,rep,name=Array,proto3" json:"Array,omitempty"`
}

func (m *PipelineEventGroup_SpanEvents) Reset()         { *m = PipelineEventGroup_SpanEvents{} }
func (m *PipelineEventGroup_SpanEvents) String() string { return proto.CompactTextString(m) }
func (*PipelineEventGroup_SpanEvents) ProtoMessage()    {}
func (*PipelineEventGroup_SpanEvents) Descriptor() ([]byte, []int) {
	return fileDescriptor_4ac730b25d5b901f, []int{0, 4}
}
func (m *PipelineEventGroup_SpanEvents) XXX_Unmarshal(b []byte) error {
	return m.Unmarshal(b)
}
func (m *PipelineEventGroup_SpanEvents) XXX_Marshal(b []byte, deterministic bool) ([]byte, error) {
	if deterministic {
		return xxx_messageInfo_PipelineEventGroup_SpanEvents.Marshal(b, m, deterministic)
	} else {
		b = b[:cap(b)]
		n, err := m.MarshalToSizedBuffer(b)
		if err != nil {
			return nil, err
		}
		return b[:n], nil
	}
}
func (m *PipelineEventGroup_SpanEvents) XXX_Merge(src proto.Message) {
	xxx_messageInfo_PipelineEventGroup_SpanEvents.Merge(m, src)
}
func (m *PipelineEventGroup_SpanEvents) XXX_Size() int {
	return m.Size()
}
func (m *PipelineEventGroup_SpanEvents) XXX_DiscardUnknown() {
	xxx_messageInfo_PipelineEventGroup_SpanEvents.DiscardUnknown(m)
}

var xxx_messageInfo_PipelineEventGroup_SpanEvents proto.InternalMessageInfo

func (m *PipelineEventGroup_SpanEvents) GetArray() []*SpanEvent {
	if m != nil {
		return m.Array
	}
	return nil
}

func init() {
	proto.RegisterType((*PipelineEventGroup)(nil), "protocol.PipelineEventGroup")
	proto.RegisterMapType((map[string][]byte)(nil), "protocol.PipelineEventGroup.MetadataEntry")
	proto.RegisterMapType((map[string][]byte)(nil), "protocol.PipelineEventGroup.TagsEntry")
	proto.RegisterType((*PipelineEventGroup_LogEvents)(nil), "protocol.PipelineEventGroup.LogEvents")
	proto.RegisterType((*PipelineEventGroup_MetricEvents)(nil), "protocol.PipelineEventGroup.MetricEvents")
	proto.RegisterType((*PipelineEventGroup_SpanEvents)(nil), "protocol.PipelineEventGroup.SpanEvents")
}

func init() { proto.RegisterFile("pipeline_event_group.proto", fileDescriptor_4ac730b25d5b901f) }

var fileDescriptor_4ac730b25d5b901f = []byte{
	// 372 bytes of a gzipped FileDescriptorProto
	0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0xff, 0x94, 0x91, 0xc1, 0x4a, 0xeb, 0x40,
	0x14, 0x86, 0x33, 0x6d, 0x73, 0x6f, 0x7b, 0xac, 0x5a, 0x8e, 0x0a, 0x43, 0x16, 0x21, 0xb8, 0xd0,
	0x54, 0x21, 0x0b, 0x45, 0x2a, 0xad, 0x20, 0x16, 0xaa, 0x2e, 0x5a, 0x90, 0xe8, 0xbe, 0x8c, 0x75,
	0x08, 0xc1, 0x98, 0x84, 0x24, 0x2d, 0xf4, 0x2d, 0x04, 0x5f, 0xca, 0x65, 0x97, 0x2e, 0xa5, 0x7d,
	0x11, 0xc9, 0xa4, 0x49, 0x1a, 0x84, 0x16, 0x57, 0xc9, 0x9c, 0xfc, 0xdf, 0x17, 0xce, 0x3f, 0xa0,
	0xf8, 0xb6, 0xcf, 0x1d, 0xdb, 0xe5, 0x43, 0x3e, 0xe1, 0x6e, 0x34, 0xb4, 0x02, 0x6f, 0xec, 0x1b,
	0x7e, 0xe0, 0x45, 0x1e, 0x56, 0xc5, 0x63, 0xe4, 0x39, 0xca, 0xae, 0xe3, 0x59, 0x49, 0x20, 0xf9,
	0xa4, 0xe0, 0x1b, 0x8f, 0x02, 0x7b, 0x54, 0x98, 0x35, 0x42, 0x9f, 0xb9, 0xab, 0x93, 0xc3, 0x0f,
	0x19, 0xf0, 0x61, 0xe9, 0xef, 0xc5, 0xf3, 0xbb, 0xd8, 0x8e, 0xb7, 0x50, 0x1d, 0xf0, 0x88, 0xbd,
	0xb0, 0x88, 0x51, 0xa2, 0x95, 0xf5, 0xad, 0xb3, 0x13, 0x23, 0xfd, 0x95, 0xf1, 0x3b, 0x6f, 0xa4,
	0xe1, 0x9e, 0x1b, 0x05, 0x53, 0x33, 0x63, 0xb1, 0x0d, 0x95, 0x27, 0x66, 0x85, 0xb4, 0x24, 0x1c,
	0x47, 0x6b, 0x1d, 0x71, 0x30, 0xe1, 0x05, 0x83, 0x57, 0x50, 0xe9, 0x7b, 0x56, 0x48, 0xcb, 0x1a,
	0xd9, 0xc8, 0xf6, 0x3d, 0x4b, 0x9c, 0xc2, 0x7b, 0xc9, 0x14, 0x14, 0xf6, 0xe0, 0xff, 0x40, 0x14,
	0x10, 0xd2, 0x8a, 0x10, 0x34, 0x37, 0x2d, 0x10, 0xd8, 0xa3, 0xcc, 0x91, 0xb2, 0x78, 0x0d, 0xf2,
	0xa3, 0xcf, 0xdc, 0x90, 0xca, 0x42, 0x72, 0xbc, 0x56, 0x12, 0x27, 0x33, 0x45, 0xc2, 0x29, 0x1d,
	0xd8, 0x2e, 0x94, 0x83, 0x0d, 0x28, 0xbf, 0xf2, 0x29, 0x25, 0x1a, 0xd1, 0x6b, 0x66, 0xfc, 0x8a,
	0xfb, 0x20, 0x4f, 0x98, 0x33, 0xe6, 0xb4, 0xa4, 0x11, 0xbd, 0x6e, 0x26, 0x87, 0x76, 0xe9, 0x92,
	0x28, 0x2d, 0xa8, 0x65, 0xad, 0xfc, 0x09, 0xbc, 0x80, 0x5a, 0x56, 0x09, 0xea, 0x20, 0xdf, 0x04,
	0x01, 0x9b, 0x2e, 0x6f, 0x12, 0xf3, 0x1d, 0xd2, 0x8c, 0x99, 0x04, 0x94, 0x0e, 0xd4, 0x57, 0x8b,
	0xc0, 0xd3, 0x22, 0x79, 0x90, 0x93, 0x2b, 0xb1, 0x14, 0x6e, 0x01, 0xe4, 0x05, 0x60, 0xb3, 0x88,
	0xee, 0xe5, 0x68, 0x16, 0x5a, 0x82, 0xdd, 0x06, 0xec, 0x14, 0xca, 0x0c, 0xbb, 0xf4, 0x73, 0xae,
	0x92, 0xd9, 0x5c, 0x25, 0xdf, 0x73, 0x95, 0xbc, 0x2f, 0x54, 0x69, 0xb6, 0x50, 0xa5, 0xaf, 0x85,
	0x2a, 0x3d, 0xff, 0x13, 0x9a, 0xf3, 0x9f, 0x00, 0x00, 0x00, 0xff, 0xff, 0x8f, 0x83, 0x57, 0xe9,
	0x15, 0x03, 0x00, 0x00,
}

func (m *PipelineEventGroup) Marshal() (dAtA []byte, err error) {
	size := m.Size()
	dAtA = make([]byte, size)
	n, err := m.MarshalToSizedBuffer(dAtA[:size])
	if err != nil {
		return nil, err
	}
	return dAtA[:n], nil
}

func (m *PipelineEventGroup) MarshalTo(dAtA []byte) (int, error) {
	size := m.Size()
	return m.MarshalToSizedBuffer(dAtA[:size])
}

func (m *PipelineEventGroup) MarshalToSizedBuffer(dAtA []byte) (int, error) {
	i := len(dAtA)
	_ = i
	var l int
	_ = l
	if m.PipelineEvents != nil {
		{
			size := m.PipelineEvents.Size()
			i -= size
			if _, err := m.PipelineEvents.MarshalTo(dAtA[i:]); err != nil {
				return 0, err
			}
		}
	}
	if len(m.Tags) > 0 {
		for k := range m.Tags {
			v := m.Tags[k]
			baseI := i
			if len(v) > 0 {
				i -= len(v)
				copy(dAtA[i:], v)
				i = encodeVarintPipelineEventGroup(dAtA, i, uint64(len(v)))
				i--
				dAtA[i] = 0x12
			}
			i -= len(k)
			copy(dAtA[i:], k)
			i = encodeVarintPipelineEventGroup(dAtA, i, uint64(len(k)))
			i--
			dAtA[i] = 0xa
			i = encodeVarintPipelineEventGroup(dAtA, i, uint64(baseI-i))
			i--
			dAtA[i] = 0x12
		}
	}
	if len(m.Metadata) > 0 {
		for k := range m.Metadata {
			v := m.Metadata[k]
			baseI := i
			if len(v) > 0 {
				i -= len(v)
				copy(dAtA[i:], v)
				i = encodeVarintPipelineEventGroup(dAtA, i, uint64(len(v)))
				i--
				dAtA[i] = 0x12
			}
			i -= len(k)
			copy(dAtA[i:], k)
			i = encodeVarintPipelineEventGroup(dAtA, i, uint64(len(k)))
			i--
			dAtA[i] = 0xa
			i = encodeVarintPipelineEventGroup(dAtA, i, uint64(baseI-i))
			i--
			dAtA[i] = 0xa
		}
	}
	return len(dAtA) - i, nil
}

func (m *PipelineEventGroup_Logs) MarshalTo(dAtA []byte) (int, error) {
	size := m.Size()
	return m.MarshalToSizedBuffer(dAtA[:size])
}

func (m *PipelineEventGroup_Logs) MarshalToSizedBuffer(dAtA []byte) (int, error) {
	i := len(dAtA)
	if m.Logs != nil {
		{
			size, err := m.Logs.MarshalToSizedBuffer(dAtA[:i])
			if err != nil {
				return 0, err
			}
			i -= size
			i = encodeVarintPipelineEventGroup(dAtA, i, uint64(size))
		}
		i--
		dAtA[i] = 0x1a
	}
	return len(dAtA) - i, nil
}
func (m *PipelineEventGroup_Metrics) MarshalTo(dAtA []byte) (int, error) {
	size := m.Size()
	return m.MarshalToSizedBuffer(dAtA[:size])
}

func (m *PipelineEventGroup_Metrics) MarshalToSizedBuffer(dAtA []byte) (int, error) {
	i := len(dAtA)
	if m.Metrics != nil {
		{
			size, err := m.Metrics.MarshalToSizedBuffer(dAtA[:i])
			if err != nil {
				return 0, err
			}
			i -= size
			i = encodeVarintPipelineEventGroup(dAtA, i, uint64(size))
		}
		i--
		dAtA[i] = 0x22
	}
	return len(dAtA) - i, nil
}
func (m *PipelineEventGroup_Spans) MarshalTo(dAtA []byte) (int, error) {
	size := m.Size()
	return m.MarshalToSizedBuffer(dAtA[:size])
}

func (m *PipelineEventGroup_Spans) MarshalToSizedBuffer(dAtA []byte) (int, error) {
	i := len(dAtA)
	if m.Spans != nil {
		{
			size, err := m.Spans.MarshalToSizedBuffer(dAtA[:i])
			if err != nil {
				return 0, err
			}
			i -= size
			i = encodeVarintPipelineEventGroup(dAtA, i, uint64(size))
		}
		i--
		dAtA[i] = 0x2a
	}
	return len(dAtA) - i, nil
}
func (m *PipelineEventGroup_LogEvents) Marshal() (dAtA []byte, err error) {
	size := m.Size()
	dAtA = make([]byte, size)
	n, err := m.MarshalToSizedBuffer(dAtA[:size])
	if err != nil {
		return nil, err
	}
	return dAtA[:n], nil
}

func (m *PipelineEventGroup_LogEvents) MarshalTo(dAtA []byte) (int, error) {
	size := m.Size()
	return m.MarshalToSizedBuffer(dAtA[:size])
}

func (m *PipelineEventGroup_LogEvents) MarshalToSizedBuffer(dAtA []byte) (int, error) {
	i := len(dAtA)
	_ = i
	var l int
	_ = l
	if len(m.Array) > 0 {
		for iNdEx := len(m.Array) - 1; iNdEx >= 0; iNdEx-- {
			{
				size, err := m.Array[iNdEx].MarshalToSizedBuffer(dAtA[:i])
				if err != nil {
					return 0, err
				}
				i -= size
				i = encodeVarintPipelineEventGroup(dAtA, i, uint64(size))
			}
			i--
			dAtA[i] = 0xa
		}
	}
	return len(dAtA) - i, nil
}

func (m *PipelineEventGroup_MetricEvents) Marshal() (dAtA []byte, err error) {
	size := m.Size()
	dAtA = make([]byte, size)
	n, err := m.MarshalToSizedBuffer(dAtA[:size])
	if err != nil {
		return nil, err
	}
	return dAtA[:n], nil
}

func (m *PipelineEventGroup_MetricEvents) MarshalTo(dAtA []byte) (int, error) {
	size := m.Size()
	return m.MarshalToSizedBuffer(dAtA[:size])
}

func (m *PipelineEventGroup_MetricEvents) MarshalToSizedBuffer(dAtA []byte) (int, error) {
	i := len(dAtA)
	_ = i
	var l int
	_ = l
	if len(m.Array) > 0 {
		for iNdEx := len(m.Array) - 1; iNdEx >= 0; iNdEx-- {
			{
				size, err := m.Array[iNdEx].MarshalToSizedBuffer(dAtA[:i])
				if err != nil {
					return 0, err
				}
				i -= size
				i = encodeVarintPipelineEventGroup(dAtA, i, uint64(size))
			}
			i--
			dAtA[i] = 0xa
		}
	}
	return len(dAtA) - i, nil
}

func (m *PipelineEventGroup_SpanEvents) Marshal() (dAtA []byte, err error) {
	size := m.Size()
	dAtA = make([]byte, size)
	n, err := m.MarshalToSizedBuffer(dAtA[:size])
	if err != nil {
		return nil, err
	}
	return dAtA[:n], nil
}

func (m *PipelineEventGroup_SpanEvents) MarshalTo(dAtA []byte) (int, error) {
	size := m.Size()
	return m.MarshalToSizedBuffer(dAtA[:size])
}

func (m *PipelineEventGroup_SpanEvents) MarshalToSizedBuffer(dAtA []byte) (int, error) {
	i := len(dAtA)
	_ = i
	var l int
	_ = l
	if len(m.Array) > 0 {
		for iNdEx := len(m.Array) - 1; iNdEx >= 0; iNdEx-- {
			{
				size, err := m.Array[iNdEx].MarshalToSizedBuffer(dAtA[:i])
				if err != nil {
					return 0, err
				}
				i -= size
				i = encodeVarintPipelineEventGroup(dAtA, i, uint64(size))
			}
			i--
			dAtA[i] = 0xa
		}
	}
	return len(dAtA) - i, nil
}

func encodeVarintPipelineEventGroup(dAtA []byte, offset int, v uint64) int {
	offset -= sovPipelineEventGroup(v)
	base := offset
	for v >= 1<<7 {
		dAtA[offset] = uint8(v&0x7f | 0x80)
		v >>= 7
		offset++
	}
	dAtA[offset] = uint8(v)
	return base
}
func (m *PipelineEventGroup) Size() (n int) {
	if m == nil {
		return 0
	}
	var l int
	_ = l
	if len(m.Metadata) > 0 {
		for k, v := range m.Metadata {
			_ = k
			_ = v
			l = 0
			if len(v) > 0 {
				l = 1 + len(v) + sovPipelineEventGroup(uint64(len(v)))
			}
			mapEntrySize := 1 + len(k) + sovPipelineEventGroup(uint64(len(k))) + l
			n += mapEntrySize + 1 + sovPipelineEventGroup(uint64(mapEntrySize))
		}
	}
	if len(m.Tags) > 0 {
		for k, v := range m.Tags {
			_ = k
			_ = v
			l = 0
			if len(v) > 0 {
				l = 1 + len(v) + sovPipelineEventGroup(uint64(len(v)))
			}
			mapEntrySize := 1 + len(k) + sovPipelineEventGroup(uint64(len(k))) + l
			n += mapEntrySize + 1 + sovPipelineEventGroup(uint64(mapEntrySize))
		}
	}
	if m.PipelineEvents != nil {
		n += m.PipelineEvents.Size()
	}
	return n
}

func (m *PipelineEventGroup_Logs) Size() (n int) {
	if m == nil {
		return 0
	}
	var l int
	_ = l
	if m.Logs != nil {
		l = m.Logs.Size()
		n += 1 + l + sovPipelineEventGroup(uint64(l))
	}
	return n
}
func (m *PipelineEventGroup_Metrics) Size() (n int) {
	if m == nil {
		return 0
	}
	var l int
	_ = l
	if m.Metrics != nil {
		l = m.Metrics.Size()
		n += 1 + l + sovPipelineEventGroup(uint64(l))
	}
	return n
}
func (m *PipelineEventGroup_Spans) Size() (n int) {
	if m == nil {
		return 0
	}
	var l int
	_ = l
	if m.Spans != nil {
		l = m.Spans.Size()
		n += 1 + l + sovPipelineEventGroup(uint64(l))
	}
	return n
}
func (m *PipelineEventGroup_LogEvents) Size() (n int) {
	if m == nil {
		return 0
	}
	var l int
	_ = l
	if len(m.Array) > 0 {
		for _, e := range m.Array {
			l = e.Size()
			n += 1 + l + sovPipelineEventGroup(uint64(l))
		}
	}
	return n
}

func (m *PipelineEventGroup_MetricEvents) Size() (n int) {
	if m == nil {
		return 0
	}
	var l int
	_ = l
	if len(m.Array) > 0 {
		for _, e := range m.Array {
			l = e.Size()
			n += 1 + l + sovPipelineEventGroup(uint64(l))
		}
	}
	return n
}

func (m *PipelineEventGroup_SpanEvents) Size() (n int) {
	if m == nil {
		return 0
	}
	var l int
	_ = l
	if len(m.Array) > 0 {
		for _, e := range m.Array {
			l = e.Size()
			n += 1 + l + sovPipelineEventGroup(uint64(l))
		}
	}
	return n
}

func sovPipelineEventGroup(x uint64) (n int) {
	return (math_bits.Len64(x|1) + 6) / 7
}
func sozPipelineEventGroup(x uint64) (n int) {
	return sovPipelineEventGroup(uint64((x << 1) ^ uint64((int64(x) >> 63))))
}
func (m *PipelineEventGroup) Unmarshal(dAtA []byte) error {
	l := len(dAtA)
	iNdEx := 0
	for iNdEx < l {
		preIndex := iNdEx
		var wire uint64
		for shift := uint(0); ; shift += 7 {
			if shift >= 64 {
				return ErrIntOverflowPipelineEventGroup
			}
			if iNdEx >= l {
				return io.ErrUnexpectedEOF
			}
			b := dAtA[iNdEx]
			iNdEx++
			wire |= uint64(b&0x7F) << shift
			if b < 0x80 {
				break
			}
		}
		fieldNum := int32(wire >> 3)
		wireType := int(wire & 0x7)
		if wireType == 4 {
			return fmt.Errorf("proto: PipelineEventGroup: wiretype end group for non-group")
		}
		if fieldNum <= 0 {
			return fmt.Errorf("proto: PipelineEventGroup: illegal tag %d (wire type %d)", fieldNum, wire)
		}
		switch fieldNum {
		case 1:
			if wireType != 2 {
				return fmt.Errorf("proto: wrong wireType = %d for field Metadata", wireType)
			}
			var msglen int
			for shift := uint(0); ; shift += 7 {
				if shift >= 64 {
					return ErrIntOverflowPipelineEventGroup
				}
				if iNdEx >= l {
					return io.ErrUnexpectedEOF
				}
				b := dAtA[iNdEx]
				iNdEx++
				msglen |= int(b&0x7F) << shift
				if b < 0x80 {
					break
				}
			}
			if msglen < 0 {
				return ErrInvalidLengthPipelineEventGroup
			}
			postIndex := iNdEx + msglen
			if postIndex < 0 {
				return ErrInvalidLengthPipelineEventGroup
			}
			if postIndex > l {
				return io.ErrUnexpectedEOF
			}
			if m.Metadata == nil {
				m.Metadata = make(map[string][]byte)
			}
			var mapkey string
			mapvalue := []byte{}
			for iNdEx < postIndex {
				entryPreIndex := iNdEx
				var wire uint64
				for shift := uint(0); ; shift += 7 {
					if shift >= 64 {
						return ErrIntOverflowPipelineEventGroup
					}
					if iNdEx >= l {
						return io.ErrUnexpectedEOF
					}
					b := dAtA[iNdEx]
					iNdEx++
					wire |= uint64(b&0x7F) << shift
					if b < 0x80 {
						break
					}
				}
				fieldNum := int32(wire >> 3)
				if fieldNum == 1 {
					var stringLenmapkey uint64
					for shift := uint(0); ; shift += 7 {
						if shift >= 64 {
							return ErrIntOverflowPipelineEventGroup
						}
						if iNdEx >= l {
							return io.ErrUnexpectedEOF
						}
						b := dAtA[iNdEx]
						iNdEx++
						stringLenmapkey |= uint64(b&0x7F) << shift
						if b < 0x80 {
							break
						}
					}
					intStringLenmapkey := int(stringLenmapkey)
					if intStringLenmapkey < 0 {
						return ErrInvalidLengthPipelineEventGroup
					}
					postStringIndexmapkey := iNdEx + intStringLenmapkey
					if postStringIndexmapkey < 0 {
						return ErrInvalidLengthPipelineEventGroup
					}
					if postStringIndexmapkey > l {
						return io.ErrUnexpectedEOF
					}
					mapkey = string(dAtA[iNdEx:postStringIndexmapkey])
					iNdEx = postStringIndexmapkey
				} else if fieldNum == 2 {
					var mapbyteLen uint64
					for shift := uint(0); ; shift += 7 {
						if shift >= 64 {
							return ErrIntOverflowPipelineEventGroup
						}
						if iNdEx >= l {
							return io.ErrUnexpectedEOF
						}
						b := dAtA[iNdEx]
						iNdEx++
						mapbyteLen |= uint64(b&0x7F) << shift
						if b < 0x80 {
							break
						}
					}
					intMapbyteLen := int(mapbyteLen)
					if intMapbyteLen < 0 {
						return ErrInvalidLengthPipelineEventGroup
					}
					postbytesIndex := iNdEx + intMapbyteLen
					if postbytesIndex < 0 {
						return ErrInvalidLengthPipelineEventGroup
					}
					if postbytesIndex > l {
						return io.ErrUnexpectedEOF
					}
					mapvalue = make([]byte, mapbyteLen)
					copy(mapvalue, dAtA[iNdEx:postbytesIndex])
					iNdEx = postbytesIndex
				} else {
					iNdEx = entryPreIndex
					skippy, err := skipPipelineEventGroup(dAtA[iNdEx:])
					if err != nil {
						return err
					}
					if (skippy < 0) || (iNdEx+skippy) < 0 {
						return ErrInvalidLengthPipelineEventGroup
					}
					if (iNdEx + skippy) > postIndex {
						return io.ErrUnexpectedEOF
					}
					iNdEx += skippy
				}
			}
			m.Metadata[mapkey] = mapvalue
			iNdEx = postIndex
		case 2:
			if wireType != 2 {
				return fmt.Errorf("proto: wrong wireType = %d for field Tags", wireType)
			}
			var msglen int
			for shift := uint(0); ; shift += 7 {
				if shift >= 64 {
					return ErrIntOverflowPipelineEventGroup
				}
				if iNdEx >= l {
					return io.ErrUnexpectedEOF
				}
				b := dAtA[iNdEx]
				iNdEx++
				msglen |= int(b&0x7F) << shift
				if b < 0x80 {
					break
				}
			}
			if msglen < 0 {
				return ErrInvalidLengthPipelineEventGroup
			}
			postIndex := iNdEx + msglen
			if postIndex < 0 {
				return ErrInvalidLengthPipelineEventGroup
			}
			if postIndex > l {
				return io.ErrUnexpectedEOF
			}
			if m.Tags == nil {
				m.Tags = make(map[string][]byte)
			}
			var mapkey string
			mapvalue := []byte{}
			for iNdEx < postIndex {
				entryPreIndex := iNdEx
				var wire uint64
				for shift := uint(0); ; shift += 7 {
					if shift >= 64 {
						return ErrIntOverflowPipelineEventGroup
					}
					if iNdEx >= l {
						return io.ErrUnexpectedEOF
					}
					b := dAtA[iNdEx]
					iNdEx++
					wire |= uint64(b&0x7F) << shift
					if b < 0x80 {
						break
					}
				}
				fieldNum := int32(wire >> 3)
				if fieldNum == 1 {
					var stringLenmapkey uint64
					for shift := uint(0); ; shift += 7 {
						if shift >= 64 {
							return ErrIntOverflowPipelineEventGroup
						}
						if iNdEx >= l {
							return io.ErrUnexpectedEOF
						}
						b := dAtA[iNdEx]
						iNdEx++
						stringLenmapkey |= uint64(b&0x7F) << shift
						if b < 0x80 {
							break
						}
					}
					intStringLenmapkey := int(stringLenmapkey)
					if intStringLenmapkey < 0 {
						return ErrInvalidLengthPipelineEventGroup
					}
					postStringIndexmapkey := iNdEx + intStringLenmapkey
					if postStringIndexmapkey < 0 {
						return ErrInvalidLengthPipelineEventGroup
					}
					if postStringIndexmapkey > l {
						return io.ErrUnexpectedEOF
					}
					mapkey = string(dAtA[iNdEx:postStringIndexmapkey])
					iNdEx = postStringIndexmapkey
				} else if fieldNum == 2 {
					var mapbyteLen uint64
					for shift := uint(0); ; shift += 7 {
						if shift >= 64 {
							return ErrIntOverflowPipelineEventGroup
						}
						if iNdEx >= l {
							return io.ErrUnexpectedEOF
						}
						b := dAtA[iNdEx]
						iNdEx++
						mapbyteLen |= uint64(b&0x7F) << shift
						if b < 0x80 {
							break
						}
					}
					intMapbyteLen := int(mapbyteLen)
					if intMapbyteLen < 0 {
						return ErrInvalidLengthPipelineEventGroup
					}
					postbytesIndex := iNdEx + intMapbyteLen
					if postbytesIndex < 0 {
						return ErrInvalidLengthPipelineEventGroup
					}
					if postbytesIndex > l {
						return io.ErrUnexpectedEOF
					}
					mapvalue = make([]byte, mapbyteLen)
					copy(mapvalue, dAtA[iNdEx:postbytesIndex])
					iNdEx = postbytesIndex
				} else {
					iNdEx = entryPreIndex
					skippy, err := skipPipelineEventGroup(dAtA[iNdEx:])
					if err != nil {
						return err
					}
					if (skippy < 0) || (iNdEx+skippy) < 0 {
						return ErrInvalidLengthPipelineEventGroup
					}
					if (iNdEx + skippy) > postIndex {
						return io.ErrUnexpectedEOF
					}
					iNdEx += skippy
				}
			}
			m.Tags[mapkey] = mapvalue
			iNdEx = postIndex
		case 3:
			if wireType != 2 {
				return fmt.Errorf("proto: wrong wireType = %d for field Logs", wireType)
			}
			var msglen int
			for shift := uint(0); ; shift += 7 {
				if shift >= 64 {
					return ErrIntOverflowPipelineEventGroup
				}
				if iNdEx >= l {
					return io.ErrUnexpectedEOF
				}
				b := dAtA[iNdEx]
				iNdEx++
				msglen |= int(b&0x7F) << shift
				if b < 0x80 {
					break
				}
			}
			if msglen < 0 {
				return ErrInvalidLengthPipelineEventGroup
			}
			postIndex := iNdEx + msglen
			if postIndex < 0 {
				return ErrInvalidLengthPipelineEventGroup
			}
			if postIndex > l {
				return io.ErrUnexpectedEOF
			}
			v := &PipelineEventGroup_LogEvents{}
			if err := v.Unmarshal(dAtA[iNdEx:postIndex]); err != nil {
				return err
			}
			m.PipelineEvents = &PipelineEventGroup_Logs{v}
			iNdEx = postIndex
		case 4:
			if wireType != 2 {
				return fmt.Errorf("proto: wrong wireType = %d for field Metrics", wireType)
			}
			var msglen int
			for shift := uint(0); ; shift += 7 {
				if shift >= 64 {
					return ErrIntOverflowPipelineEventGroup
				}
				if iNdEx >= l {
					return io.ErrUnexpectedEOF
				}
				b := dAtA[iNdEx]
				iNdEx++
				msglen |= int(b&0x7F) << shift
				if b < 0x80 {
					break
				}
			}
			if msglen < 0 {
				return ErrInvalidLengthPipelineEventGroup
			}
			postIndex := iNdEx + msglen
			if postIndex < 0 {
				return ErrInvalidLengthPipelineEventGroup
			}
			if postIndex > l {
				return io.ErrUnexpectedEOF
			}
			v := &PipelineEventGroup_MetricEvents{}
			if err := v.Unmarshal(dAtA[iNdEx:postIndex]); err != nil {
				return err
			}
			m.PipelineEvents = &PipelineEventGroup_Metrics{v}
			iNdEx = postIndex
		case 5:
			if wireType != 2 {
				return fmt.Errorf("proto: wrong wireType = %d for field Spans", wireType)
			}
			var msglen int
			for shift := uint(0); ; shift += 7 {
				if shift >= 64 {
					return ErrIntOverflowPipelineEventGroup
				}
				if iNdEx >= l {
					return io.ErrUnexpectedEOF
				}
				b := dAtA[iNdEx]
				iNdEx++
				msglen |= int(b&0x7F) << shift
				if b < 0x80 {
					break
				}
			}
			if msglen < 0 {
				return ErrInvalidLengthPipelineEventGroup
			}
			postIndex := iNdEx + msglen
			if postIndex < 0 {
				return ErrInvalidLengthPipelineEventGroup
			}
			if postIndex > l {
				return io.ErrUnexpectedEOF
			}
			v := &PipelineEventGroup_SpanEvents{}
			if err := v.Unmarshal(dAtA[iNdEx:postIndex]); err != nil {
				return err
			}
			m.PipelineEvents = &PipelineEventGroup_Spans{v}
			iNdEx = postIndex
		default:
			iNdEx = preIndex
			skippy, err := skipPipelineEventGroup(dAtA[iNdEx:])
			if err != nil {
				return err
			}
			if (skippy < 0) || (iNdEx+skippy) < 0 {
				return ErrInvalidLengthPipelineEventGroup
			}
			if (iNdEx + skippy) > l {
				return io.ErrUnexpectedEOF
			}
			iNdEx += skippy
		}
	}

	if iNdEx > l {
		return io.ErrUnexpectedEOF
	}
	return nil
}
func (m *PipelineEventGroup_LogEvents) Unmarshal(dAtA []byte) error {
	l := len(dAtA)
	iNdEx := 0
	for iNdEx < l {
		preIndex := iNdEx
		var wire uint64
		for shift := uint(0); ; shift += 7 {
			if shift >= 64 {
				return ErrIntOverflowPipelineEventGroup
			}
			if iNdEx >= l {
				return io.ErrUnexpectedEOF
			}
			b := dAtA[iNdEx]
			iNdEx++
			wire |= uint64(b&0x7F) << shift
			if b < 0x80 {
				break
			}
		}
		fieldNum := int32(wire >> 3)
		wireType := int(wire & 0x7)
		if wireType == 4 {
			return fmt.Errorf("proto: LogEvents: wiretype end group for non-group")
		}
		if fieldNum <= 0 {
			return fmt.Errorf("proto: LogEvents: illegal tag %d (wire type %d)", fieldNum, wire)
		}
		switch fieldNum {
		case 1:
			if wireType != 2 {
				return fmt.Errorf("proto: wrong wireType = %d for field Array", wireType)
			}
			var msglen int
			for shift := uint(0); ; shift += 7 {
				if shift >= 64 {
					return ErrIntOverflowPipelineEventGroup
				}
				if iNdEx >= l {
					return io.ErrUnexpectedEOF
				}
				b := dAtA[iNdEx]
				iNdEx++
				msglen |= int(b&0x7F) << shift
				if b < 0x80 {
					break
				}
			}
			if msglen < 0 {
				return ErrInvalidLengthPipelineEventGroup
			}
			postIndex := iNdEx + msglen
			if postIndex < 0 {
				return ErrInvalidLengthPipelineEventGroup
			}
			if postIndex > l {
				return io.ErrUnexpectedEOF
			}
			m.Array = append(m.Array, &LogEvent{})
			if err := m.Array[len(m.Array)-1].Unmarshal(dAtA[iNdEx:postIndex]); err != nil {
				return err
			}
			iNdEx = postIndex
		default:
			iNdEx = preIndex
			skippy, err := skipPipelineEventGroup(dAtA[iNdEx:])
			if err != nil {
				return err
			}
			if (skippy < 0) || (iNdEx+skippy) < 0 {
				return ErrInvalidLengthPipelineEventGroup
			}
			if (iNdEx + skippy) > l {
				return io.ErrUnexpectedEOF
			}
			iNdEx += skippy
		}
	}

	if iNdEx > l {
		return io.ErrUnexpectedEOF
	}
	return nil
}
func (m *PipelineEventGroup_MetricEvents) Unmarshal(dAtA []byte) error {
	l := len(dAtA)
	iNdEx := 0
	for iNdEx < l {
		preIndex := iNdEx
		var wire uint64
		for shift := uint(0); ; shift += 7 {
			if shift >= 64 {
				return ErrIntOverflowPipelineEventGroup
			}
			if iNdEx >= l {
				return io.ErrUnexpectedEOF
			}
			b := dAtA[iNdEx]
			iNdEx++
			wire |= uint64(b&0x7F) << shift
			if b < 0x80 {
				break
			}
		}
		fieldNum := int32(wire >> 3)
		wireType := int(wire & 0x7)
		if wireType == 4 {
			return fmt.Errorf("proto: MetricEvents: wiretype end group for non-group")
		}
		if fieldNum <= 0 {
			return fmt.Errorf("proto: MetricEvents: illegal tag %d (wire type %d)", fieldNum, wire)
		}
		switch fieldNum {
		case 1:
			if wireType != 2 {
				return fmt.Errorf("proto: wrong wireType = %d for field Array", wireType)
			}
			var msglen int
			for shift := uint(0); ; shift += 7 {
				if shift >= 64 {
					return ErrIntOverflowPipelineEventGroup
				}
				if iNdEx >= l {
					return io.ErrUnexpectedEOF
				}
				b := dAtA[iNdEx]
				iNdEx++
				msglen |= int(b&0x7F) << shift
				if b < 0x80 {
					break
				}
			}
			if msglen < 0 {
				return ErrInvalidLengthPipelineEventGroup
			}
			postIndex := iNdEx + msglen
			if postIndex < 0 {
				return ErrInvalidLengthPipelineEventGroup
			}
			if postIndex > l {
				return io.ErrUnexpectedEOF
			}
			m.Array = append(m.Array, &MetricEvent{})
			if err := m.Array[len(m.Array)-1].Unmarshal(dAtA[iNdEx:postIndex]); err != nil {
				return err
			}
			iNdEx = postIndex
		default:
			iNdEx = preIndex
			skippy, err := skipPipelineEventGroup(dAtA[iNdEx:])
			if err != nil {
				return err
			}
			if (skippy < 0) || (iNdEx+skippy) < 0 {
				return ErrInvalidLengthPipelineEventGroup
			}
			if (iNdEx + skippy) > l {
				return io.ErrUnexpectedEOF
			}
			iNdEx += skippy
		}
	}

	if iNdEx > l {
		return io.ErrUnexpectedEOF
	}
	return nil
}
func (m *PipelineEventGroup_SpanEvents) Unmarshal(dAtA []byte) error {
	l := len(dAtA)
	iNdEx := 0
	for iNdEx < l {
		preIndex := iNdEx
		var wire uint64
		for shift := uint(0); ; shift += 7 {
			if shift >= 64 {
				return ErrIntOverflowPipelineEventGroup
			}
			if iNdEx >= l {
				return io.ErrUnexpectedEOF
			}
			b := dAtA[iNdEx]
			iNdEx++
			wire |= uint64(b&0x7F) << shift
			if b < 0x80 {
				break
			}
		}
		fieldNum := int32(wire >> 3)
		wireType := int(wire & 0x7)
		if wireType == 4 {
			return fmt.Errorf("proto: SpanEvents: wiretype end group for non-group")
		}
		if fieldNum <= 0 {
			return fmt.Errorf("proto: SpanEvents: illegal tag %d (wire type %d)", fieldNum, wire)
		}
		switch fieldNum {
		case 1:
			if wireType != 2 {
				return fmt.Errorf("proto: wrong wireType = %d for field Array", wireType)
			}
			var msglen int
			for shift := uint(0); ; shift += 7 {
				if shift >= 64 {
					return ErrIntOverflowPipelineEventGroup
				}
				if iNdEx >= l {
					return io.ErrUnexpectedEOF
				}
				b := dAtA[iNdEx]
				iNdEx++
				msglen |= int(b&0x7F) << shift
				if b < 0x80 {
					break
				}
			}
			if msglen < 0 {
				return ErrInvalidLengthPipelineEventGroup
			}
			postIndex := iNdEx + msglen
			if postIndex < 0 {
				return ErrInvalidLengthPipelineEventGroup
			}
			if postIndex > l {
				return io.ErrUnexpectedEOF
			}
			m.Array = append(m.Array, &SpanEvent{})
			if err := m.Array[len(m.Array)-1].Unmarshal(dAtA[iNdEx:postIndex]); err != nil {
				return err
			}
			iNdEx = postIndex
		default:
			iNdEx = preIndex
			skippy, err := skipPipelineEventGroup(dAtA[iNdEx:])
			if err != nil {
				return err
			}
			if (skippy < 0) || (iNdEx+skippy) < 0 {
				return ErrInvalidLengthPipelineEventGroup
			}
			if (iNdEx + skippy) > l {
				return io.ErrUnexpectedEOF
			}
			iNdEx += skippy
		}
	}

	if iNdEx > l {
		return io.ErrUnexpectedEOF
	}
	return nil
}
func skipPipelineEventGroup(dAtA []byte) (n int, err error) {
	l := len(dAtA)
	iNdEx := 0
	depth := 0
	for iNdEx < l {
		var wire uint64
		for shift := uint(0); ; shift += 7 {
			if shift >= 64 {
				return 0, ErrIntOverflowPipelineEventGroup
			}
			if iNdEx >= l {
				return 0, io.ErrUnexpectedEOF
			}
			b := dAtA[iNdEx]
			iNdEx++
			wire |= (uint64(b) & 0x7F) << shift
			if b < 0x80 {
				break
			}
		}
		wireType := int(wire & 0x7)
		switch wireType {
		case 0:
			for shift := uint(0); ; shift += 7 {
				if shift >= 64 {
					return 0, ErrIntOverflowPipelineEventGroup
				}
				if iNdEx >= l {
					return 0, io.ErrUnexpectedEOF
				}
				iNdEx++
				if dAtA[iNdEx-1] < 0x80 {
					break
				}
			}
		case 1:
			iNdEx += 8
		case 2:
			var length int
			for shift := uint(0); ; shift += 7 {
				if shift >= 64 {
					return 0, ErrIntOverflowPipelineEventGroup
				}
				if iNdEx >= l {
					return 0, io.ErrUnexpectedEOF
				}
				b := dAtA[iNdEx]
				iNdEx++
				length |= (int(b) & 0x7F) << shift
				if b < 0x80 {
					break
				}
			}
			if length < 0 {
				return 0, ErrInvalidLengthPipelineEventGroup
			}
			iNdEx += length
		case 3:
			depth++
		case 4:
			if depth == 0 {
				return 0, ErrUnexpectedEndOfGroupPipelineEventGroup
			}
			depth--
		case 5:
			iNdEx += 4
		default:
			return 0, fmt.Errorf("proto: illegal wireType %d", wireType)
		}
		if iNdEx < 0 {
			return 0, ErrInvalidLengthPipelineEventGroup
		}
		if depth == 0 {
			return iNdEx, nil
		}
	}
	return 0, io.ErrUnexpectedEOF
}

var (
	ErrInvalidLengthPipelineEventGroup        = fmt.Errorf("proto: negative length found during unmarshaling")
	ErrIntOverflowPipelineEventGroup          = fmt.Errorf("proto: integer overflow")
	ErrUnexpectedEndOfGroupPipelineEventGroup = fmt.Errorf("proto: unexpected end of group")
)
