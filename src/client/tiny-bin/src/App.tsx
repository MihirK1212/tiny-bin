import { useEffect, useState } from 'react'
import type { SubmitEvent } from 'react'
import { ApiError, createPaste, getPaste } from './api'
import type { Paste } from './api'
import './App.css'

const encoder = new TextEncoder()

function errorMessage(error: unknown) {
  return error instanceof Error ? error.message : 'Something went wrong. Please try again.'
}

function PasteForm({ onSave }: { onSave: (content: string) => Promise<void> }) {
  const [content, setContent] = useState('')
  const [busy, setBusy] = useState(false)
  const [error, setError] = useState('')

  const isEmptyContent = encoder.encode(content).length === 0 || content.trim() === ''

  async function submit(event: SubmitEvent<HTMLFormElement>) {
    event.preventDefault()
    if (busy || isEmptyContent) return
    setBusy(true)
    setError('')
    try {
      await onSave(content)
    } catch (failure) {
      setError(errorMessage(failure))
    } finally {
      setBusy(false)
    }
  }

  return (
    <form onSubmit={submit}>
      <fieldset disabled={busy}>
        <div className="field">
          <label htmlFor="content">Paste content</label>
          <textarea id="content" value={content} onChange={(event) => setContent(event.target.value)}
            rows={14} required spellCheck={false} aria-describedby="content-help" />
        </div>
        {error && <p className="error" role="alert">{error}</p>}
        <div className="actions">
          <button type="submit" disabled={isEmptyContent}>
            {busy ? 'Saving…' : 'Create paste'}
          </button>
        </div>
      </fieldset>
    </form>
  )
}

function PastePage({ id }: { id: string }) {
  const [paste, setPaste] = useState<Paste | null>(null)
  const [loadError, setLoadError] = useState('')
  const [notice, setNotice] = useState('')

  useEffect(() => {
    const controller = new AbortController()
    getPaste(id, controller.signal).then(setPaste).catch((failure: unknown) => {
      if (!controller.signal.aborted) {
        setLoadError(failure instanceof ApiError && failure.status === 404
          ? 'This paste does not exist.' : errorMessage(failure))
      }
    })
    return () => controller.abort()
  }, [id])

  async function copyLink() {
    if (!paste) return
    try {
      await navigator.clipboard.writeText(paste.url)
      setNotice('Link copied.')
    } catch {
      setNotice('Select the link above to copy it manually.')
    }
  }

  if (loadError) return <p className="error" role="alert">{loadError}</p>
  if (!paste) return <p role="status">Loading paste…</p>

  return (
    <section aria-labelledby="paste-heading">
      <h1 id="paste-heading">Paste</h1>
      <p className="muted">Created {new Date(paste.created_at).toLocaleString()}</p>
      <div className="field share">
        <label htmlFor="share-link">Share link</label>
        <div className="share-row">
          <input id="share-link" readOnly value={paste.url} onFocus={(event) => event.target.select()} />
          <button type="button" className="secondary" onClick={copyLink}>Copy link</button>
        </div>
      </div>
      {notice && <p role="status">{notice}</p>}
      <pre className="paste-content">{paste.content}</pre>
    </section>
  )
}

function App() {
  const [path, setPath] = useState(window.location.pathname)
  useEffect(() => {
    function onPopState() {
      setPath(window.location.pathname)
    }
    window.addEventListener('popstate', onPopState)
    return () => window.removeEventListener('popstate', onPopState)
  }, [])

  function navigate(nextPath: string) {
    window.history.pushState(null, '', nextPath)
    setPath(nextPath)
    window.scrollTo(0, 0)
  }
  const pasteRoute = /^\/p\/([0-9a-fA-F-]{36})\/?$/.exec(path)

  return (
    <div className="app">
      <header>
        <a className="brand" href="/">TinyBin</a>
        {path !== '/' && <a href="/">New paste</a>}
      </header>
      <main>
        {path === '/' ? (
          <section aria-labelledby="new-paste-heading">
            <h1 id="new-paste-heading">New paste</h1>
            <p className="intro">Share plain text with a link. Anyone with the link can read it.</p>
            <PasteForm onSave={async (content) => {
              const paste = await createPaste(content)
              navigate(`/p/${paste.id}`)
            }} />
          </section>
        ) : pasteRoute ? (
          <PastePage key={path} id={pasteRoute[1]} />
        ) : <><h1>Page not found</h1><p><a href="/">Create a new paste</a></p></>}
      </main>
    </div>
  )
}

export default App
