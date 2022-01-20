package metrics

import (
	"strings"
)

func Clear(name string) {
	set := make(map[string]struct{}, 16)
	defaultSet.mu.Lock()
	for _, a := range defaultSet.a {
		if strings.Contains(a.name, name) {
			set[a.name] = struct{}{}
		}
	}
	for k := range defaultSet.m {
		_, ok := set[k]
		if ok {
			continue
		}
		if strings.Contains(k, name) {
			set[k] = struct{}{}
		}
	}
	defaultSet.mu.Unlock()

	for k := range set {
		UnregisterMetric(k)
	}
}
