package logger

import (
	"testing"
)

func TestLoggerFunctions_NoNilPointers(t *testing.T) {
	defer func() {
		if r := recover(); r != nil {
			t.Errorf("Logger function panicked: %v", r)
		}
	}()

	// Test logger functions (excluding Fatal which exits)
	Debug("test debug", "key", "value")
	Info("test info", "key", "value")
	Warn("test warn", "key", "value")
	Error("test error", "key", "value")

	// Test with no key-value pairs
	Debug("message only")
	Info("message only")
	Warn("message only")
	Error("message only")
}

func TestLoggerWithMultipleArgs(t *testing.T) {
	defer func() {
		if r := recover(); r != nil {
			t.Errorf("Logger with multiple args panicked: %v", r)
		}
	}()

	Debug("test", "key1", "val1", "key2", "val2", "key3", "val3")
	Info("test", "a", 1, "b", 2.5, "c", true)
	Warn("test", "str", "value", "int", 42)
	Error("test", "err", "error message", "code", 500)
}

func TestLoggerWithDifferentTypes(t *testing.T) {
	defer func() {
		if r := recover(); r != nil {
			t.Errorf("Logger with different types panicked: %v", r)
		}
	}()

	Debug("test", "string", "value", "int", 123, "float", 45.67, "bool", true, "nil", nil)
	Info("test", "items", []string{"a", "b"})
	Warn("test", "map", map[string]string{"key": "value"})
}
