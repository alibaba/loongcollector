package helper

import (
	"testing"

	"github.com/stretchr/testify/require"
)

func TestReverseStringSlice(t *testing.T) {
	ss := []string{"a", "b"}
	ReverseStringSlice(ss)
	require.Equal(t, ss, []string{"b", "a"})
}
