package logger

import (
	"os"

	"github.com/charmbracelet/log"
	"github.com/zfogg/sidechain/cli/pkg/config"
)

var logger *log.Logger

// Init initializes the logger
func Init(verbose bool) {
	// Determine log level
	logLevel := log.InfoLevel
	if verbose {
		logLevel = log.DebugLevel
	}

	// Get log file path from config
	logFile := config.GetString("log.file")

	// Create log file
	f, err := os.OpenFile(logFile, os.O_CREATE|os.O_APPEND|os.O_WRONLY, 0600)
	if err != nil {
		// If we can't create log file, just log to stderr
		f = os.Stderr
	}

	logger = log.New(f)
	logger.SetLevel(logLevel)
}

// Debug logs a debug message
func Debug(msg string, args ...interface{}) {
	if logger != nil {
		logger.Debug(msg, args...)
	}
}

// Info logs an info message
func Info(msg string, args ...interface{}) {
	if logger != nil {
		logger.Info(msg, args...)
	}
}

// Warn logs a warning message
func Warn(msg string, args ...interface{}) {
	if logger != nil {
		logger.Warn(msg, args...)
	}
}

// Error logs an error message
func Error(msg string, args ...interface{}) {
	if logger != nil {
		logger.Error(msg, args...)
	}
}

// Fatal logs a fatal message and exits
func Fatal(msg string, args ...interface{}) {
	if logger != nil {
		logger.Fatal(msg, args...)
	} else {
		os.Exit(1)
	}
}

// GetLogger returns the logger instance
func GetLogger() *log.Logger {
	return logger
}
