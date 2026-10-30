#ifndef DB_QUERY_H
#define DB_QUERY_H

#include <mutex>
#include <pqxx/pqxx>

#include "connection.h"

inline pqxx::result execute_query(DBConnection& connection, const std::string& query) {
    std::lock_guard<std::mutex> lock(connection.query_mutex());
    pqxx::connection& cx = connection.get_connection();
    pqxx::work tx{cx};
    pqxx::result result = tx.exec(query);
    tx.commit();
    return result;
}

#endif