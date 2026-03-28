#include "TuiClient.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>

// ───────── Utilities ─────────

// Format a line with caret indicator and line number prefix
// Imlec gostergesi ve satir numarasi oneki ile satir bicimlendir
static std::string lineWithCaretPrefix(int idx, bool caret, const std::string& text) {
    std::ostringstream os;
    os << (caret ? ">" : " ") << std::setw(4) << (idx + 1) << " " << text;
    return os.str();
}

TuiClient::TuiClient(const std::string& wsUrl) : wsUrl_(wsUrl) {}

TuiClient::~TuiClient() {
    disconnect();
}

bool TuiClient::connect() {
    ws_.setUrl(wsUrl_);

    ws_.setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            onMessage(msg->str);
        } else if (msg->type == ix::WebSocketMessageType::Open) {
            connected_ = true;
            requestSync();
        } else if (msg->type == ix::WebSocketMessageType::Close) {
            connected_ = false;
        } else if (msg->type == ix::WebSocketMessageType::Error) {
            // Connection error handled silently; run() loop will exit
            // Baglanti hatasi sessizce islenir; run() dongusu cikacak
        }
    });

    ws_.start();

    // Wait for connection (up to 5 seconds)
    // Baglanti icin bekle (en fazla 5 saniye)
    for (int i = 0; i < 50; ++i) {
        if (connected_) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return connected_.load();
}

void TuiClient::disconnect() {
    running_ = false;
    ws_.stop();
}

void TuiClient::onMessage(const std::string& msg) {
    auto j = json::parse(msg, nullptr, false);
    if (!j.is_object()) return;

    std::string event = j.value("event", "");
    auto data = j.value("data", json::object());

    if (event == "fullSync") {
        applyFullSync(data);
    } else if (event == "bufferChanged") {
        // Re-request full sync on buffer change for simplicity
        // Basitlik icin buffer degisikliginde tam senkronizasyon iste
        requestSync();
    } else if (event == "cursorMoved") {
        applyCursorMoved(data);
    } else if (event == "tabChanged") {
        requestSync();
    }
}

void TuiClient::applyFullSync(const json& data) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    if (data.contains("buffer")) {
        auto& buf = data["buffer"];
        state_.lines.clear();
        if (buf.contains("lines")) {
            for (auto& l : buf["lines"]) {
                state_.lines.push_back(l.get<std::string>());
            }
        }
        state_.lineCount = buf.value("lineCount", 0);
        state_.filePath = buf.value("filePath", "");
        state_.modified = buf.value("modified", false);
    }
    if (data.contains("cursor")) {
        state_.cursorLine = data["cursor"].value("line", 0);
        state_.cursorCol = data["cursor"].value("col", 0);
    }
    state_.mode = data.value("mode", "normal");
    state_.activeIndex = data.value("activeIndex", 0);
    if (data.contains("buffers")) {
        state_.buffers.clear();
        for (auto& b : data["buffers"]) {
            state_.buffers.push_back(b);
        }
    }
}

void TuiClient::applyCursorMoved(const json& data) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    state_.cursorLine = data.value("line", 0);
    state_.cursorCol = data.value("col", 0);
}

void TuiClient::sendKey(const std::string& key, bool ctrl, bool alt) {
    json msg = {
        {"cmd", "input.key"},
        {"args", {{"code", key}, {"ctrl", ctrl}, {"alt", alt}}}
    };
    ws_.send(msg.dump());
}

void TuiClient::sendChar(const std::string& text) {
    json msg = {
        {"cmd", "input.char"},
        {"args", {{"text", text}}}
    };
    ws_.send(msg.dump());
}

void TuiClient::sendCommand(const std::string& cmd, const json& args) {
    json msg = {{"cmd", cmd}, {"args", args}};
    ws_.send(msg.dump());
}

void TuiClient::requestSync() {
    json msg = {{"action", "requestSync"}};
    ws_.send(msg.dump());
}

// ───────── Input Handling ─────────

// Handle key-down events: Ctrl+Q quits, everything else goes to server
// Tus-basma olaylarini isle: Ctrl+Q cikar, gerisi sunucuya gider
void TuiClient::handleKeyDown(const InputHandler::KeyEvent& ev) {
    // Ctrl+Q → quit
    if (ev.ctrl && (ev.ch == U'Q' || ev.ch == U'q')) {
        running_ = false;
        return;
    }

    // Build chord string and send to server
    // Akor dizesi olustur ve sunucuya gonder
    std::string chord = InputHandler::toChordString(ev);
    if (!chord.empty()) {
        sendKey(chord, ev.ctrl, ev.alt);
    }
}

