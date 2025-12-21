package container

import (
	"fmt"
	"strings"
)

// InitializationError indicates that required dependencies are missing
type InitializationError struct {
	Message     string
	MissingDeps []string
}

// NewInitializationError creates a new initialization error
func NewInitializationError(message string, missingDeps []string) *InitializationError {
	return &InitializationError{
		Message:     message,
		MissingDeps: missingDeps,
	}
}

// Error implements the error interface
func (e *InitializationError) Error() string {
	if len(e.MissingDeps) == 0 {
		return e.Message
	}
	return fmt.Sprintf("%s: %s", e.Message, strings.Join(e.MissingDeps, ", "))
}

// DependencyNotFoundError indicates that a dependency was not found
type DependencyNotFoundError struct {
	DependencyName string
}

// NewDependencyNotFoundError creates a new dependency not found error
func NewDependencyNotFoundError(name string) *DependencyNotFoundError {
	return &DependencyNotFoundError{
		DependencyName: name,
	}
}

// Error implements the error interface
func (e *DependencyNotFoundError) Error() string {
	return fmt.Sprintf("dependency not found: %s", e.DependencyName)
}
