package k8smeta

import "k8s.io/apimachinery/pkg/labels"

type labelMatcher struct {
	obj      interface{}
	selector labels.Selector
}

type labelMatchers []*labelMatcher

// newLabelMatcher create a new label matcher.
func newLabelMatcher(obj interface{}, selector labels.Selector) *labelMatcher {
	return &labelMatcher{
		obj, selector,
	}
}
