import { exec } from 'child_process'
import { promisify } from 'util'
import { testUsers } from './test-users'

const execAsync = promisify(exec)

/**
 * Database seeding utilities for E2E tests
 * These functions handle initializing the test database with real users
 * and data via the backend's seeding mechanism
 */

const BACKEND_DIR = '../backend' // Relative to web directory
const BACKEND_URL = process.env.BACKEND_URL || 'http://localhost:8787'
const DATABASE_URL =
  process.env.DATABASE_URL ||
  'postgres://sidechain:test_password@localhost:5433/sidechain_test'

/**
 * Wait for backend to be healthy and ready for seeding
 */
export async function waitForBackendReady(
  timeoutMs: number = 60000
): Promise<void> {
  const startTime = Date.now()

  while (Date.now() - startTime < timeoutMs) {
    try {
      const response = await fetch(`${BACKEND_URL}/health`, {
        method: 'GET',
      })

      if (response.ok) {
        console.log('✓ Backend is healthy and ready')
        return
      }
    } catch (error) {
      // Backend not ready yet, continue waiting
    }

    // Wait 1 second before retrying
    await new Promise((resolve) => setTimeout(resolve, 1000))
  }

  throw new Error(
    `Backend did not become ready within ${timeoutMs}ms. Is it running on ${BACKEND_URL}?`
  )
}

/**
 * Run database migrations
 */
export async function runMigrations(): Promise<void> {
  console.log('Running database migrations...')

  try {
    const { stdout, stderr } = await execAsync(
      `cd ${BACKEND_DIR} && go run cmd/migrate/main.go up`,
      {
        env: {
          ...process.env,
          DATABASE_URL,
        },
        timeout: 30000,
      }
    )

    if (stdout) console.log(stdout)
    if (stderr) console.log('Migration stderr:', stderr)

    console.log('✓ Migrations completed')
  } catch (error) {
    throw new Error(
      `Failed to run migrations: ${error instanceof Error ? error.message : String(error)}`
    )
  }
}

/**
 * Seed test database with test users and data
 * Uses backend's seeding mechanism which creates real users in the database
 */
export async function seedTestDatabase(): Promise<void> {
  console.log('Seeding test database with test users...')

  try {
    const { stdout, stderr } = await execAsync(
      `cd ${BACKEND_DIR} && go run cmd/seed/main.go test`,
      {
        env: {
          ...process.env,
          DATABASE_URL,
        },
        timeout: 30000,
      }
    )

    if (stdout) console.log(stdout)
    if (stderr) console.log('Seed stderr:', stderr)

    console.log('✓ Database seeded with test users')
    logTestCredentials()
  } catch (error) {
    throw new Error(
      `Failed to seed database: ${error instanceof Error ? error.message : String(error)}`
    )
  }
}

/**
 * Clean up seed data from database
 */
export async function cleanSeedData(): Promise<void> {
  console.log('Cleaning up seed data...')

  try {
    const { stdout, stderr } = await execAsync(
      `cd ${BACKEND_DIR} && go run cmd/seed/main.go clean`,
      {
        env: {
          ...process.env,
          DATABASE_URL,
        },
        timeout: 30000,
      }
    )

    if (stdout) console.log(stdout)
    if (stderr) console.log('Clean stderr:', stderr)

    console.log('✓ Seed data cleaned up')
  } catch (error) {
    throw new Error(
      `Failed to clean seed data: ${error instanceof Error ? error.message : String(error)}`
    )
  }
}

/**
 * Initialize test database from scratch
 * 1. Run migrations to create schema
 * 2. Seed test data with users
 */
export async function initializeTestDatabase(): Promise<void> {
  console.log('\n=== Initializing Test Database ===')
  console.log(`Backend URL: ${BACKEND_URL}`)
  console.log(`Database: ${DATABASE_URL.split('/').pop()}`)

  await waitForBackendReady()
  await runMigrations()
  await seedTestDatabase()

  console.log('=== Database Ready for Testing ===\n')
}

/**
 * Verify test users exist in database by trying to authenticate
 */
export async function verifyTestUsers(): Promise<void> {
  console.log('Verifying test users exist in database...')

  for (const [name, user] of Object.entries(testUsers)) {
    try {
      const response = await fetch(`${BACKEND_URL}/api/v1/auth/login`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          email: user.email,
          password: 'password123',
        }),
      })

      if (response.ok) {
        const data = await response.json()
        if (data.auth?.token) {
          console.log(`  ✓ ${name} (${user.email})`)
        } else {
          throw new Error('No token in response')
        }
      } else {
        const errorText = await response.text()
        throw new Error(`${response.status}: ${errorText}`)
      }
    } catch (error) {
      throw new Error(
        `Failed to verify user ${name} (${user.email}): ${error instanceof Error ? error.message : String(error)}`
      )
    }
  }

  console.log('✓ All test users verified and can authenticate')
}

/**
 * Log test user credentials for reference
 */
function logTestCredentials(): void {
  console.log('\nTest User Credentials:')
  console.log('────────────────────────────────────────')

  for (const [name, user] of Object.entries(testUsers)) {
    console.log(`${name}:`)
    console.log(`  Email: ${user.email}`)
    console.log(`  Username: ${user.username}`)
    console.log(`  Password: password123`)
    console.log(`  Display Name: ${user.displayName}`)
  }

  console.log('────────────────────────────────────────\n')
}

/**
 * Main entry point for CLI usage
 * Usage: node -r esbuild-register e2e/fixtures/seed.ts [command]
 * Commands: init, verify, clean, migrate
 */
if (require.main === module) {
  const command = process.argv[2] || 'init'

  ;(async () => {
    try {
      switch (command) {
        case 'init':
          await initializeTestDatabase()
          await verifyTestUsers()
          break
        case 'verify':
          await verifyTestUsers()
          break
        case 'clean':
          await cleanSeedData()
          break
        case 'migrate':
          await runMigrations()
          break
        default:
          console.log('Unknown command:', command)
          console.log('Available commands: init, verify, clean, migrate')
          process.exit(1)
      }
    } catch (error) {
      console.error('Error:', error)
      process.exit(1)
    }
  })()
}
