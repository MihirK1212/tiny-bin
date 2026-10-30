#include "connection.h"

DBConnection::DBConnection()
    : cx{"postgresql://postgres:postgres@localhost:5432/testdb"} {}

DBConnection::~DBConnection() = default;
