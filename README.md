# tiny-bin

# Launch C++ server
```
cmake -S . -B build/ && cmake --build build/ && ./build/tiny_bin_server -p 8000 -n 8
```

# Postgrsql setup 

## Install libs 

```
sudo apt install -y \
    postgresql \
    postgresql-client \
    libpqxx-dev \
    libpq-dev \
    g++ \
    pkg-config
```

## Start postgresql server

```
sudo systemctl enable --now postgresql
```

## Enter postgress shell (to configure password, create db etc)

```
sudo -u postgres psql
```

## Run SQL scripts  

```
psql -h localhost -p 5432 \
     -U postgres \
     -d testdb \
     -W \
     -v ON_ERROR_STOP=1 \
     -f src/sql/down.sql
```