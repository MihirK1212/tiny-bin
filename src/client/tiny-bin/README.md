# TinyBin client

Minimal React + TypeScript UI for creating and reading plain-text pastes.

See the [root README](../../../README.md) for PostgreSQL, backend configuration, and end-to-end setup.

```sh
npm ci
npm run dev
```

Copy `.env.example` to `.env` to change the backend proxy target or frontend host/port. The browser always uses relative `/api` requests.

```sh
npm run build
npm run lint
npm run preview
```

Both development and local preview proxy `/api` to `BACKEND_URL`. A future production static server must provide API proxying and SPA fallback for `/p/*`.
