#include "connection.h"

DBConnection::DBConnection(const std::string& database_url)
    : cx{database_url} {}

DBConnection::~DBConnection() = default;
