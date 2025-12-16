/**
 * Test user credentials for E2E tests
 * These users should be seeded in the test database before tests run
 */

export interface TestUser {
  username: string
  email: string
  password: string
  displayName: string
}

export const TEST_PASSWORD = 'password123'

export const testUsers = {
  alice: {
    username: 'alice',
    email: 'alice@example.com',
    password: TEST_PASSWORD,
    displayName: 'Alice Smith',
  } as TestUser,

  bob: {
    username: 'bob',
    email: 'bob@example.com',
    password: TEST_PASSWORD,
    displayName: 'Bob Johnson',
  } as TestUser,

  charlie: {
    username: 'charlie',
    email: 'charlie@example.com',
    password: TEST_PASSWORD,
    displayName: 'Charlie Brown',
  } as TestUser,

  diana: {
    username: 'diana',
    email: 'diana@example.com',
    password: TEST_PASSWORD,
    displayName: 'Diana Prince',
  } as TestUser,

  eve: {
    username: 'eve',
    email: 'eve@example.com',
    password: TEST_PASSWORD,
    displayName: 'Eve Wilson',
  } as TestUser,
}

export const defaultTestUser = testUsers.alice
