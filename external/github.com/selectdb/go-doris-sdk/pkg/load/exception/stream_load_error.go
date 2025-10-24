// Package exception provides error types used in the Doris Stream Load client
package exception

// StreamLoadError represents an error that occurred during a stream load operation
type StreamLoadError struct {
	Message string
}

// Error returns the error message
func (e *StreamLoadError) Error() string {
	return e.Message
}

// NewStreamLoadError creates a new StreamLoadError with the given message
func NewStreamLoadError(message string) *StreamLoadError {
	return &StreamLoadError{
		Message: message,
	}
} 