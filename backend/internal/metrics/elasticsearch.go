package metrics

import (
	"github.com/prometheus/client_golang/prometheus"
	"github.com/prometheus/client_golang/prometheus/promauto"
)

// Elasticsearch metrics for monitoring search performance and health
var (
	// Query metrics
	ElasticsearchQueryDuration = promauto.NewHistogramVec(
		prometheus.HistogramOpts{
			Name:    "elasticsearch_query_duration_seconds",
			Help:    "Duration of Elasticsearch queries in seconds",
			Buckets: []float64{0.001, 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1, 2.5, 5},
		},
		[]string{"index", "operation", "status"},
	)

	ElasticsearchQueriesTotal = promauto.NewCounterVec(
		prometheus.CounterOpts{
			Name: "elasticsearch_queries_total",
			Help: "Total number of Elasticsearch queries",
		},
		[]string{"index", "operation", "status"},
	)

	ElasticsearchErrorsTotal = promauto.NewCounterVec(
		prometheus.CounterOpts{
			Name: "elasticsearch_errors_total",
			Help: "Total number of Elasticsearch errors",
		},
		[]string{"index", "operation", "error_type"},
	)

	// Index operation metrics
	ElasticsearchIndexOperationsTotal = promauto.NewCounterVec(
		prometheus.CounterOpts{
			Name: "elasticsearch_index_operations_total",
			Help: "Total number of Elasticsearch index operations",
		},
		[]string{"index", "operation"},
	)

	ElasticsearchIndexOperationDuration = promauto.NewHistogramVec(
		prometheus.HistogramOpts{
			Name:    "elasticsearch_index_operation_duration_seconds",
			Help:    "Duration of Elasticsearch index operations",
			Buckets: []float64{0.01, 0.05, 0.1, 0.25, 0.5, 1, 2.5, 5},
		},
		[]string{"index", "operation"},
	)

	// Document count gauge
	ElasticsearchDocumentCount = promauto.NewGaugeVec(
		prometheus.GaugeOpts{
			Name: "elasticsearch_document_count",
			Help: "Number of documents in each index",
		},
		[]string{"index"},
	)

	// Connection health
	ElasticsearchConnectionErrors = promauto.NewCounter(
		prometheus.CounterOpts{
			Name: "elasticsearch_connection_errors_total",
			Help: "Total number of Elasticsearch connection errors",
		},
	)

	// Reconciliation metrics
	ElasticsearchReconciliationRuns = promauto.NewCounter(
		prometheus.CounterOpts{
			Name: "elasticsearch_reconciliation_runs_total",
			Help: "Total number of reconciliation runs",
		},
	)

	ElasticsearchReconciliationDrift = promauto.NewGaugeVec(
		prometheus.GaugeOpts{
			Name: "elasticsearch_reconciliation_drift",
			Help: "Number of documents resynced in last reconciliation",
		},
		[]string{"index"},
	)

	ElasticsearchReconciliationDuration = promauto.NewHistogramVec(
		prometheus.HistogramOpts{
			Name:    "elasticsearch_reconciliation_duration_seconds",
			Help:    "Duration of reconciliation runs in seconds",
			Buckets: []float64{1, 5, 10, 30, 60, 120, 300, 600},
		},
		[]string{"index"},
	)
)
