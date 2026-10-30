#ifndef DB_CONNECTION_H
#define DB_CONNECTION_H

#include <mutex>
#include <pqxx/pqxx>

class DBConnection {
    public:
        DBConnection();
        ~DBConnection();

        pqxx::connection& get_connection() {
            return cx;
        }

        std::mutex& query_mutex() {
            return query_mutex_;
        }

    private:
        pqxx::connection cx;
        std::mutex query_mutex_;
};

#endif
