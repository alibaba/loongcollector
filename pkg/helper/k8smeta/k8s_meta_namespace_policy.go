package k8smeta

import (
	"strings"

	"k8s.io/apimachinery/pkg/api/meta"
	"k8s.io/apimachinery/pkg/apis/meta/v1/unstructured"
)

// namespacePolicy is one pipeline's NamespaceBlackList / NamespaceWhiteList.
// - Neither list: allows all namespaces (such a policy is not registered).
// - BlackList only: allow ns ∉ BlackList.
// - WhiteList only: allow ns ∈ WhiteList.
// - Both: union semantics — allow if ns ∉ BlackList OR ns ∈ WhiteList.
type namespacePolicy struct {
	black    map[string]struct{}
	white    map[string]struct{}
	hasBlack bool
	hasWhite bool
}

func newNamespacePolicy(blackList, whiteList []string) *namespacePolicy {
	p := &namespacePolicy{}
	for _, s := range blackList {
		s = strings.TrimSpace(s)
		if s == "" {
			continue
		}
		if p.black == nil {
			p.black = make(map[string]struct{})
		}
		p.black[s] = struct{}{}
		p.hasBlack = true
	}
	for _, s := range whiteList {
		s = strings.TrimSpace(s)
		if s == "" {
			continue
		}
		if p.white == nil {
			p.white = make(map[string]struct{})
		}
		p.white[s] = struct{}{}
		p.hasWhite = true
	}
	return p
}

func (p *namespacePolicy) allowsNamespace(ns string) bool {
	if !p.hasBlack && !p.hasWhite {
		return true
	}
	inB := p.hasBlack && p.black != nil && containsSet(p.black, ns)
	inW := p.hasWhite && p.white != nil && containsSet(p.white, ns)
	if !p.hasBlack {
		return inW
	}
	if !p.hasWhite {
		return !inB
	}
	// Allow if: (1) not in blacklist, OR (2) in whitelist
	return !inB || inW
}

func containsSet(m map[string]struct{}, ns string) bool {
	_, ok := m[ns]
	return ok
}

// ObjectMetaNamespaceForFilter returns the namespace string used for policy checks and whether the object is cluster-scoped (no namespace filtering).
func ObjectMetaNamespaceForFilter(resourceType string, raw interface{}) (ns string, clusterScoped bool) {
	if raw == nil {
		return "", true
	}
	switch t := raw.(type) {
	case *unstructured.Unstructured:
		if resourceType == NAMESPACE {
			return t.GetName(), false
		}
		n := t.GetNamespace()
		if n == "" {
			return "", true
		}
		return n, false
	default:
		switch resourceType {
		case NODE, PERSISTENTVOLUME, STORAGECLASS:
			return "", true
		case NAMESPACE:
			acc, err := meta.Accessor(raw)
			if err != nil {
				return "", true
			}
			return acc.GetName(), false
		}
		acc, err := meta.Accessor(raw)
		if err != nil {
			return "", true
		}
		n := acc.GetNamespace()
		if n == "" {
			return "", true
		}
		return n, false
	}
}

type nsPolicyReg struct {
	id int
	p  *namespacePolicy
}

// RegisterNamespacePolicy registers one input's namespace rules. Multiple inputs are combined with OR:
// a namespace passes if any registered policy allows it. Returns -1 when both lists are empty (nothing registered).
// Unregister non-negative ids with UnregisterNamespacePolicy on stop.
func (m *MetaManager) RegisterNamespacePolicy(blackList, whiteList []string) int {
	p := newNamespacePolicy(blackList, whiteList)
	if !p.hasBlack && !p.hasWhite {
		return -1
	}
	m.nsPolicyMu.Lock()
	defer m.nsPolicyMu.Unlock()
	id := m.nextNsPolicyID
	m.nextNsPolicyID++
	m.nsPolicyRegs = append(m.nsPolicyRegs, nsPolicyReg{id: id, p: p})
	return id
}

// UnregisterNamespacePolicy removes a policy registered with RegisterNamespacePolicy. Pass id -1 for no-op.
func (m *MetaManager) UnregisterNamespacePolicy(id int) {
	if id < 0 {
		return
	}
	m.nsPolicyMu.Lock()
	defer m.nsPolicyMu.Unlock()
	out := make([]nsPolicyReg, 0, len(m.nsPolicyRegs))
	for _, r := range m.nsPolicyRegs {
		if r.id != id {
			out = append(out, r)
		}
	}
	m.nsPolicyRegs = out
}

// MetaObjectPassesNamespacePolicy returns whether the object may enter the meta cache or be broadcast (add/update/delete).
func (m *MetaManager) MetaObjectPassesNamespacePolicy(o *ObjectWrapper) bool {
	if o == nil || o.Raw == nil {
		return true
	}
	if _, ok := o.Raw.(*TimerEvent); ok {
		return true
	}
	ns, clusterScoped := ObjectMetaNamespaceForFilter(o.ResourceType, o.Raw)
	if clusterScoped {
		return true
	}
	m.nsPolicyMu.RLock()
	regs := m.nsPolicyRegs
	m.nsPolicyMu.RUnlock()
	if len(regs) == 0 {
		return true
	}
	for _, r := range regs {
		if r.p.allowsNamespace(ns) {
			return true
		}
	}
	return false
}
