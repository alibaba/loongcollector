package models

import (
	"reflect"
	"testing"
	"unsafe"

	"github.com/stretchr/testify/assert"
)

func Test_Tags_SortTo(t *testing.T) {
	tests := []struct {
		caseName      string
		tags          map[string]string
		buf           []KeyValue[string]
		want          []KeyValue[string]
		wantOriginBuf bool
	}{
		{
			caseName: "empty tags empty buf",
			tags:     nil,
			buf:      nil,
			want:     nil,
		},
		{
			caseName: "empty tags none empty buf",
			tags:     nil,
			buf:      []KeyValue[string]{{Key: "a", Value: "b"}},
			want:     []KeyValue[string]{},
		},
		{
			caseName: "none empty tags empty buf",
			tags:     map[string]string{"b": "b", "a": "a"},
			buf:      nil,
			want:     []KeyValue[string]{{Key: "a", Value: "a"}, {Key: "b", Value: "b"}},
		},
		{
			caseName:      "none empty tags none empty buf enough len",
			tags:          map[string]string{"b": "b", "a": "a"},
			buf:           []KeyValue[string]{{Key: "c", Value: "c"}, {Key: "c", Value: "c"}, {Key: "c", Value: "c"}},
			want:          []KeyValue[string]{{Key: "a", Value: "a"}, {Key: "b", Value: "b"}},
			wantOriginBuf: true,
		},
		{
			caseName: "none empty tags none empty buf not enough len",
			tags:     map[string]string{"b": "b", "a": "a"},
			buf:      []KeyValue[string]{{Key: "c", Value: "c"}},
			want:     []KeyValue[string]{{Key: "a", Value: "a"}, {Key: "b", Value: "b"}},
		},
	}

	for _, cases := range tests {
		tags := NewTagsWithMap(cases.tags)
		result := tags.SortTo(cases.buf)
		assert.ElementsMatchf(t, result, cases.want, cases.caseName)
		if cases.wantOriginBuf {
			assert.Equalf(t, (*reflect.SliceHeader)(unsafe.Pointer(&cases.buf)).Data, (*reflect.SliceHeader)(unsafe.Pointer(&result)).Data, cases.caseName)
		}
	}
}
