# E2E Testing for Sidechain Web Frontend

This directory contains end-to-end tests for the Sidechain web frontend using **Playwright**. Tests run against a **real backend** with a **real test database**, ensuring full integration validation.

## Architecture

### How It Works

```
┌─────────────────────────────────────────────────────────────┐
│                  Playwright E2E Tests                       │
│              (Browser Automation Framework)                 │
└─────────────────────────────────────────────────────────────┘
                              ↓
        ┌─────────────────────────────────────────┐
        │    Frontend (Next.js Dev Server)         │
        │   http://localhost:5173                 │
        │   - React Components                     │
        │   - State Management (Zustand)          │
        │   - API Client Layer                     │
        └─────────────────────────────────────────┘
                              ↓
        ┌─────────────────────────────────────────┐
        │    Backend API (Real Go Server)          │
        │   http://localhost:8787                 │
        │   - JWT Authentication                  │
        │   - User Management                      │
        │   - Feed APIs                            │
        │   - Social Features                      │
        └─────────────────────────────────────────┘
                              ↓
        ┌─────────────────────────────────────────┐
        │     Test Database (PostgreSQL)           │
        │   postgres://...@localhost:5433         │
        │   - Real User Records                    │
        │   - Real Post Data                       │
        │   - Test Data Seeding                    │
        └─────────────────────────────────────────┘
```

## Prerequisites

### Required Software
- **Node.js** 20+ (for Playwright and web dependencies)
- **Go** 1.21+ (for backend seeding and migrations)
- **Docker** & **Docker Compose** (for database and redis services)
- **Git** (for cloning the repository)

### Verify Installation
```bash
node --version   # v20+
go version       # go version 1.21+
docker --version # Docker version 20+
```

## Quick Start

### 1. Start Test Infrastructure (Docker)

This starts PostgreSQL, Redis, and the Go backend server:

```bash
# From web directory
npm run test:e2e:docker:up
```

This will:
- Start PostgreSQL on port 5433 (test database)
- Start Redis on port 6380
- Start Go backend on port 8787
- Wait for all services to be healthy

**Check status:**
```bash
docker ps
curl http://localhost:8787/health
```

### 2. Initialize Test Database

This runs migrations and seeds test users:

```bash
npm run test:e2e:seed
```

This will:
- Run database migrations
- Create test users (alice, bob, charlie, diana, eve)
- Create sample posts and comments
- Verify all test users can authenticate

**Test User Credentials:**
```
Email: alice@example.com       Password: password123
Email: bob@example.com         Password: password123
Email: charlie@example.com     Password: password123
Email: diana@example.com       Password: password123
Email: eve@example.com         Password: password123
```

### 3. Run E2E Tests

```bash
# Run all E2E tests
npm run test:e2e

# Run in UI mode (interactive)
npm run test:e2e:ui

# Run specific test file
npm run test:e2e -- e2e/tests/auth/login.spec.ts

# Run in debug mode
npm run test:e2e:debug

# View test report
npm run test:e2e:report
```

### 4. Stop Test Infrastructure

```bash
npm run test:e2e:docker:down
```

This will remove all containers, volumes, and databases.

## Full Integration Test Workflow

Run everything end-to-end:

```bash
npm run test:e2e:full
```

This is equivalent to:
```bash
npm run test:e2e:docker:up      # Start containers
npm run test:e2e:seed            # Seed database
npm run test:e2e                 # Run tests
npm run test:e2e:docker:down    # Cleanup
```

## Test Structure

### Directory Layout
```
e2e/
├── README.md                 # This file
├── playwright.config.ts      # Playwright configuration
├── docker-compose.test.yml   # Docker services
├── .env.test                 # Test environment variables
├── fixtures/
│   ├── auth.ts              # Authentication fixture
│   ├── test-users.ts        # Test user credentials
│   └── seed.ts              # Database seeding utilities
├── helpers/
│   ├── websocket-helper.ts  # WebSocket testing
│   └── wait-strategies.ts   # Async wait utilities
├── page-objects/
│   ├── FeedPage.ts          # Feed page interactions
│   ├── ProfilePage.ts       # Profile page interactions
│   └── PostCard.ts          # Post card component
└── tests/
    ├── auth/
    │   └── login.spec.ts    # Authentication tests
    ├── feed/
    │   ├── feed-load.spec.ts
    │   ├── feed-types.spec.ts
    │   ├── feed-interactions.spec.ts
    │   ├── feed-realtime.spec.ts
    │   └── feed-infinite-scroll.spec.ts
    └── profile/
        ├── profile-view.spec.ts
        ├── profile-follow.spec.ts
        ├── profile-edit.spec.ts
        └── profile-posts.spec.ts
```

