package k8smeta

import (
	apierrors "k8s.io/apimachinery/pkg/api/errors"
	"k8s.io/apimachinery/pkg/api/meta"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
)

// informerGiveUpFailureThreshold is how many consecutive Reflector ListAndWatch errors
// of a “give up” class (RBAC, missing API resource, etc.) trigger stopping the informer factory.
const informerGiveUpFailureThreshold = 3

func isInformerAuthFailure(err error) bool {
	if err == nil {
		return false
	}
	if apierrors.IsForbidden(err) || apierrors.IsUnauthorized(err) {
		return true
	}
	switch apierrors.ReasonForError(err) {
	case metav1.StatusReasonForbidden, metav1.StatusReasonUnauthorized:
		return true
	default:
		return false
	}
}

// isInformerPermanentResourceFailure matches Reflector ListAndWatch errors that will not
// recover without cluster changes (e.g. CRD not installed: "the server could not find the requested resource").
func isInformerPermanentResourceFailure(err error) bool {
	if err == nil {
		return false
	}
	if apierrors.IsNotFound(err) {
		return true
	}
	if meta.IsNoMatchError(err) {
		return true
	}
	switch apierrors.ReasonForError(err) {
	case metav1.StatusReasonNotFound:
		return true
	default:
		return false
	}
}

func isInformerGiveUpFailure(err error) bool {
	return isInformerAuthFailure(err) || isInformerPermanentResourceFailure(err)
}
