package output

import (
	"testing"
)

func TestGetOutputFormat(t *testing.T) {
	format := GetOutputFormat()
	if format != FormatJSON && format != FormatText && format != FormatTable {
		t.Errorf("Invalid output format: %v", format)
	}
}

func TestValidateOutputFormat(t *testing.T) {
	tests := []struct {
		format  string
		isValid bool
	}{
		{"json", true},
		{"text", true},
		{"table", true},
		{"invalid", false},
	}

	for _, tt := range tests {
		result := ValidateOutputFormat(tt.format)
		if result != tt.isValid {
			t.Errorf("ValidateOutputFormat(%s): got %v, want %v", tt.format, result, tt.isValid)
		}
	}
}

func TestPrintFunctions_NoNilPointers(t *testing.T) {
	defer func() {
		if r := recover(); r != nil {
			t.Errorf("Print function panicked: %v", r)
		}
	}()

	data := map[string]interface{}{
		"name": "test",
		"id":   123,
		"tags": []string{"a", "b"},
	}

	Print("Test Data", data)
	PrintRecord("Record", data)
	PrintSuccess("Operation completed")
	PrintError("Operation failed")

	items := []map[string]interface{}{
		{"name": "item1", "id": 1},
		{"name": "item2", "id": 2},
	}
	PrintList("Items", items, []string{"name", "id"})
}
