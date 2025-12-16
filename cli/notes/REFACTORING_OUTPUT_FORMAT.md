# Output Format Refactoring Plan

**Objective**: Implement comprehensive `--output-format` (or `--output`) flag support across all CLI commands

**Current Status**:
- ✅ Root command has `--output` flag
- ✅ Config package has default values
- ⚠️ Formatter doesn't fully support json/table/text for all output types
- ❌ Root command doesn't set the flag value in config
- ❌ Services don't know about output format

**Goal**: Make all 70+ commands respect the `--output` flag for consistent formatting

---

## Architecture Overview

```
Command Layer
    ↓
PersistentPreRun sets config.SetString("output.format", outputFmt)
    ↓
Service Layer
    ↓
Formatter Package (reads from config)
    ↓
Output (JSON / Table / Text)
```

---

## Changes Required

### Phase 1: Fix Root Command & Config Integration

**File**: `internal/cmd/root.go`
- [x] Already has `--output` flag defined (line 71)
- [ ] Update `PersistentPreRun` to call `config.SetString("output.format", outputFmt)`
- [ ] Add flag validation in `init()` to ensure only valid formats

**Impact**: 1 file, 5-10 lines

### Phase 2: Enhance Formatter Package

**File**: `pkg/formatter/formatter.go`

Create unified output functions that handle all three formats:

#### Functions to Create:
1. **PrintList()** - For lists/arrays
   - `json` → Pretty-printed JSON array
   - `table` → Table with headers
   - `text` → One item per line with formatting

2. **PrintRecord()** - For single objects/maps
   - `json` → Pretty-printed JSON object
   - `table` → Two-column table (key/value)
   - `text` → Formatted key: value pairs

3. **PrintSuccess/Error/Info/Warning()** - Already exist
   - Keep as-is, but ensure they work with all formats
   - For JSON mode: output as `{"status": "success", "message": "..."}`

#### Refactored Functions:
- `PrintTable()` - Already handles table format, add JSON/text support
- `PrintKeyValue()` - Add better JSON and table support
- `PrintObject()` - Fix to actually handle all three formats properly

**Impact**: ~100-150 lines

### Phase 3: Create Output Helpers

**File**: `pkg/formatter/output.go` (new file)

Create helper functions to work with the three formats:

```go
// OutputFormat represents the output format type
type OutputFormat string

const (
    FormatJSON  OutputFormat = "json"
    FormatTable OutputFormat = "table"
    FormatText  OutputFormat = "text"
)

// GetOutputFormat returns the configured output format
func GetOutputFormat() OutputFormat { ... }

// ValidateOutputFormat checks if format is valid
func ValidateOutputFormat(format string) bool { ... }

// PrintList handles lists in all formats
func PrintList(title string, items []interface{}, headers []string) { ... }

// PrintRecord handles single records in all formats
func PrintRecord(title string, data map[string]interface{}) { ... }

// PrintData is a generic function for any data type
func PrintData(data interface{}, format OutputFormat) { ... }
```

**Impact**: ~150 lines

### Phase 4: Update Service Methods (Incremental)

For each service that currently returns data:
- [ ] Remove direct formatter calls from service layer
- [ ] Return raw data structs from services
- [ ] Let command layer handle formatting

Example pattern:
```go
// OLD (service knows about formatting)
func (s *Service) GetUsers() error {
    users, _ := api.GetUsers()
    formatter.PrintTable(headers, rows)
    return nil
}

// NEW (service returns data, command formats)
func (s *Service) GetUsers() ([]User, error) {
    return api.GetUsers()
}

// In command:
func init() {
    cmd := &cobra.Command{
        RunE: func(cmd *cobra.Command, args []string) error {
            svc := service.NewService()
            users, err := svc.GetUsers()
            if err != nil { return err }

            format := formatter.GetOutputFormat()
            switch format {
            case "json":
                fmt.Println(formatter.ToJSON(users))
            case "table":
                formatter.PrintTable(headers, formatUsersAsTable(users))
            default:
                formatter.PrintList("Users", users, headers)
            }
            return nil
        },
    }
}
```

**Impact**: Refactor all ~50 service methods that output data

---

## Implementation Strategy

### Option A: Minimal Refactoring (Recommended for Phase 1)
1. Fix root command to set config value ✅ (2 minutes)
2. Enhance formatter package to detect format automatically ✅ (30 minutes)
3. Keep current service architecture
4. Services can call: `formatter.PrintRecord()`, `formatter.PrintList()`, etc.
5. These functions automatically use the configured format

**Time**: ~30-40 minutes
**Files Changed**: 2 (root.go, formatter.go)
**Breaking Changes**: None
**Benefit**: Immediate, all future commands use correct formatting

### Option B: Full Refactoring (Better Long-term)
1. All of Option A
2. Refactor services to return data instead of printing
3. Move all formatting logic to command layer
4. Create unified output helpers

**Time**: ~4-6 hours
**Files Changed**: 20+ service files + commands + formatter
**Breaking Changes**: All service method signatures change
**Benefit**: Clean separation of concerns, testable services

---

## Recommended Approach

**Start with Option A**:
1. Fix root command (2 min)
2. Enhance formatter.go (30 min)
3. Test with a few commands (10 min)
4. All new Phase 2+ commands use proper formatting automatically
5. Later: Gradually refactor existing services if needed (Option B)

---

## Test Cases

After implementation, verify with:

```bash
# Test text format (default)
sidechain-cli post list
sidechain-cli post list --output text

# Test JSON format
sidechain-cli post list --output json | jq .

# Test table format
sidechain-cli post list --output table

# Test with different commands
sidechain-cli profile followers --output json
sidechain-cli feed timeline --output table
sidechain-cli search users --output text
```

---

## Files Affected

### Phase 1 (Option A - Minimal Refactoring)

1. **internal/cmd/root.go** (modify)
   - Add `config.SetString()` call in PersistentPreRun
   - Add format validation

2. **pkg/formatter/formatter.go** (enhance)
   - Update all Print* functions to respect config format
   - Fix table formatting for json/text modes
   - Create unified PrintRecord(), PrintList() helpers

### Phase 2+ (Option B - Full Refactoring, if needed)

- All service files (~20 files)
- All command files (~25 files)
- formatter package

---

## Benefits

✅ Consistent output across all commands
✅ JSON output for scripting/integration
✅ Table format for readability
✅ Text format for simple display
✅ Backward compatible (default is text)
✅ Future-proof for Phase 2, 3 implementations

---

## Estimated Timeline

- **Option A (Recommended)**:
  - Implementation: 40 minutes
  - Testing: 15 minutes
  - Total: ~1 hour
  - Ready to continue Phase 2 immediately

- **Option B (Full Refactoring)**:
  - Implementation: 5-6 hours
  - Testing: 1 hour
  - Total: ~6-7 hours
  - Better architecture, but delays Phase 2

---

## Decision

**Recommendation**: Implement Option A now
- Get all three output formats working quickly
- All new Phase 2 commands automatically support them
- Can refactor services later (Option B) if needed

**Next Steps**:
1. Implement Option A (1 hour)
2. Test with existing commands (15 min)
3. Continue with Phase 2 features
