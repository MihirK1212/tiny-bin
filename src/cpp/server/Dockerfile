FROM ubuntu:latest AS build

RUN apt-get update && apt-get install -y build-essential g++ cmake git pkg-config libpqxx-dev libpq-dev

WORKDIR /app 

COPY src/cpp ./src/cpp

COPY CMakeLists.txt .

RUN cmake -S . -B ./build/ 

RUN cmake --build ./build/ 


FROM ubuntu:latest

RUN apt-get update && apt-get install -y --no-install-recommends \
        libpqxx-dev libpq5 

COPY --from=build /app/build/tiny_bin_server /tiny_bin_server

EXPOSE 8000

CMD ["/tiny_bin_server"]