package sls

// SubStoreKey key define
type SubStoreKey struct {
	Name string `json:"name"`
	Type string `json:"type"`
}

// IsValid ...
func (s *SubStoreKey) IsValid() bool {
	if len(s.Name) == 0 {
		return false
	}
	if s.Type != "text" &&
		s.Type != "long" &&
		s.Type != "double" {
		return false
	}
	return true
}

// SubStore define
type SubStore struct {
	Name           string        `json:"name,omitempty"`
	TTL            int           `json:"ttl"`
	SortedKeyCount int           `json:"sortedKeyCount"`
	TimeIndex      int           `json:"timeIndex"`
	Keys           []SubStoreKey `json:"keys"`
}

// NewSubStore create a new sorted sub store
func NewSubStore(name string,
	ttl int,
	sortedKeyCount int,
	timeIndex int,
	keys []SubStoreKey) *SubStore {
	sss := &SubStore{
		Name:           name,
		TTL:            ttl,
		SortedKeyCount: sortedKeyCount,
		TimeIndex:      timeIndex,
		Keys:           keys,
	}
	if sss.IsValid() {
		return sss
	}
	return nil
}

// IsValid ...
func (s *SubStore) IsValid() bool {
	if s.SortedKeyCount <= 0 || s.SortedKeyCount >= len(s.Keys) {
		return false
	}
	if s.TimeIndex >= len(s.Keys) || s.TimeIndex < s.SortedKeyCount {
		return false
	}
	if s.TTL <= 0 || s.TTL > 3650 {
		return false
	}
	for index, key := range s.Keys {
		if !key.IsValid() {
			return false
		}
		if index == s.TimeIndex && key.Type != "long" {
			return false
		}
		if index < s.SortedKeyCount && key.Type == "double" {
			return false
		}
	}
	return true
}
