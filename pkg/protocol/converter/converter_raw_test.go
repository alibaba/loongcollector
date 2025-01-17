package protocol

import (
	"fmt"
	"testing"

	"github.com/stretchr/testify/assert"

	"github.com/alibaba/ilogtail/pkg/models"
)

func TestConverter_ConvertToRawStream(t *testing.T) {
	type fields struct {
		Protocol             string
		Encoding             string
		Separator            string
		TagKeyRenameMap      map[string]string
		ProtocolKeyRenameMap map[string]string
	}
	type args struct {
		groupEvents  *models.PipelineGroupEvents
		targetFields []string
	}
	mockValidFields := fields{
		Protocol:  ProtocolRaw,
		Encoding:  EncodingCustom,
		Separator: "\n",
	}
	mockInvalidFields := fields{
		Protocol: ProtocolRaw,
		Encoding: EncodingJSON,
	}
	mockGroup := models.NewGroup(models.NewMetadataWithMap(map[string]string{"db": "test"}), nil)
	mockByteEvent := []byte("cpu.load.short,host=server01,region=cn value=0.6")
	tests := []struct {
		name       string
		fields     fields
		args       args
		wantStream [][]byte
		wantValues []map[string]string
		wantErr    assert.ErrorAssertionFunc
	}{
		{
			name:   "single byte event",
			fields: mockValidFields,
			args: args{
				groupEvents: &models.PipelineGroupEvents{Group: mockGroup,
					Events: []models.PipelineEvent{models.ByteArray(mockByteEvent)}},
				targetFields: []string{"metadata.db"},
			},
			wantStream: [][]byte{mockByteEvent},
			wantValues: []map[string]string{{"metadata.db": "test"}},
			wantErr:    assert.NoError,
		}, {
			name:   "multiple events",
			fields: mockValidFields,
			args: args{
				groupEvents: &models.PipelineGroupEvents{Group: mockGroup,
					Events: []models.PipelineEvent{models.ByteArray(mockByteEvent), models.ByteArray(mockByteEvent)}},
				targetFields: []string{"metadata.db"},
			},
			wantStream: [][]byte{append(append(mockByteEvent, "\n"...), mockByteEvent...)},
			wantValues: []map[string]string{{"metadata.db": "test"}},
			wantErr:    assert.NoError,
		}, {
			name:   "unsupported metric event type",
			fields: mockInvalidFields,
			args: args{
				groupEvents: &models.PipelineGroupEvents{Group: mockGroup,
					Events: []models.PipelineEvent{&models.Metric{
						Name:      "cpu.load.short",
						Timestamp: 0,
						Tags:      models.NewTagsWithKeyValues("host", "server01", "region", "cn"),
						Value:     &models.MetricSingleValue{Value: 0.64},
					}, models.ByteArray(mockByteEvent)}},
				targetFields: []string{"metadata.db"},
			},
			wantStream: nil,
			wantValues: nil,
			wantErr:    assert.Error,
		},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			c := &Converter{
				Protocol:             tt.fields.Protocol,
				Encoding:             tt.fields.Encoding,
				Separator:            tt.fields.Separator,
				TagKeyRenameMap:      tt.fields.TagKeyRenameMap,
				ProtocolKeyRenameMap: tt.fields.ProtocolKeyRenameMap,
			}
			gotStream, gotValues, err := c.ConvertToRawStream(tt.args.groupEvents, tt.args.targetFields)
			if !tt.wantErr(t, err, fmt.Sprintf("ConvertToRawStream(%v, %v)", tt.args.groupEvents, tt.args.targetFields)) {
				return
			}
			assert.Equalf(t, tt.wantStream, gotStream, "ConvertToRawStream(%v, %v)", tt.args.groupEvents, tt.args.targetFields)
			assert.Equalf(t, tt.wantValues, gotValues, "ConvertToRawStream(%v, %v)", tt.args.groupEvents, tt.args.targetFields)
		})
	}
}

