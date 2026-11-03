export interface Paste {
  id: string
  content: string
  created_at: string
  url: string
}

export class ApiError extends Error {
  status: number

  constructor(message: string, status: number) {
    super(message)
    this.status = status
  }
}

async function request<T>(path: string, options: RequestInit = {}): Promise<T> {
  let response: Response
  try {
    response = await fetch(`/api/v1/pastes${path}`, {
      ...options,
      signal: options.signal
        ? AbortSignal.any([options.signal, AbortSignal.timeout(10000)])
        : AbortSignal.timeout(10000),
    })
  } catch (error) {
    if (options.signal?.aborted) throw error
    throw new Error('Could not reach TinyBin. Please try again.', { cause: error })
  }
  if (!response.ok) {
    const body = await response.json().catch(() => null)
    throw new ApiError(body?.error?.message ?? 'Request failed. Please try again.', response.status)
  }
  return response.json() as Promise<T>
}

const pasteQuery = (id: string) => `?id=${encodeURIComponent(id)}`

export const getPaste = (id: string, signal: AbortSignal) =>
  request<Paste>(pasteQuery(id), { signal })
export const createPaste = (content: string) =>
  request<Paste>('', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ content }),
  })
