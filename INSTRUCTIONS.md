# TinyBin

A minimal React UI backed by the custom C++ HTTP server and PostgreSQL. Create a plain-text paste and share its URL. Reading is public. There is no paste listing, editing, deletion, or user-account system.

## Requirements

- Linux, C++17 compiler, CMake 3.21+, Git, pkg-config
- PostgreSQL 13+ and its client tools
- libpqxx development libraries
- Node.js 22.12+ (Node 24 recommended) and npm
- Python 3 and PostgreSQL server tools for integration tests

On Ubuntu, install the backend dependencies with:

```sh
sudo apt install g++ cmake git pkg-config libpqxx-dev libpq-dev postgresql postgresql-client python3
```

CMake downloads pinned versions of cxxopts and nlohmann/json on the first configure. React dependencies are locked in `src/client/tiny-bin/package-lock.json`.

## Local setup

Create a PostgreSQL role and database if you do not already have one:

```sh
sudo -u postgres createuser --pwprompt tinybin
sudo -u postgres createdb --owner=tinybin tinybin
```

From the repository root, copy `.env.example` to `.env` **if you do not already have one**, then edit it. Set `DATABASE_URL` to your database connection and set `PUBLIC_BASE_URL` to the browser-facing URL. Percent-encode special characters in a connection URI password, or use PostgreSQL keyword/value connection syntax.

Load your local settings into the shell, create the schema, and start the backend:

```sh
set -a
source .env
set +a
psql "$DATABASE_URL" -X -v ON_ERROR_STOP=1 -f src/sql/up.sql
cmake -S . -B build
cmake --build build -j2
./build/tiny_bin_server
```

`src/sql/up.sql` creates the full schema. Run `psql "$DATABASE_URL" -X -v ON_ERROR_STOP=1 -f src/sql/down.sql` to drop everything if you need a clean slate.

In another terminal:

```sh
cd src/client/tiny-bin
npm ci
# Optional: copy .env.example to .env and adjust the proxy target/host/port.
npm run dev
```

Open `http://localhost:5173`. Shared URLs open directly at `/p/<id>`.

## Configuration

The C++ executable reads environment variables. It does not automatically load `.env`; the shell commands above do that for local development. A future container runtime can inject the same variables directly.

| Variable | Default | Purpose |
|---|---|---|
| `HOST` | `127.0.0.1` | IPv4 bind address; use `0.0.0.0` when needed for a container listener |
| `PORT` | `8000` | Backend port; `-p` overrides it |
| `NUM_WORKERS` | `1` | Existing network worker count; `-n` overrides it |
| `DATABASE_URL` | Required | PostgreSQL URI or connection string; include `connect_timeout=5` |
| `PUBLIC_BASE_URL` | `http://localhost:5173` | Browser-facing HTTP(S) origin used for generated share links |

Vite reads `src/client/tiny-bin/.env`; exported environment variables take precedence:

| Variable | Default | Purpose |
|---|---|---|
| `BACKEND_URL` | `http://127.0.0.1:8000` | Backend address used by the Vite proxy |
| `FRONTEND_HOST` | `127.0.0.1` | Frontend bind address |
| `FRONTEND_PORT` | `5173` | Frontend port; keep `PUBLIC_BASE_URL` consistent |

The browser uses relative `/api` URLs. Vite development and local preview proxy those requests to the C++ server. Proxy settings are runtime settings for Vite and are not embedded in the frontend bundle. Never put API keys or database credentials in `VITE_*` variables.

For the next containerization step, a static frontend server will need `/api` proxying and SPA fallback to `index.html` for `/p/*`. Vite preview is only a local build check. This change does not add containers, the Go gateway, or rate limiting.

## API

| Method | Route | Result |
|---|---|---|
| POST | `/api/v1/pastes` | `201` paste and share URL |
| GET | `/api/v1/pastes?id={id}` | `200` paste |
| GET | `/healthz` | `200` while the server responds |
| GET | `/readyz` | `200` if the DB and paste table are available, otherwise `503` |

POST accepts `Content-Type: application/json` and `{"content":"your text"}`. Content must be valid UTF-8, contain 1–10,240 bytes, and contain no null characters. Whitespace is preserved. The existing HTTP parser additionally caps the encoded request body at 16 KiB; unusually heavily escaped content may reach that cap first, which the UI checks before sending.

Paste responses contain `id`, `content`, `created_at` (UTC), and `url`. IDs are server-generated UUIDv4 values, passed as the `id` query parameter on GET. Application errors use `{"error":{"code":"...","message":"..."}}`. Unknown routes return `404`; PUT and DELETE return `405`; a missing or invalid paste id returns `400`; a missing paste returns `404`; DB failures return `503`. Existing low-level malformed/oversized HTTP framing still follows the transport's close-on-error behavior.

The old `/ping` request-reflection and `/users` demo endpoints have been replaced by the paste application. The existing network/event-loop structure remains, including synchronous mutex-serialized DB access. Queries have a five-second statement timeout. The separate HTTP hardening and asynchronous DB work remain follow-up tasks before public deployment.

## Verification

```sh
cmake -S . -B build
cmake --build build -j2
ctest --test-dir build --output-on-failure
cd src/client/tiny-bin
npm run build
npm run lint
```

CTest creates a temporary PostgreSQL cluster and backend, exercises create and get, validation, persistence across restart, routing, DB failures, and fragmented/pipelined HTTP, then cleans up. It does not connect to `DATABASE_URL` from your environment. Run as a non-root user with permission to open local sockets.

To include real-browser checks (once Playwright Chromium is installed):

```sh
cd src/client/tiny-bin
npx playwright install chromium
cd ../../..
TINYBIN_BROWSER_TESTS=1 python3 tests/integration.py build/tiny_bin_server
```

The browser tests start an isolated Vite instance and check create and read, direct links and reloads, text rendering, validation, failure states, and mobile layout.
