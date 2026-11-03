import { test, expect } from '@playwright/test'

test('create a paste and read it from a direct link', async ({ page, browser }) => {
  const content = '<script>window.pasteExecuted = true</script>\nHello π 🙂'
  await page.goto('/')
  await page.getByLabel('Paste content').fill(content)
  await page.getByRole('button', { name: 'Create paste' }).click()
  await expect(page).toHaveURL(/\/p\/[a-f0-9-]{36}$/)
  await expect(page.locator('.paste-content')).toHaveText(content)
  expect(await page.evaluate(() => Object.hasOwn(window, 'pasteExecuted'))).toBe(false)
  await expect(page.getByLabel('Share link')).toHaveValue(/http:\/\/localhost:5173\/p\/[a-f0-9-]{36}/)
  await expect(page.getByRole('button', { name: 'Edit paste' })).toHaveCount(0)
  await expect(page.getByRole('button', { name: 'Delete paste' })).toHaveCount(0)
  const pastePath = new URL(page.url()).pathname

  const readerContext = await browser.newContext()
  const reader = await readerContext.newPage()
  await reader.goto(new URL(pastePath, page.url()).href)
  await expect(reader.locator('.paste-content')).toHaveText(content)
  await reader.reload()
  await expect(reader.locator('.paste-content')).toHaveText(content)
  await readerContext.close()

  await page.reload()
  await expect(page.locator('.paste-content')).toHaveText(content)
  await page.goto('/p/00000000-0000-4000-8000-000000000000')
  await expect(page.getByRole('alert')).toContainText('does not exist')
})

test('basic validation, missing routes, and narrow screens', async ({ page }) => {
  await page.setViewportSize({ width: 375, height: 812 })
  await page.goto('/')
  await expect(page.getByRole('button', { name: 'Create paste' })).toBeDisabled()
  await page.getByLabel('Paste content').fill('🙂'.repeat(2561))
  await expect(page.locator('#content-help')).toContainText('at most 10,240')
  await expect(page.getByRole('button', { name: 'Create paste' })).toBeDisabled()
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= window.innerWidth)).toBe(true)
  await page.getByLabel('Paste content').fill('A small note, shared simply.')
  await page.screenshot({ path: 'test-results/new-paste-mobile.png', fullPage: true })
  await page.setViewportSize({ width: 1280, height: 900 })
  await page.screenshot({ path: 'test-results/new-paste-desktop.png', fullPage: true })
  await page.goto('/missing')
  await expect(page.getByRole('heading', { name: 'Page not found' })).toBeVisible()
})

test('API failures preserve the draft', async ({ page }) => {
  await page.goto('/')
  await page.getByLabel('Paste content').fill('Keep this draft')
  await page.route('**/api/v1/pastes', (route) => route.fulfill({
    status: 503, contentType: 'application/json',
    body: JSON.stringify({ error: { code: 'DATABASE_UNAVAILABLE', message: 'Database unavailable. Please try again.' } }),
  }))
  await page.getByRole('button', { name: 'Create paste' }).click()
  await expect(page.getByRole('alert')).toContainText('Database unavailable')
  await expect(page.getByLabel('Paste content')).toHaveValue('Keep this draft')
  await expect(page.getByRole('button', { name: 'Create paste' })).toBeEnabled()
})
