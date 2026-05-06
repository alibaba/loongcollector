//go:build windows
// +build windows

package input_etw

import (
	"encoding/json"
	"testing"

	"golang.org/x/sys/windows"

	"github.com/alibaba/ilogtail/plugins/test/mock"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func TestEtwInput_Init_WithProviderName(t *testing.T) {
	input := &EtwInput{
		ProviderName: "Microsoft-Windows-Kernel-Process",
	}
	ctx := mock.NewEmptyContext("test", "test", "test")
	_, err := input.Init(ctx)
	require.NoError(t, err)
	assert.NotEqual(t, windows.GUID{}, input.parsedGUID)
}

func TestEtwInput_Init_WithProviderGUID(t *testing.T) {
	input := &EtwInput{
		ProviderGUID: "{22FB2CD6-0E7B-422B-A0C7-2FAD1FD0E716}",
	}
	ctx := mock.NewEmptyContext("test", "test", "test")
	_, err := input.Init(ctx)
	require.NoError(t, err)
	assert.NotEqual(t, windows.GUID{}, input.parsedGUID)
}

func TestEtwInput_Init_ProviderNameTakesPrecedence(t *testing.T) {
	input := &EtwInput{
		ProviderName: "Microsoft-Windows-Kernel-Process",
		ProviderGUID: "not-a-valid-guid",
	}
	ctx := mock.NewEmptyContext("test", "test", "test")
	_, err := input.Init(ctx)
	require.NoError(t, err)
	assert.NotEqual(t, windows.GUID{}, input.parsedGUID)
}

func TestEtwInput_Init_NoProvider(t *testing.T) {
	input := &EtwInput{}
	ctx := mock.NewEmptyContext("test", "test", "test")
	_, err := input.Init(ctx)
	assert.Error(t, err)
	assert.Contains(t, err.Error(), "either ProviderName or ProviderGUID must be specified")
}

func TestEtwInput_Init_InvalidGUID(t *testing.T) {
	input := &EtwInput{
		ProviderGUID: "not-a-valid-guid",
	}
	ctx := mock.NewEmptyContext("test", "test", "test")
	_, err := input.Init(ctx)
	assert.Error(t, err)
	assert.Contains(t, err.Error(), "invalid GUID")
}

func TestEtwInput_Init_Defaults(t *testing.T) {
	input := &EtwInput{
		ProviderGUID: "{22FB2CD6-0E7B-422B-A0C7-2FAD1FD0E716}",
	}
	ctx := mock.NewEmptyContext("test", "test", "test")
	_, err := input.Init(ctx)
	require.NoError(t, err)
	assert.Equal(t, 4, input.Level)
	assert.Equal(t, uint64(0), uint64(input.Keywords))
}

func TestEtwInput_Description(t *testing.T) {
	input := &EtwInput{}
	assert.NotEmpty(t, input.Description())
}

func TestEtwInput_StopBeforeStart(t *testing.T) {
	input := &EtwInput{
		ProviderGUID: "{22FB2CD6-0E7B-422B-A0C7-2FAD1FD0E716}",
	}
	ctx := mock.NewEmptyContext("test", "test", "test")
	_, err := input.Init(ctx)
	require.NoError(t, err)
	err = input.Stop()
	assert.NoError(t, err)
}

func TestKeywordMask_UnmarshalJSON(t *testing.T) {
	tests := []struct {
		name string
		data string
		want uint64
	}{
		{name: "number", data: `{"Keywords":15}`, want: 15},
		{name: "decimal string", data: `{"Keywords":"15"}`, want: 15},
		{name: "hex string", data: `{"Keywords":"0x8000000000000037"}`, want: 0x8000000000000037},
		{name: "empty string", data: `{"Keywords":""}`, want: 0},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			var input EtwInput
			require.NoError(t, json.Unmarshal([]byte(tt.data), &input))
			assert.Equal(t, tt.want, uint64(input.Keywords))
		})
	}
}

func TestKeywordMask_UnmarshalJSON_Invalid(t *testing.T) {
	var input EtwInput
	err := json.Unmarshal([]byte(`{"Keywords":"not-a-number"}`), &input)
	require.Error(t, err)
	assert.Contains(t, err.Error(), "invalid Keywords")
}

func TestResolveProviderName(t *testing.T) {
	guid, err := resolveProviderName("Microsoft-Windows-Kernel-Process")
	require.NoError(t, err)
	assert.NotEmpty(t, guid)
	assert.Contains(t, guid, "{")
	assert.Contains(t, guid, "}")
}

func TestResolveProviderName_NotFound(t *testing.T) {
	_, err := resolveProviderName("NonExistent-Provider-That-Does-Not-Exist-12345")
	assert.Error(t, err)
	assert.Contains(t, err.Error(), "not found")
}
