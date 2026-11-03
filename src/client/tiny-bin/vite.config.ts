import react from '@vitejs/plugin-react'
import { defineConfig, loadEnv } from 'vite'

export default defineConfig(({ mode }) => {
  const env = { ...loadEnv(mode, process.cwd(), ''), ...process.env }
  const target = env.BACKEND_URL || 'http://127.0.0.1:8000'
  const host = env.FRONTEND_HOST || '127.0.0.1'
  const port = Number(env.FRONTEND_PORT || '5173')
  if (!Number.isInteger(port) || port < 1 || port > 65535) {
    throw new Error('FRONTEND_PORT must be between 1 and 65535')
  }
  const proxy = { '/api': { target, changeOrigin: true, timeout: 15000, proxyTimeout: 15000 } }
  return {
    plugins: [react()],
    server: { host, port, strictPort: true, proxy },
    preview: { host, port, strictPort: true, proxy },
  }
})
