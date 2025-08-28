#include "sync/sync_engine.hpp"
#include "sync/server_mode.hpp"
#include "sync/client_mode.hpp"
#include<iostream>
int main(int argc, char** argv) {

    if(argc<2){
        std::cerr << "Insufficient arguments\n";
        return 1;
    }

    std::string mode=argv[1];
    if(mode=="server"){
        ServerMode server(8080);
        server.startServer();
    }else{
        ClientMode client;
        client.startCLI();
    }

    return 0;
}
