# tiny-bin

# Deployment setup without docker (local development)

## Set env variables for C++ server

- the C++ server requires the environment variables to be set (see `.env.example.server`)

```
set -a && source src/cpp/server/.env.dev.local && set +a
```

## Launch C++ server

```
cmake -S . -B build/ && cmake --build build/ && ./build/tiny_bin_server
```

## Create env file for client

- client requires some env vars to be set so that the vite runtime process can use it
- `npm run dev` in `src/client/tiny-bin` loads `.env.dev.local` (can be created using `.env.example.client`)

## Postgrsql setup

### Install libs

```
sudo apt install g++ cmake git pkg-config libpqxx-dev libpq-dev postgresql postgresql-client python3
```

### Start postgresql server

```
sudo systemctl enable --now postgresql
```

### Enter postgress shell (to configure password, create db etc)

```
sudo -u postgres psql
```

### Setup users and db

```
sudo -u postgres createuser --pwprompt tinybin
sudo -u postgres createdb --owner=tinybin tinybin
```

### Run SQL scripts

From the repository root:

```
psql -h localhost -p 5432 \
     -U tinybin \
     -d tinybin \
     -W \
     -v ON_ERROR_STOP=1 \
     -f src/sql/up.sql

psql -h localhost -p 5432 \
     -U tinybin \
     -d tinybin \
     -W \
     -v ON_ERROR_STOP=1 \
     -f src/sql/down.sql
```

# Docker containers architecture and flow

- one container for the gateway nginx proxy
- one container for the C++ server
- one container for the postgres DB
- one container for the statically served client (using nginx)

## Gateway nginx proxy

- this is the gateway for all requests (eg:- from browser)
- it forwards /api/ requests to the C++ server container (inside the docker network)

```
location /api/ {
        set $api_upstream tiny-bin-server:8000;
        proxy_pass http://$api_upstream$request_uri;
        ...
```

- it forwards all other requests to the static client nginx (inside the docker network)

```
location / {
        set $client_upstream tiny-bin-client:80;
        proxy_pass http://$client_upstream$request_uri;
        ...
```

## C++ server

- the C++ server container works pretty much the same as it does in development
- the C++ server container port 8000 is exposed only inside the docker network and not exposed outside

## Static client nginx

- the client is served statically using nginx. 
- after the build, the dist/ is placed at the nginx's document root 
- so this client nginx will serve statically built assets
- the client container port 80 is exposed only inside the docker network and not exposed outside

### SPA fallback

in the client nginx, we have:

```
location / {
    try_files $uri $uri/ /index.html;
}
```

- lets say we go to url /p/
- nginx first tries the URL as a file. if nothing matches, we serve index.html
- the react app loads and then handles the /p/, parses  and fetched the data from the backed

# Deployment with docker but without docker compose (local development and prod)

## Network (when not using docker compose)

- We need to create a network so that the individual containers can talk to each other within it

```
docker network create tiny-bin-network
```

## Postgres Conainter

- We bind mount the  src/sql/up.sql script into the initdb entry point so that the schemas get set up when the container starts running

```
docker run --name postgres-service \
  --network tiny-bin-network \
  -e POSTGRES_USER=tinybin \
  -e POSTGRES_PASSWORD=tinybin_password \
  -e POSTGRES_DB=tinybin \
  -p 5433:5432 \
  -v "$PWD/src/sql/up.sql":/docker-entrypoint-initdb.d/01-up.sql:ro \
  -d postgres:17
```

## C++ container

- Build the C++ server container

```
docker build -f ./src/cpp/server/Dockerfile -t tiny-bin-server .
```

- Start the C++ server container
- for C++ server the env vars still need to be set in `.env.dev.docker` (see `.env.example.server`)
- `HOST=0.0.0.0` in `.env.dev.docker` so the proxy on the same network can connect
- do not publish port 8000 as the proxy acts as the public entry point

```
docker run --name tiny-bin-server \
    --env-file ./src/cpp/server/.env.dev.docker \
    --network tiny-bin-network \
    -d \
    tiny-bin-server
```

## Client

- No env vars file required
- Docker serves the static client image and does not use any env vars file file.

```
docker build -f ./src/client/tiny-bin/Dockerfile -t tiny-bin-client .
```

```
docker run --name tiny-bin-client \
  --network tiny-bin-network \
  -d \
  tiny-bin-client
```

## Proxy

- Build and start the proxy (only published port)
- `/api` goes to `tiny-bin-server:8000`, everything else to `tiny-bin-client:80`
- container names must stay `tiny-bin-server` and `tiny-bin-client`

```
docker build -f ./src/proxy/Dockerfile -t tiny-bin-proxy .
```

```
docker run --name tiny-bin-proxy \
  --network tiny-bin-network \
  -p 5173:80 \
  -d \
  tiny-bin-proxy
```


# Deployment with docker compose

- `compose.yaml` has everthing required
 
- only prerequisite is that a `.env` file must be created at the root
- it should have all the env vars required by the C++ server (see `env.example.server`)

eg:-

```.env
HOST=0.0.0.0
PORT=8000
NUM_WORKERS=1
DB_USER=tinybin
DB_PASSWORD=tinybin_password
DB_HOST=postgres-service
DB_PORT=5432
DB_NAME=tinybin
```

- the postgres container borrows the env vars from this and maps it to the env vars it requires
- eg:- POSTGRES_USER=${DB_USER}

```
docker compose up -d --build
```

```
docker compose down
```
