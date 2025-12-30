/**
 * Test user credentials for E2E tests
 * These users are seeded by the backend seeder with password 'password123'
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
    username: 'drMarks',
    email: 'kathryneolson@collier.info',
    password: TEST_PASSWORD,
    displayName: 'Larry Kozey',
  } as TestUser,

  bob: {
    username: 'DarkOrchid_rhythm',
    email: 'earnestschroeder@macejkovic.com',
    password: TEST_PASSWORD,
    displayName: 'Khalid Sauer',
  } as TestUser,

  charlie: {
    username: 'BlackcurrantSmoggy',
    email: 'kayleeglover@dooley.org',
    password: TEST_PASSWORD,
    displayName: 'Eryn Feil',
  } as TestUser,

  diana: {
    username: 'rhinoceros.61',
    email: 'ellenjenkins@schmitt.net',
    password: TEST_PASSWORD,
    displayName: 'Magdalena Lowe',
  } as TestUser,

  eve: {
    username: 'StrawberryOdd95',
    email: 'rileyzieme@schoen.name',
    password: TEST_PASSWORD,
    displayName: 'Cecilia Harber',
  } as TestUser,
}

export const defaultTestUser = testUsers.alice
