# tiny-bin

# Set env variables

```
set -a && source .env && set +a
```

# Launch C++ server
```
cmake -S . -B build/ && cmake --build build/ && ./build/tiny_bin_server -p 8000 -n 8
```

# Postgrsql setup 

## Install libs 

```
sudo apt install g++ cmake git pkg-config libpqxx-dev libpq-dev postgresql postgresql-client python3
```

## Start postgresql server

```
sudo systemctl enable --now postgresql
```

## Enter postgress shell (to configure password, create db etc)

```
sudo -u postgres psql
```

## Setup users and db

```
sudo -u postgres createuser --pwprompt tinybin
sudo -u postgres createdb --owner=tinybin tinybin
```

## Run SQL scripts

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