#ifndef TINY_BIN_APP_H
#define TINY_BIN_APP_H

#include "db/connection.h"
#include "router.h"

class App {
    public:
        virtual ~App() = default;
        virtual const Router& get_router() const = 0;
};

class TinyBinApp : public App {
    public:
        TinyBinApp()
            : db_connection(),
              router(build_router(db_connection)) {}

        const Router& get_router() const override {
            return router;
        }

    private:
        DBConnection db_connection;
        Router router;
};

#endif
