#ifndef DB_QUERY_H
#define DB_QUERY_H

#include <mutex>
#include <pqxx/pqxx>

#include "connection.h"

template<typename... Params>
inline pqxx::result execute_query(DBConnection& connection, const std::string& query, const Params&... params) {
    std::lock_guard<std::mutex> lock(connection.query_mutex());
    pqxx::connection& cx = connection.get_connection();
    pqxx::work tx{cx};
    tx.exec("SET LOCAL statement_timeout = '5s'");
    pqxx::result result = tx.exec_params(query, params...);
    tx.commit();
    return result;
}

#endif
