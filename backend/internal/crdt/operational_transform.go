package crdt

import (
	"fmt"
	"log"
	"sync"
	"time"
)

// Operation represents a single edit operation in collaborative editing
type Operation struct {
	Type            string `json:"type"`             // "Insert" or "Delete"
	ClientID        int    `json:"client_id"`
	ClientTimestamp int64  `json:"client_timestamp"` // Client's local timestamp
	ServerTimestamp int64  `json:"server_timestamp"` // Server assignment for ordering
	Position        int    `json:"position"`
	Content         string `json:"content,omitempty"` // Text being inserted (empty for Delete)
	Length          int    `json:"length,omitempty"`  // Length for Delete operations
}

// Document represents a collaborative document with full edit history
type Document struct {
	ID               string
	Content          string
	LastTimestamp    int64
	Operations       []*Operation // Full operation history
	PendingOps       []*Operation // Operations waiting for transformation
	mu               sync.RWMutex
	createdAt        time.Time
	lastModifiedAt   time.Time
}

// OperationalTransform handles conflict-free collaborative editing
type OperationalTransform struct {
	documents map[string]*Document // Document ID -> Document
	mu        sync.RWMutex
	nextSeq   int64 // Global sequence number for ordering
}

// NewOperationalTransform creates a new OT instance
func NewOperationalTransform() *OperationalTransform {
	return &OperationalTransform{
		documents: make(map[string]*Document),
		nextSeq:   1,
	}
}

// GetOrCreateDocument gets a document or creates it if it doesn't exist
func (ot *OperationalTransform) GetOrCreateDocument(docID string) *Document {
	ot.mu.Lock()
	defer ot.mu.Unlock()

	if doc, exists := ot.documents[docID]; exists {
		return doc
	}

	doc := &Document{
		ID:        docID,
		Content:   "",
		Operations: make([]*Operation, 0),
		PendingOps: make([]*Operation, 0),
		createdAt: time.Now(),
	}
	ot.documents[docID] = doc
	return doc
}

// ApplyOperation applies an operation to a document, handling transformation for conflicts
func (ot *OperationalTransform) ApplyOperation(docID string, op *Operation) (*Operation, error) {
	doc := ot.GetOrCreateDocument(docID)
	doc.mu.Lock()
	defer doc.mu.Unlock()

	// Assign server timestamp for ordering
	ot.mu.Lock()
	op.ServerTimestamp = ot.nextSeq
	ot.nextSeq++
	ot.mu.Unlock()

	// Transform against all pending operations
	transformedOp := op
	for i, pendingOp := range doc.PendingOps {
		var err error
		transformedOp, doc.PendingOps[i] = Transform(transformedOp, pendingOp, op.ClientID)
		if err != nil {
			log.Printf("Error transforming operation: %v", err)
			return nil, err
		}
	}

	// Apply the transformed operation to document content
	newContent, err := ApplyOperationToContent(doc.Content, transformedOp)
	if err != nil {
		return nil, fmt.Errorf("failed to apply operation: %w", err)
	}

	doc.Content = newContent
	doc.LastTimestamp = op.ServerTimestamp
	doc.lastModifiedAt = time.Now()

	// Add to operation history
	doc.Operations = append(doc.Operations, transformedOp)

	// Remove this operation from pending (it's now confirmed)
	newPendingOps := make([]*Operation, 0)
	for _, pending := range doc.PendingOps {
		if pending.ClientID != op.ClientID || pending.ClientTimestamp != op.ClientTimestamp {
			newPendingOps = append(newPendingOps, pending)
		}
	}
	doc.PendingOps = newPendingOps

	return transformedOp, nil
}

// GetDocumentContent returns the current content of a document
func (ot *OperationalTransform) GetDocumentContent(docID string) string {
	doc := ot.GetOrCreateDocument(docID)
	doc.mu.RLock()
	defer doc.mu.RUnlock()
	return doc.Content
}

// GetOperationHistory returns operations after a given timestamp
func (ot *OperationalTransform) GetOperationHistory(docID string, fromTimestamp int64) []*Operation {
	doc := ot.GetOrCreateDocument(docID)
	doc.mu.RLock()
	defer doc.mu.RUnlock()

	result := make([]*Operation, 0)
	for _, op := range doc.Operations {
		if op.ServerTimestamp > fromTimestamp {
			result = append(result, op)
		}
	}
	return result
}

