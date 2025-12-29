package telemetry

import (
	"fmt"
	"strings"
	"time"

	"go.opentelemetry.io/otel"
	"go.opentelemetry.io/otel/attribute"
	"go.opentelemetry.io/otel/codes"
	"go.opentelemetry.io/otel/trace"
	"gorm.io/gorm"
)

const (
	dbSystemKey      = "db.system"
	dbSystemPostgres = "postgresql"
	dbTableKey       = "db.table"
	dbOperationKey   = "db.operation"
	dbStatementKey   = "db.statement"
)

// GORMTracingPlugin returns a GORM plugin that traces database operations
func GORMTracingPlugin() gorm.Plugin {
	return &tracingPlugin{
		tracer: otel.Tracer("gorm"),
	}
}

type tracingPlugin struct {
	tracer trace.Tracer
}

func (p *tracingPlugin) Name() string {
	return "telemetry:tracing"
}

func (p *tracingPlugin) Initialize(db *gorm.DB) error {
	// Register before callbacks
	if err := db.Callback().Query().Before("gorm:query").Register("telemetry:before_query", p.beforeQuery); err != nil {
		return fmt.Errorf("failed to register before_query callback: %w", err)
	}
	if err := db.Callback().Create().Before("gorm:create").Register("telemetry:before_create", p.beforeCreate); err != nil {
		return fmt.Errorf("failed to register before_create callback: %w", err)
	}
	if err := db.Callback().Update().Before("gorm:update").Register("telemetry:before_update", p.beforeUpdate); err != nil {
		return fmt.Errorf("failed to register before_update callback: %w", err)
	}
	if err := db.Callback().Delete().Before("gorm:delete").Register("telemetry:before_delete", p.beforeDelete); err != nil {
		return fmt.Errorf("failed to register before_delete callback: %w", err)
	}

	// Register after callbacks
	if err := db.Callback().Query().After("gorm:query").Register("telemetry:after_query", p.afterQuery); err != nil {
		return fmt.Errorf("failed to register after_query callback: %w", err)
	}
	if err := db.Callback().Create().After("gorm:create").Register("telemetry:after_create", p.afterQuery); err != nil {
		return fmt.Errorf("failed to register after_create callback: %w", err)
	}
	if err := db.Callback().Update().After("gorm:update").Register("telemetry:after_update", p.afterQuery); err != nil {
		return fmt.Errorf("failed to register after_update callback: %w", err)
	}
	if err := db.Callback().Delete().After("gorm:delete").Register("telemetry:after_delete", p.afterQuery); err != nil {
		return fmt.Errorf("failed to register after_delete callback: %w", err)
	}

	return nil
}

func (p *tracingPlugin) beforeQuery(db *gorm.DB) {
	p.startSpan(db, "SELECT")
}

func (p *tracingPlugin) beforeCreate(db *gorm.DB) {
	p.startSpan(db, "INSERT")
}

func (p *tracingPlugin) beforeUpdate(db *gorm.DB) {
	p.startSpan(db, "UPDATE")
}

func (p *tracingPlugin) beforeDelete(db *gorm.DB) {
	p.startSpan(db, "DELETE")
}

func (p *tracingPlugin) startSpan(db *gorm.DB, operation string) {
	ctx := db.Statement.Context
	if ctx == nil {
		return
	}

	table := db.Statement.Table
	if table == "" {
		table = "unknown"
	}

	_, span := p.tracer.Start(ctx, fmt.Sprintf("db.%s", strings.ToLower(operation)),
		trace.WithAttributes(
			attribute.String(dbSystemKey, dbSystemPostgres),
			attribute.String(dbTableKey, table),
			attribute.String(dbOperationKey, operation),
		),
	)

	db.InstanceSet("otel:span", span)
	db.InstanceSet("otel:startTime", time.Now())
}

func (p *tracingPlugin) afterQuery(db *gorm.DB) {
	spanRaw, exists := db.InstanceGet("otel:span")
	if !exists {
		return
	}

	span, ok := spanRaw.(trace.Span)
	if !ok {
		return
	}
	defer span.End()

	// Record duration
	if startTimeRaw, exists := db.InstanceGet("otel:startTime"); exists {
		if startTime, ok := startTimeRaw.(time.Time); ok {
			duration := time.Since(startTime).Milliseconds()
			span.SetAttributes(attribute.Int64("db.duration_ms", duration))
		}
	}

	// Record SQL statement (sanitized)
	if db.Statement.SQL.String() != "" {
		// Truncate very long queries to avoid cardinality explosion
		sql := db.Statement.SQL.String()
		if len(sql) > 500 {
			sql = sql[:500] + "... (truncated)"
		}
		span.SetAttributes(attribute.String(dbStatementKey, sql))
	}

	// Record rows affected
	if db.RowsAffected > 0 {
		span.SetAttributes(attribute.Int64("db.rows_affected", db.RowsAffected))
	}

	// Record error if any
	if db.Error != nil {
		span.SetStatus(codes.Error, db.Error.Error())
		span.RecordError(db.Error, trace.WithStackTrace(true))
	}
}