## Using Test Fixtures

### Authenticated Page

```typescript
import { test } from '../../fixtures/auth'

test('should show user feed', async ({ authenticatedPage }) => {
  // Page is already logged in as alice
  await authenticatedPage.goto('/feed')

  // Interact with the page
  const posts = await authenticatedPage.locator('[data-testid="post-card"]').count()
  expect(posts).toBeGreaterThan(0)
})
```

### Authenticate as Different User

```typescript
import { test } from '../../fixtures/auth'
import { testUsers } from '../../fixtures/test-users'

test('should show bob profile on bob account', async ({ authenticatedPageAs }) => {
  const bobPage = await authenticatedPageAs(testUsers.bob)
  await bobPage.goto('/profile/bob')

  // Verify it's bob's profile
  await expect(bobPage.locator('text=Bob Johnson')).toBeVisible()
})
```

### Access API Token

```typescript
import { test } from '../../fixtures/auth'

test('should make API request with token', async ({ apiToken }) => {
  const response = await fetch('http://localhost:8787/api/v1/users/me', {
    headers: {
      'Authorization': `Bearer ${apiToken}`
    }
  })

  const user = await response.json()
  expect(user.email).toBe('alice@example.com')
})
```

## Writing Tests

### Test Pattern 1: UI Flow Testing

```typescript
import { test, expect } from '@playwright/test'

test('should filter feed by type', async ({ page }) => {
  await page.goto('http://localhost:5173/login')

  // Login
  await page.locator('input[type="email"]').fill('alice@example.com')
  await page.locator('input[type="password"]').fill('password123')
  await page.locator('button[type="submit"]').click()

  // Wait for feed to load
  await page.waitForURL('**/feed')

  // Click trending button
  await page.locator('button:has-text("Trending")').click()

  // Verify posts are loaded
  const posts = await page.locator('[data-testid="post"]').count()
  expect(posts).toBeGreaterThan(0)
})
```

### Test Pattern 2: Using Fixtures

```typescript
import { test, expect } from '../../fixtures/auth'
import { waitForLoadingToComplete } from '../../helpers/wait-strategies'

test('should like a post', async ({ authenticatedPage }) => {
  await authenticatedPage.goto('/feed')

  // Wait for content to load
  await waitForLoadingToComplete(authenticatedPage)

  // Like first post
  const likeButton = authenticatedPage.locator('[data-testid="like-button"]').first()
  await likeButton.click()

  // Verify like count increased
  const likeCount = await likeButton.locator('..').locator('text=/\\d+/').textContent()
  expect(likeCount).toMatch(/[1-9]/)
})
```

### Test Pattern 3: Multiple Users

```typescript
import { test } from '../../fixtures/auth'
import { testUsers } from '../../fixtures/test-users'

test('should follow user', async ({ authenticatedPageAs }) => {
  // Login as alice
  const alicePage = await authenticatedPageAs(testUsers.alice)

  // View bob's profile
  await alicePage.goto('/profile/bob')

  // Click follow
  await alicePage.locator('button:has-text("Follow")').click()

  // Verify button changed to "Following"
  await expect(alicePage.locator('button:has-text("Following")')).toBeVisible()
})
```

## Environment Variables

### `.env.test` Configuration

```env
# Backend Configuration
BACKEND_URL=http://localhost:8787
DATABASE_URL=postgres://sidechain:test_password@localhost:5433/sidechain_test

# Frontend Configuration
PLAYWRIGHT_TEST_BASE_URL=http://localhost:5173

# Stream.io (optional, for social features)
STREAM_API_KEY=your_stream_key
STREAM_API_SECRET=your_stream_secret

# Test Database
POSTGRES_USER=sidechain
POSTGRES_PASSWORD=test_password
POSTGRES_DB=sidechain_test
```

