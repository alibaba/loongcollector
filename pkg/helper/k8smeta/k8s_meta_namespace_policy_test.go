package k8smeta

import (
	"testing"

	v1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/apis/meta/v1/unstructured"
)

func TestNamespacePolicyAllows(t *testing.T) {
	tests := []struct {
		name      string
		black     []string
		white     []string
		ns        string
		wantAllow bool
	}{
		{"empty both", nil, nil, "kube-system", true},
		{"black only drop", []string{"kube-system"}, nil, "kube-system", false},
		{"black only allow", []string{"kube-system"}, nil, "app", true},
		{"white only in", nil, []string{"app"}, "app", true},
		{"white only out", nil, []string{"app"}, "kube-system", false},
		{"both union rescue", []string{"kube-system"}, []string{"kube-system"}, "kube-system", true},
		{"both black blocks", []string{"bad"}, []string{"app"}, "bad", false},
		{"both allow via white", []string{"bad"}, []string{"app"}, "app", true},
		{"both allow via not black", []string{"bad"}, []string{"app"}, "other", true},
	}
	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			p := newNamespacePolicy(tt.black, tt.white)
			if !p.hasBlack && !p.hasWhite {
				if !p.allowsNamespace(tt.ns) {
					t.Fatalf("empty policy should allow")
				}
				return
			}
			if got := p.allowsNamespace(tt.ns); got != tt.wantAllow {
				t.Fatalf("allowsNamespace(%q) = %v, want %v", tt.ns, got, tt.wantAllow)
			}
		})
	}
}

func TestObjectMetaNamespaceForFilter(t *testing.T) {
	pod := &v1.Pod{
		ObjectMeta: metav1.ObjectMeta{Namespace: "app", Name: "p"},
	}
	ns, cluster := ObjectMetaNamespaceForFilter(POD, pod)
	if cluster || ns != "app" {
		t.Fatalf("pod: got ns=%q cluster=%v", ns, cluster)
	}
	nsObj := &v1.Namespace{
		ObjectMeta: metav1.ObjectMeta{Name: "kube-system"},
	}
	ns, cluster = ObjectMetaNamespaceForFilter(NAMESPACE, nsObj)
	if cluster || ns != "kube-system" {
		t.Fatalf("namespace resource: got ns=%q cluster=%v", ns, cluster)
	}
	node := &v1.Node{
		ObjectMeta: metav1.ObjectMeta{Name: "n1"},
	}
	ns, cluster = ObjectMetaNamespaceForFilter(NODE, node)
	if !cluster {
		t.Fatalf("node should be cluster scoped")
	}
	u := &unstructured.Unstructured{}
	u.SetNamespace("cr-ns")
	u.SetName("wf1")
	ns, cluster = ObjectMetaNamespaceForFilter("custom.entity", u)
	if cluster || ns != "cr-ns" {
		t.Fatalf("unstructured: got ns=%q cluster=%v", ns, cluster)
	}
}

func TestMetaManagerNamespacePolicyOR(t *testing.T) {
	m := &MetaManager{}
	id1 := m.RegisterNamespacePolicy([]string{"kube-system"}, nil)
	if id1 < 0 {
		t.Fatal(id1)
	}
	id2 := m.RegisterNamespacePolicy(nil, []string{"app"})
	if id2 < 0 {
		t.Fatal(id2)
	}
	wrap := func(rt string, raw interface{}) *ObjectWrapper {
		return &ObjectWrapper{ResourceType: rt, Raw: raw}
	}
	if !m.MetaObjectPassesNamespacePolicy(wrap(POD, &v1.Pod{
		ObjectMeta: metav1.ObjectMeta{Namespace: "app", Name: "a"},
	})) {
		t.Fatal("app should pass whitelist policy")
	}
	if !m.MetaObjectPassesNamespacePolicy(wrap(POD, &v1.Pod{
		ObjectMeta: metav1.ObjectMeta{Namespace: "default", Name: "a"},
	})) {
		t.Fatal("default should pass via only-black policy (not kube-system)")
	}
	if m.MetaObjectPassesNamespacePolicy(wrap(POD, &v1.Pod{
		ObjectMeta: metav1.ObjectMeta{Namespace: "kube-system", Name: "a"},
	})) {
		t.Fatal("kube-system should be blocked by blacklist policy")
	}
	m.UnregisterNamespacePolicy(id1)
	m.UnregisterNamespacePolicy(id2)
	if len(m.nsPolicyRegs) != 0 {
		t.Fatal("regs should be empty")
	}
}