// Handle character input: filter control chars, send printable chars to server
// Karakter girdisini isle: kontrol karakterlerini filtrele, yazilabilir karakterleri sunucuya gonder
void TuiClient::handleCharInput(const InputHandler::KeyEvent& ev) {
    if (!ev.isChar) return;

    // Filter control characters (handled in keyDown path)
    // Kontrol karakterlerini filtrele (keyDown yolunda islenir)
    if (ev.ch == U'\r' || ev.ch == U'\n' || ev.ch == U'\b') return;
    if (ev.ctrl) return;

    sendChar(ev.text);
}

// ───────── Rendering ─────────

// Render the current editor state to the terminal using BerkTerm
// BerkTerm kullanarak mevcut editor durumunu terminale render et
void TuiClient::render(BerkTerm& term) {
    // Take a snapshot of the state under lock
    // Kilit altinda durumun bir anlk goruntusunu al
    TuiState snap;
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        snap = state_;
    }

    term.beginFrame();

    // Title bar
    // Baslik cubugu
    {
        std::ostringstream os;
        os << "=== " << (snap.filePath.empty() ? "[scratch]" : snap.filePath)
           << " (" << snap.lineCount << " lines)"
           << (snap.modified ? " [+]" : "") << " ===";
        term.setTitle(os.str());
    }

    // Visible area (title = row 1, footer = last row)
    // Gorunur alan (baslik = satir 1, alt bilgi = son satir)
    const int top = 2;
    const int bottom = term.rows() - 1;
    const int visible = std::max(0, bottom - top + 1);

    // Auto-scroll to keep cursor visible
    // Imleci gorunur tutmak icin otomatik kaydir
    int first = topRow_;
    if (snap.cursorLine < first) first = snap.cursorLine;
    if (snap.cursorLine >= first + visible) first = snap.cursorLine - (visible - 1);
    int maxLine = std::max(0, (int)snap.lines.size() - 1);
    first = std::clamp(first, 0, maxLine);

    // Draw content lines with line number prefix
    // Satir numarasi oneki ile icerik satirlarini ciz
    for (int row = 0; row < visible; ++row) {
        int li = first + row;
        bool caret = (li == snap.cursorLine);
        std::string text = (li < (int)snap.lines.size()) ? snap.lines[li] : "";
        term.drawLinePadded(top + row, lineWithCaretPrefix(li, caret, text), caret);
    }

    // Footer: controls on left, cursor position on right
    // Alt bilgi: solda kontroller, sagda imlec konumu
    {
        std::string footerLeft = "[Ctrl+Q Quit]";
        std::ostringstream os;
        os << "Ln " << (snap.cursorLine + 1) << ", Col " << (snap.cursorCol + 1);
        term.drawStatus(footerLeft, os.str());
    }

    // Place caret on screen (1-based, prefix " >NNNN " = 7 chars)
    // Imleci ekrana yerlestir (1-tabanli, onek " >NNNN " = 7 karakter)
    int visRow = snap.cursorLine - first;
    int caretRow = top + visRow;
    int caretCol = 1 + 1 + 4 + 1 + snap.cursorCol; // prefix: " >NNNN "
    term.placeCaret(caretRow, std::max(1, caretCol));

    term.endFrame();

    // Save scroll state
    // Kayma durumunu kaydet
    topRow_ = first;
}

// ───────── Main Loop ─────────

// Main run loop: initialize terminal, start input, render at ~30fps
// Ana calistirma dongusu: terminali baslat, girdiyi baslat, ~30fps render et
void TuiClient::run() {
    running_ = true;
    BerkTerm term;
    term.clear();
    InputHandler input;

    // Wire input callbacks
    // Girdi geri cagirimlarini bagla
    input.setOnKeyDown([this](const InputHandler::KeyEvent& ev) {
        handleKeyDown(ev);
    });
    input.setOnCharInput([this](const InputHandler::KeyEvent& ev) {
        handleCharInput(ev);
    });

    input.start();

    // Render loop at ~30fps with diff flush
    // Fark gonderi ile ~30fps render dongusu
    while (running_ && connected_) {
        render(term);
        std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }

    input.stop();

    // Restore terminal state
    // Terminal durumunu geri yukle
    std::cout << "\x1b[0m\x1b[?25h\n";
}