## Troubleshooting

### Backend Not Connecting

```bash
# Check if backend is running
curl http://localhost:8787/health

# Check backend logs
docker logs -f sidechain-backend-test

# Restart services
npm run test:e2e:docker:down
npm run test:e2e:docker:up
```

### Database Connection Issues

```bash
# Check database
docker exec sidechain-postgres-test psql -U sidechain -d sidechain_test -c "SELECT COUNT(*) FROM users"

# Check migrations ran
docker exec sidechain-postgres-test psql -U sidechain -d sidechain_test -c "\dt"
```

### Seeding Failed

```bash
# Verify backend is healthy
npm run test:e2e:docker:up
sleep 10
npm run test:e2e:seed

# Check seed logs
tail -50 docker-compose.test.yml  # After running seed
```

### Tests Timing Out

- Increase timeout in tests: `{ timeout: 20000 }`
- Check if frontend dev server is running on port 5173
- Verify backend and database services are healthy
- Check network connectivity between services

## Debugging Tests

### Debug Mode
```bash
npm run test:e2e:debug
```

This opens Playwright Inspector with:
- Step-through debugging
- DOM inspection
- Network requests
- Console output

### View Test Report
```bash
npm run test:e2e:report
```

This opens interactive HTML report with:
- Test results
- Screenshots on failure
- Video recordings on failure
- Trace files for inspection

### Run Single Test
```bash
npm run test:e2e -- e2e/tests/auth/login.spec.ts
```

### Run With Verbose Output
```bash
npm run test:e2e -- --verbose
```

## Best Practices

### ✅ DO

- Use data-testid attributes for selectors
- Wait for content explicitly (don't sleep)
- Use page objects for complex pages
- Authenticate once per test group
- Clean up test data between runs
- Test user flows, not implementation
- Mock external services (Stream.io, S3, etc.)

### ❌ DON'T

- Use hardcoded sleeps/waits
- Click without waiting for element
- Test against production database
- Skip cleanup after tests
- Test single components (use unit tests)
- Mock the real backend (test real integration)
- Rely on page load order

## CI/CD Integration

### GitHub Actions

See `.github/workflows/e2e.yml` for full workflow.

```yaml
- name: Run E2E tests
  run: |
    npm run test:e2e:docker:up
    npm run test:e2e:seed
    npm run test:e2e
    npm run test:e2e:docker:down
```

### Local CI Simulation

```bash
CI=true npm run test:e2e
```

This will:
- Run tests once (no retries on first run)
- Use 2 workers instead of 4
- Generate junit XML report
- Upload artifacts on failure

## Performance Tips

### Run Tests Faster

```bash
# Run only critical tests
npm run test:e2e -- e2e/tests/auth

# Run in parallel with more workers
npx playwright test --workers=8

# Run without video/screenshots
npx playwright test --no-screenshoot --no-video
```

### Isolate Database Per Worker

Each test worker gets its own database connection:
- Avoid test conflicts
- Enable true parallelization
- Reduces flakiness

## Test Data Management

### Seed Fresh Data

```bash
npm run test:e2e:seed
```

### Clean Seed Data

```bash
# Remove all test data
npm run test:e2e -- --setup-only

# Or manually
cd backend && go run cmd/seed/main.go clean
```

### Verify Test Users

```bash
# Verify users exist and can authenticate
npm run test:e2e -- e2e/tests/auth/login.spec.ts
```

## Resources

- **Playwright Docs**: https://playwright.dev
- **Test Best Practices**: https://playwright.dev/docs/best-practices
- **Debugging Guide**: https://playwright.dev/docs/debug
- **CI/CD Guide**: https://playwright.dev/docs/ci

## Contributing

When adding new tests:

1. Create test file in appropriate directory
2. Use existing fixtures and helpers
3. Follow naming convention: `feature.spec.ts`
4. Add JSDoc comments explaining test purpose
5. Use page objects for complex interactions
6. Run locally before pushing: `npm run test:e2e`
7. Check for flakiness (run multiple times)

## Support

For issues or questions:
1. Check this README
2. Check Playwright documentation
3. Review existing test examples
4. Check backend logs: `docker logs sidechain-backend-test`
5. Open GitHub issue with details
