package k8smeta

import (
	apierrors "k8s.io/apimachinery/pkg/api/errors"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
)

// Consecutive RBAC/auth (Forbidden/Unauthorized) watch errors before stopping this informer.
// 3 balances a single flake vs. failing fast on permanent bad credentials/RBAC.
const informerAuthFailureStopAfter = 3

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
