package helper

import (
	"github.com/stretchr/testify/require"

	"strings"
	"testing"
)

func TestGetFileListByPrefix(t *testing.T) {
	files, err := GetFileListByPrefix(".", "mount", false, 0)
	require.NoError(t, err)
	println(strings.Join(files, ","))
}
