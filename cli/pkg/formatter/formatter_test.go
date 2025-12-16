package formatter

import (
	"testing"
)

func TestPrintSuccess(t *testing.T) {
	// Test that function doesn't panic
	defer func() {
		if r := recover(); r != nil {
			t.Errorf("PrintSuccess panicked: %v", r)
		}
	}()
	PrintSuccess("test success message")
}

func TestPrintError(t *testing.T) {
	defer func() {
		if r := recover(); r != nil {
			t.Errorf("PrintError panicked: %v", r)
		}
	}()
	PrintError("test error message")
}

func TestPrintInfo(t *testing.T) {
	defer func() {
		if r := recover(); r != nil {
			t.Errorf("PrintInfo panicked: %v", r)
		}
	}()
	PrintInfo("test info message")
}

func TestPrintWarning(t *testing.T) {
	defer func() {
		if r := recover(); r != nil {
			t.Errorf("PrintWarning panicked: %v", r)
		}
	}()
	PrintWarning("test warning message")
}

func TestPrintKeyValue(t *testing.T) {
	defer func() {
		if r := recover(); r != nil {
			t.Errorf("PrintKeyValue panicked: %v", r)
		}
	}()

	data := map[string]interface{}{
		"key1": "value1",
		"key2": 123,
		"key3": true,
	}
	PrintKeyValue(data)
}

func TestPrintTable(t *testing.T) {
	defer func() {
		if r := recover(); r != nil {
			t.Errorf("PrintTable panicked: %v", r)
		}
	}()

	headers := []string{"Name", "Age", "Email"}
	rows := [][]string{
		{"Alice", "25", "alice@example.com"},
		{"Bob", "30", "bob@example.com"},
	}
	PrintTable(headers, rows)
}

func TestPrintTable_Empty(t *testing.T) {
	defer func() {
		if r := recover(); r != nil {
			t.Errorf("PrintTable with empty rows panicked: %v", r)
		}
	}()

	headers := []string{"Name", "Age"}
	rows := [][]string{}
	PrintTable(headers, rows)
}

func TestPrintTable_Mismatched(t *testing.T) {
	defer func() {
		if r := recover(); r != nil {
			t.Errorf("PrintTable with mismatched columns panicked: %v", r)
		}
	}()

	headers := []string{"Name", "Age"}
	rows := [][]string{
		{"Alice", "25", "extra"},
	}
	PrintTable(headers, rows)
}

func TestPrintJSON(t *testing.T) {
	defer func() {
		if r := recover(); r != nil {
			t.Errorf("PrintJSON panicked: %v", r)
		}
	}()
	data := map[string]interface{}{"test": "data"}
	PrintJSON(data)
}

func TestPrintObject(t *testing.T) {
	defer func() {
		if r := recover(); r != nil {
			t.Errorf("PrintObject panicked: %v", r)
		}
	}()
	data := map[string]interface{}{"name": "test"}
	PrintObject(data, "TestObject")
}