func TestConverter_ConvertToRawStreamSeparator(t *testing.T) {
	type fields struct {
		Protocol             string
		Encoding             string
		Separator            string
		TagKeyRenameMap      map[string]string
		ProtocolKeyRenameMap map[string]string
	}
	type args struct {
		groupEvents  *models.PipelineGroupEvents
		targetFields []string
	}
	mockFieldsWithSep := fields{
		Protocol:  ProtocolRaw,
		Encoding:  EncodingCustom,
		Separator: "\r\n",
	}
	mockFieldsWithoutSep := fields{
		Protocol: ProtocolRaw,
		Encoding: EncodingJSON,
	}
	mockGroup := models.NewGroup(models.NewMetadataWithMap(map[string]string{"db": "test"}), nil)
	mockByteEvent := []byte("cpu.load.short,host=server01,region=cn value=0.6")
	tests := []struct {
		name       string
		fields     fields
		args       args
		wantStream [][]byte
		wantValues []map[string]string
		wantErr    assert.ErrorAssertionFunc
	}{
		{
			name:   "join with sep",
			fields: mockFieldsWithSep,
			args: args{
				groupEvents: &models.PipelineGroupEvents{Group: mockGroup,
					Events: []models.PipelineEvent{models.ByteArray(mockByteEvent), models.ByteArray(mockByteEvent)}},
				targetFields: []string{"metadata.db"},
			},
			wantStream: [][]byte{append(append(mockByteEvent, "\r\n"...), mockByteEvent...)},
			wantValues: []map[string]string{{"metadata.db": "test"}},
			wantErr:    assert.NoError,
		}, {
			name:   "not join",
			fields: mockFieldsWithoutSep,
			args: args{
				groupEvents: &models.PipelineGroupEvents{Group: mockGroup,
					Events: []models.PipelineEvent{models.ByteArray(mockByteEvent), models.ByteArray(mockByteEvent)}},
				targetFields: []string{"metadata.db"},
			},
			wantStream: [][]byte{mockByteEvent, mockByteEvent},
			wantValues: []map[string]string{{"metadata.db": "test"}, {"metadata.db": "test"}},
			wantErr:    assert.NoError,
		},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			c := &Converter{
				Protocol:             tt.fields.Protocol,
				Encoding:             tt.fields.Encoding,
				Separator:            tt.fields.Separator,
				TagKeyRenameMap:      tt.fields.TagKeyRenameMap,
				ProtocolKeyRenameMap: tt.fields.ProtocolKeyRenameMap,
			}
			gotStream, gotValues, err := c.ConvertToRawStream(tt.args.groupEvents, tt.args.targetFields)
			if !tt.wantErr(t, err, fmt.Sprintf("ConvertToRawStream(%v, %v)", tt.args.groupEvents, tt.args.targetFields)) {
				return
			}
			assert.Equalf(t, tt.wantStream, gotStream, "ConvertToRawStream(%v, %v)", tt.args.groupEvents, tt.args.targetFields)
			assert.Equalf(t, tt.wantValues, gotValues, "ConvertToRawStream(%v, %v)", tt.args.groupEvents, tt.args.targetFields)
		})
	}
}

func Test_findTargetFieldsInGroup(t *testing.T) {
	type args struct {
		targetFields []string
		group        *models.GroupInfo
	}
	tests := []struct {
		name string
		args args
		want map[string]string
	}{
		{name: "find metadata fields",
			args: args{
				targetFields: []string{"metadata.db"},
				group:        models.NewGroup(models.NewMetadataWithMap(map[string]string{"db": "test"}), nil),
			}, want: map[string]string{"metadata.db": "test"},
		},
		{
			name: "find tags fields",
			args: args{
				targetFields: []string{"tag.tagKey"},
				group:        models.NewGroup(nil, models.NewTagsWithMap(map[string]string{"tagKey": "tagValue"})),
			}, want: map[string]string{"tag.tagKey": "tagValue"},
		},
		{name: "can not find metadata fields",
			args: args{
				targetFields: []string{"metadata.db"},
				group:        models.NewGroup(models.NewMetadataWithMap(map[string]string{"nodb": "test"}), nil),
			}, want: map[string]string{"metadata.db": ""},
		},
		{
			name: "can not find tags fields",
			args: args{
				targetFields: []string{"tag.tagKey"},
				group:        models.NewGroup(nil, models.NewTagsWithMap(map[string]string{"noTagKey": "tagValue"})),
			}, want: map[string]string{"tag.tagKey": ""},
		},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			assert.Equalf(t, tt.want, findTargetFieldsInGroup(tt.args.targetFields, tt.args.group), "findTargetFieldsInGroup(%v, %v)", tt.args.targetFields, tt.args.group)
		})
	}
}
