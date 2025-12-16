package prompter

import (
	"bufio"
	"fmt"
	"os"
	"strings"
	"syscall"

	"golang.org/x/term"
)

// PromptString prompts user for a string input
func PromptString(label string) (string, error) {
	fmt.Print(label)
	reader := bufio.NewReader(os.Stdin)
	input, err := reader.ReadString('\n')
	if err != nil {
		return "", err
	}
	return strings.TrimSpace(input), nil
}

// PromptPassword prompts user for a password (hidden input)
func PromptPassword(label string) (string, error) {
	fmt.Print(label)

	// Read password without echoing
	bytepw, err := term.ReadPassword(int(syscall.Stdin))
	if err != nil {
		return "", err
	}

	fmt.Println() // New line after password input

	return string(bytepw), nil
}

// PromptConfirm prompts user for yes/no confirmation
func PromptConfirm(label string) (bool, error) {
	fmt.Print(label + " (y/n) ")
	reader := bufio.NewReader(os.Stdin)
	input, err := reader.ReadString('\n')
	if err != nil {
		return false, err
	}

	response := strings.TrimSpace(strings.ToLower(input))
	return response == "y" || response == "yes", nil
}

// PromptSelect prompts user to select from options
func PromptSelect(label string, options []string) (int, error) {
	fmt.Println(label)
	for i, opt := range options {
		fmt.Printf("%d) %s\n", i+1, opt)
	}

	fmt.Print("Select option: ")
	reader := bufio.NewReader(os.Stdin)
	input, err := reader.ReadString('\n')
	if err != nil {
		return -1, err
	}

	input = strings.TrimSpace(input)

	var selection int
	_, err = fmt.Sscanf(input, "%d", &selection)
	if err != nil {
		return -1, err
	}

	if selection < 1 || selection > len(options) {
		return -1, fmt.Errorf("invalid selection")
	}

	return selection - 1, nil
}

// PromptMultilineString prompts user for multi-line input
func PromptMultilineString(label string, maxLines int) (string, error) {
	fmt.Printf("%s (Enter %d empty lines to finish):\n", label, 1)

	reader := bufio.NewReader(os.Stdin)
	var lines []string
	emptyCount := 0

	for i := 0; i < maxLines; i++ {
		line, err := reader.ReadString('\n')
		if err != nil {
			return "", err
		}

		trimmedLine := strings.TrimRight(line, "\n")

		if trimmedLine == "" {
			emptyCount++
			if emptyCount >= 1 {
				break
			}
		} else {
			emptyCount = 0
			lines = append(lines, trimmedLine)
		}
	}

	return strings.Join(lines, "\n"), nil
}
