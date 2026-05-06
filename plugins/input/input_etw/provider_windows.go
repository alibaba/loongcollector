//go:build windows
// +build windows

package input_etw

import (
	"fmt"
	"strings"

	"golang.org/x/sys/windows/registry"
)

const publishersKeyPath = `SOFTWARE\Microsoft\Windows\CurrentVersion\WINEVT\Publishers`

func resolveProviderName(name string) (string, error) {
	k, err := registry.OpenKey(registry.LOCAL_MACHINE, publishersKeyPath,
		registry.ENUMERATE_SUB_KEYS)
	if err != nil {
		return "", fmt.Errorf("open publishers registry: %w", err)
	}
	defer k.Close()

	guids, err := k.ReadSubKeyNames(-1)
	if err != nil {
		return "", fmt.Errorf("enumerate publishers: %w", err)
	}

	for _, guid := range guids {
		sub, err := registry.OpenKey(k, guid, registry.QUERY_VALUE)
		if err != nil {
			continue
		}
		val, _, err := sub.GetStringValue("")
		sub.Close()
		if err != nil {
			continue
		}
		if strings.EqualFold(val, name) {
			return guid, nil
		}
	}
	return "", fmt.Errorf("ETW provider %q not found in registry", name)
}
