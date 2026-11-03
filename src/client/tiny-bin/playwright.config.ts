import { defineConfig } from '@playwright/test'

if (!process.env.TEST_BASE_URL) {
  throw new Error('Run browser tests through tests/integration.py with TINYBIN_BROWSER_TESTS=1')
}

export default defineConfig({
  testDir: './tests',
  workers: 1,
  use: { baseURL: process.env.TEST_BASE_URL, headless: true },
  reporter: 'list',
})
