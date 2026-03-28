#include "TuiClient.h"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    std::string wsUrl = "ws://localhost:1882";

    // Parse args
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.rfind("ws://", 0) == 0 || arg.rfind("wss://", 0) == 0) {
            wsUrl = arg;
        } else if (arg.rfind("--token=", 0) == 0) {
            std::string token = arg.substr(8);
            // Append token to URL
            if (wsUrl.find('?') == std::string::npos) {
                wsUrl += "?token=" + token;
            } else {
                wsUrl += "&token=" + token;
            }
        }
    }

    std::cout << "berkide-tui connecting to " << wsUrl << "...\n";

    TuiClient client(wsUrl);
    if (!client.connect()) {
        std::cerr << "Failed to connect to BerkIDE server at " << wsUrl << "\n";
        return 1;
    }

    client.run();
    client.disconnect();

    std::cout << "berkide-tui disconnected.\n";
    return 0;
}
