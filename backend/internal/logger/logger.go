package logger

import (
	"os"
	"strings"

	"go.uber.org/zap"
	"go.uber.org/zap/zapcore"
	"gopkg.in/natefinch/lumberjack.v2"
)

// Log is the global logger instance
var Log *zap.Logger

// SugaredLog is a sugared logger for printf-style logging (optional, for gradual migration)
var SugaredLog *zap.SugaredLogger

// Initialize sets up the structured logger with file rotation
// logLevel: "debug", "info", "warn", "error" (default: "info")
// logFile: path to log file (default: "server.log")
func Initialize(logLevel string, logFile string) error {
	if logFile == "" {
		logFile = "server.log"
	}

	if logLevel == "" {
		logLevel = "info"
	}

	// Parse log level
	level := parseLogLevel(logLevel)

	// Configure file output with rotation
	fileWriter := zapcore.AddSync(&lumberjack.Logger{
		Filename:   logFile,
		MaxSize:    100, // megabytes
		MaxBackups: 5,   // keep 5 old files
		MaxAge:     7,   // keep for 7 days
		Compress:   true,
	})

	// Console encoder (human-readable for development)
	consoleEncoder := zapcore.NewConsoleEncoder(zap.NewDevelopmentEncoderConfig())

	// JSON encoder (machine-readable for production)
	jsonEncoderConfig := zap.NewProductionEncoderConfig()
	jsonEncoderConfig.EncodeTime = zapcore.ISO8601TimeEncoder
	jsonEncoder := zapcore.NewJSONEncoder(jsonEncoderConfig)

	// Create cores for both outputs
	consoleCore := zapcore.NewCore(
		consoleEncoder,
		zapcore.AddSync(os.Stdout),
		level,
	)

	fileCore := zapcore.NewCore(
		jsonEncoder,
		fileWriter,
		level,
	)

	// Combine cores
	core := zapcore.NewTee(consoleCore, fileCore)

	// Create logger
	Log = zap.New(core, zap.AddCaller(), zap.AddStacktrace(zapcore.ErrorLevel))
	SugaredLog = Log.Sugar()

	Log.Info("Logger initialized",
		zap.String("level", logLevel),
		zap.String("file", logFile),
	)

	return nil
}

// Close flushes the logger before shutdown
func Close() error {
	if Log != nil {
		return Log.Sync()
	}
	return nil
}

// parseLogLevel converts string to zapcore.Level
func parseLogLevel(levelStr string) zapcore.Level {
	switch strings.ToLower(levelStr) {
	case "debug":
		return zapcore.DebugLevel
	case "info":
		return zapcore.InfoLevel
	case "warn", "warning":
		return zapcore.WarnLevel
	case "error":
		return zapcore.ErrorLevel
	default:
		return zapcore.InfoLevel
	}
}

// Helper functions for common logging patterns

// InfoWithFields logs an info message with structured fields
func InfoWithFields(msg string, fields ...zap.Field) {
	Log.Info(msg, fields...)
}

// Warn logs a warning message, optionally with an error
func Warn(msg string, fields ...zap.Field) {
	Log.Warn(msg, fields...)
}

// WarnWithFields logs a warning message with structured fields
func WarnWithFields(msg string, err error) {
	if err != nil {
		Log.Warn(msg, zap.Error(err))
	} else {
		Log.Warn(msg)
	}
}

// ErrorWithFields logs an error message with an error
func ErrorWithFields(msg string, err error) {
	if err != nil {
		Log.Error(msg, zap.Error(err))
	} else {
		Log.Error(msg)
	}
}

// Error logs an error with structured fields
func Error(msg string, fields ...zap.Field) {
	Log.Error(msg, fields...)
}

// DebugWithFields logs a debug message with structured fields
func DebugWithFields(msg string, fields ...zap.Field) {
	Log.Debug(msg, fields...)
}

// FatalWithFields logs a fatal error and exits
func FatalWithFields(msg string, err error) {
	if err != nil {
		Log.Fatal(msg, zap.Error(err))
	} else {
		Log.Fatal(msg)
	}
}

// Deprecated: Printf-style logging (for gradual migration, not recommended)
func Infof(format string, args ...interface{}) {
	SugaredLog.Infof(format, args...)
}

func Warnf(format string, args ...interface{}) {
	SugaredLog.Warnf(format, args...)
}

func Errorf(format string, args ...interface{}) {
	SugaredLog.Errorf(format, args...)
}

func Debugf(format string, args ...interface{}) {
	SugaredLog.Debugf(format, args...)
}

func Fatalf(format string, args ...interface{}) {
	SugaredLog.Fatalf(format, args...)
}

// Printf provides printf-style logging for compatibility (not recommended for new code)
func Printf(format string, args ...interface{}) {
	SugaredLog.Infof(format, args...)
}

// GetFieldsFromContext returns zap fields commonly needed in handlers
func WithRequestID(requestID string) zap.Field {
	return zap.String("request_id", requestID)
}

func WithUserID(userID string) zap.Field {
	return zap.String("user_id", userID)
}

func WithPostID(postID string) zap.Field {
	return zap.String("post_id", postID)
}

func WithIP(ip string) zap.Field {
	return zap.String("ip", ip)
}

func WithStatus(status int) zap.Field {
	return zap.Int("status", status)
}

func WithDuration(duration interface{}) zap.Field {
	return zap.Any("duration", duration)
}
