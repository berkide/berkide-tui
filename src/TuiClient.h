#pragma once
#include <ixwebsocket/IXWebSocket.h>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include "nlohmann/json.hpp"
#include "BerkTerm.h"
#include "input.h"

using json = nlohmann::json;

// Local state cache, updated from WS events
// WS olaylarindan guncellenen yerel durum onbellegi
struct TuiState {
    std::vector<std::string> lines;
    int cursorLine = 0;
    int cursorCol = 0;
    int lineCount = 0;
    std::string filePath;
    bool modified = false;
    std::string mode = "normal";
    int activeIndex = 0;
    std::vector<json> buffers;
};

// WebSocket-based TUI client that connects to a BerkIDE server
// BerkIDE sunucusuna baglanan WebSocket tabanli TUI istemcisi
class TuiClient {
public:
    TuiClient(const std::string& wsUrl);
    ~TuiClient();

    // Connect to the BerkIDE server via WebSocket
    // WebSocket uzerinden BerkIDE sunucusuna baglan
    bool connect();

    // Disconnect from the server
    // Sunucudan baglantiyi kes
    void disconnect();

    // Main loop: render + read input + send to server
    // Ana dongu: render + girdi oku + sunucuya gonder
    void run();

    // Send key event to server
    // Sunucuya tus olayi gonder
    void sendKey(const std::string& key, bool ctrl = false, bool alt = false);

    // Send character input to server
    // Sunucuya karakter girdisi gonder
    void sendChar(const std::string& text);

    // Send a named command with optional arguments
    // Istege bagli argumanlarla adlandirilmis komut gonder
    void sendCommand(const std::string& cmd, const json& args = json::object());

    // Request a full state sync from the server
    // Sunucudan tam durum senkronizasyonu iste
    void requestSync();

private:
    // Handle incoming WebSocket messages
    // Gelen WebSocket mesajlarini isle
    void onMessage(const std::string& msg);

    // Apply a full state sync from server data
    // Sunucu verisinden tam durum senkronizasyonu uygula
    void applyFullSync(const json& data);
    void applyBufferChanged(const json& data);
    void applyCursorMoved(const json& data);

    // Render the current state to the terminal
    // Mevcut durumu terminale render et
    void render(BerkTerm& term);

    // Handle a key-down event from InputHandler
    // InputHandler'dan gelen tus-basma olayini isle
    void handleKeyDown(const InputHandler::KeyEvent& ev);

    // Handle a character input event from InputHandler
    // InputHandler'dan gelen karakter girdisi olayini isle
    void handleCharInput(const InputHandler::KeyEvent& ev);

    std::string wsUrl_;
    ix::WebSocket ws_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> running_{false};

    mutable std::mutex stateMutex_;
    TuiState state_;

    int topRow_ = 0;  // Scroll offset / Kayma ofseti
};
