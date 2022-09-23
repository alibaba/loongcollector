package helper

import (
	"testing"

	"github.com/stretchr/testify/assert"
)

func TestExtractPodWorkload(t *testing.T) {
	type args struct {
		name string
	}
	tests := []struct {
		name string
		args args
		want string
	}{
		{
			name: "pod",
			args: args{
				name: "kube-state-metrics-86679c9454-5rmck",
			},
			want: "kube-state-metrics",
		},

		{
			name: "set",
			args: args{
				name: "kube-state-metrics-5rmck",
			},
			want: "kube-state-metrics",
		},
		{
			name: "statefulset",
			args: args{
				name: "kube-state-metrics-0",
			},
			want: "kube-state-metrics",
		},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			assert.Equalf(t, tt.want, ExtractPodWorkload(tt.args.name), "ExtractPodWorkload(%v)", tt.args.name)
		})
	}
}

func TestExtractStatefulSetNum(t *testing.T) {
	num := ExtractStatefulSetNum("kube-state-metrics-1")
	assert.Equal(t, num, 1)
	num = ExtractStatefulSetNum("kube-state-metrics-1-1")
	assert.Equal(t, num, 1)
}
