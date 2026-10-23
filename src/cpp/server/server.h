#ifndef SERVER_H
#define SERVER_H

class TinyBinServer {
    public:
        TinyBinServer(int port, int num_workers);
        void start();

    private:
        int port;
        int num_workers;
};

#endif