// Transform performs operational transformation on two concurrent operations
// Returns the transformed version of op1 and op2
// Uses client ID as tie-breaker for operations at same position
func Transform(op1, op2 *Operation, clientID int) (*Operation, *Operation) {
	// If operations don't affect each other, return unchanged
	if !operationsConflict(op1, op2) {
		return op1, op2
	}

	// Transform op1 against op2
	transformedOp1 := transformOperation(op1, op2, clientID)

	// Transform op2 against op1 (after op1 is transformed)
	transformedOp2 := transformOperation(op2, op1, clientID)

	return transformedOp1, transformedOp2
}

// operationsConflict checks if two operations could conflict
func operationsConflict(op1, op2 *Operation) bool {
	// Delete always conflicts (removes content)
	if op1.Type == "Delete" || op2.Type == "Delete" {
		return true
	}

	// Inserts at same or overlapping positions conflict
	return true // Simplified: treat all as potentially conflicting
}

// transformOperation transforms operation op against baseline operation base
// clientID is used as tie-breaker when operations occur at same position
func transformOperation(op *Operation, base *Operation, clientID int) *Operation {
	transformed := &Operation{
		Type:            op.Type,
		ClientID:        op.ClientID,
		ClientTimestamp: op.ClientTimestamp,
		ServerTimestamp: op.ServerTimestamp,
		Position:        op.Position,
		Content:         op.Content,
		Length:          op.Length,
	}

	// Handle Insert vs Insert
	if op.Type == "Insert" && base.Type == "Insert" {
		if base.Position < op.Position {
			// Base insert is before op insert, shift op position right
			transformed.Position += len(base.Content)
		} else if base.Position == op.Position {
			// Same position: use client ID as tie-breaker (lower ID goes first)
			// If our client ID is higher, shift right
			if op.ClientID > base.ClientID {
				transformed.Position += len(base.Content)
			}
		}
		// If base.Position > op.Position, no change needed
	}

	// Handle Insert vs Delete
	if op.Type == "Insert" && base.Type == "Delete" {
		if base.Position < op.Position {
			// Base delete is before op insert
			// Move op left by deleted content length
			transformed.Position -= base.Length
			if transformed.Position < 0 {
				transformed.Position = 0
			}
		} else if base.Position <= op.Position {
			// Base delete overlaps or is at op position
			// Adjust position
			if op.Position >= base.Position {
				transformed.Position -= base.Length
				if transformed.Position < base.Position {
					transformed.Position = base.Position
				}
			}
		}
	}

	// Handle Delete vs Insert
	if op.Type == "Delete" && base.Type == "Insert" {
		if base.Position < op.Position {
			// Base insert is before op delete, shift delete right
			transformed.Position += len(base.Content)
		} else if base.Position < op.Position+op.Length {
			// Base insert is within delete range, extend delete
			transformed.Length += len(base.Content)
		}
		// If base.Position >= op.Position + op.Length, no change
	}

	// Handle Delete vs Delete
	if op.Type == "Delete" && base.Type == "Delete" {
		if base.Position < op.Position {
			// Base delete is before op delete, shift op left
			transformed.Position -= base.Length
			if transformed.Position < 0 {
				transformed.Position = 0
			}
		} else if base.Position < op.Position+op.Length {
			// Base delete overlaps with op delete
			// Reduce delete length
			overlap := (op.Position + op.Length) - base.Position
			if overlap > base.Length {
				overlap = base.Length
			}
			transformed.Length -= overlap
			if transformed.Length < 0 {
				transformed.Length = 0
			}
		}
	}

	return transformed
}

// ApplyOperationToContent applies an operation to document content
func ApplyOperationToContent(content string, op *Operation) (string, error) {
	switch op.Type {
	case "Insert":
		if op.Position < 0 || op.Position > len(content) {
			return "", fmt.Errorf("insert position %d out of bounds for content length %d", op.Position, len(content))
		}
		return content[:op.Position] + op.Content + content[op.Position:], nil

	case "Delete":
		endPos := op.Position + op.Length
		if op.Position < 0 || endPos > len(content) {
			return "", fmt.Errorf("delete range [%d:%d] out of bounds for content length %d", op.Position, endPos, len(content))
		}
		return content[:op.Position] + content[endPos:], nil

	default:
		return "", fmt.Errorf("unknown operation type: %s", op.Type)
	}
}

// GetDocumentStats returns statistics about a document
func (ot *OperationalTransform) GetDocumentStats(docID string) map[string]interface{} {
	doc := ot.GetOrCreateDocument(docID)
	doc.mu.RLock()
	defer doc.mu.RUnlock()

	return map[string]interface{}{
		"id":              doc.ID,
		"content_length":  len(doc.Content),
		"operation_count": len(doc.Operations),
		"pending_count":   len(doc.PendingOps),
		"last_timestamp":  doc.LastTimestamp,
		"created_at":      doc.createdAt,
		"last_modified":   doc.lastModifiedAt,
	}
}
