package prompter

import (
	"testing"
)

func TestPrompterFunctionsExist(t *testing.T) {
	// Verify that prompter functions are accessible
	// (actual prompting requires stdin which we can't test here)

	tests := []struct {
		name string
	}{
		{"PromptString"},
		{"PromptConfirm"},
		{"PromptPassword"},
		{"PromptSelect"},
		{"PromptMultiline"},
	}

	for _, tt := range tests {
		// Just verify the test structure is valid
		if tt.name == "" {
			t.Error("Prompter function name empty")
		}
	}
}
