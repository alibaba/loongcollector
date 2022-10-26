package helper

import (
	"strconv"

	"github.com/alibaba/ilogtail/pkg/protocol"
)

var FileOffsetKey = "__tag__:__file_offset__"

func ReviseFileOffset(log *protocol.Log, offset int64, enableMeta bool) {
	if enableMeta {
		cont := GetFileOffsetTag(log)
		if cont != nil {
			if beginOffset, err := strconv.ParseInt(cont.Value, 10, 64); err == nil {
				cont.Value = strconv.FormatInt(beginOffset+offset, 10)
			}
		}
	}
}

func GetFileOffsetTag(log *protocol.Log) *protocol.Log_Content {
	for i := len(log.Contents) - 1; i >= 0; i-- {
		cont := log.Contents[i]
		if cont.Key == FileOffsetKey {
			return cont
		}
	}
	return nil
}
