#ifndef APP_DB_PASTES_H
#define APP_DB_PASTES_H

#include "query.h"

inline const std::string paste_columns =
    "id::text, content, to_char(created_at AT TIME ZONE 'UTC', "
    "'YYYY-MM-DD\"T\"HH24:MI:SS.US\"Z\"') AS created_at";

inline pqxx::result create_paste(DBConnection& db, const std::string& content) {
    return execute_query(db, "INSERT INTO tiny_bin.pastes (content) VALUES ($1) RETURNING " +
                         paste_columns, content);
}

inline pqxx::result get_paste(DBConnection& db, const std::string& id) {
    return execute_query(db, "SELECT " + paste_columns + " FROM tiny_bin.pastes WHERE id = $1::uuid", id);
}

#endif
