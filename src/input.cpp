#include "input.h"
#include <iostream>
#include <vector>
#include <cstring>

#ifdef _WIN32
    #include <conio.h>
#else
    #include <termios.h>
    #include <unistd.h>
    #include <sys/select.h>
#endif

// POSIX helper: RAII guard to switch terminal to raw mode and restore on destruction
// POSIX yardimcisi: Terminali ham moda geciren ve yikim sirasinda geri yukleyen RAII koruyucu
#ifndef _WIN32
struct TermiosGuard {
    termios old{};
    bool ok{false};
    TermiosGuard() {
        if (tcgetattr(STDIN_FILENO, &old) == 0) {
            termios raw = old;
            cfmakeraw(&raw);
            raw.c_cc[VMIN] = 1;
            raw.c_cc[VTIME] = 0;
            if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0) ok = true;
        }
    }
    ~TermiosGuard() {
        if (ok) tcsetattr(STDIN_FILENO, TCSANOW, &old);
    }
};
#endif

// ---------------------- InputHandler: public API ----------------------

// Register a callback to be invoked on every key press event
// Her tus basma olayinda cagrilacak bir geri cagirim kaydet
void InputHandler::setOnKeyDown(KeyCallback cb) {
    std::lock_guard<std::mutex> lock(cbMutex_);
    onKeyDown_ = std::move(cb);
}

// Register a callback for printable character input only
// Yalnizca yazilabilir karakter girislerinde cagrilacak geri cagirim kaydet
void InputHandler::setOnCharInput(KeyCallback cb) {
    std::lock_guard<std::mutex> lock(cbMutex_);
    onCharInput_ = std::move(cb);
}

// Bind a key chord (e.g., "Ctrl+S") to a specific callback
// Bir tus kombinasyonunu (ornegin "Ctrl+S") belirli bir geri cagirim ile iliskilendir
void InputHandler::bindChord(const std::string& chord, KeyCallback cb) {
    std::lock_guard<std::mutex> lock(cbMutex_);
    chordHandlers_[chord] = std::move(cb);
}

// Start the input reading loop in a background thread
// Girdi okuma dongusunu arka plan is parcaciginda baslat
void InputHandler::start() {
    if (running_.exchange(true)) return;
    worker_ = std::thread([this]{ loop(); });
}

// Stop the input reading loop and join the background thread
// Girdi okuma dongusunu durdur ve arka plan is parcacigini bekle
void InputHandler::stop() {
    if (!running_.exchange(false)) return;
    if (worker_.joinable()) worker_.join();
}

// Destructor: ensure the input loop is stopped before destruction
// Yikici: yikimdan once girdi dongusunun durduruldugunu garanti et
InputHandler::~InputHandler() {
    stop();
}

// Main input reading loop: reads key events and dispatches them
// Ana girdi okuma dongusu: tus olaylarini okur ve dagitir
void InputHandler::loop() {
#ifndef _WIN32
    TermiosGuard guard;
#endif
    KeyEvent ev;
    while (running_) {
        if (!readKeyEvent(ev)) continue;
        dispatch(ev);
    }
}

// Dispatch a key event to chord handlers and registered callbacks
// Tus olayini kombinasyon isleyicilerine ve kayitli geri cagirimlara dagit
void InputHandler::dispatch(const KeyEvent& ev) {
    tryDispatchChord(ev);

    KeyCallback kd;
    KeyCallback ci;
    {
        std::lock_guard<std::mutex> lock(cbMutex_);
        kd = onKeyDown_;
        ci = onCharInput_;
    }
    if (kd) kd(ev);

    if (ev.isChar && ci) ci(ev);
}

// Try to match and dispatch a key chord binding (e.g., Ctrl+S)
// Tus kombinasyonu eslesmeyi dene ve varsa ilgili isleyiciyi cagir
void InputHandler::tryDispatchChord(const KeyEvent& ev) {
    std::string chord = toChordString(ev);
    if (chord.empty()) return;
    KeyCallback h;
    {
        std::lock_guard<std::mutex> lock(cbMutex_);
        auto it = chordHandlers_.find(chord);
        if (it != chordHandlers_.end()) h = it->second;
    }
    if (h) h(ev);
}

// Convert a key event into a chord string representation (e.g., "Ctrl+S", "Alt+Left")
// Tus olayini kombinasyon dizesine donustur (ornegin "Ctrl+S", "Alt+Left")
std::string InputHandler::toChordString(const KeyEvent& ev) {
    auto specialName = [&](KeyCode c) -> const char* {
        switch (c) {
            case KeyCode::Enter: return "Enter";
            case KeyCode::Escape: return "Escape";
            case KeyCode::Backspace: return "Backspace";
            case KeyCode::Tab: return "Tab";
            case KeyCode::ArrowUp: return "Up";
            case KeyCode::ArrowDown: return "Down";
            case KeyCode::ArrowLeft: return "Left";
            case KeyCode::ArrowRight: return "Right";
            case KeyCode::Home: return "Home";
            case KeyCode::End: return "End";
            case KeyCode::PageUp: return "PageUp";
            case KeyCode::PageDown: return "PageDown";
            case KeyCode::DeleteKey: return "Delete";
            case KeyCode::InsertKey: return "Insert";
            case KeyCode::F1: return "F1"; case KeyCode::F2: return "F2"; case KeyCode::F3: return "F3";
            case KeyCode::F4: return "F4"; case KeyCode::F5: return "F5"; case KeyCode::F6: return "F6";
            case KeyCode::F7: return "F7"; case KeyCode::F8: return "F8"; case KeyCode::F9: return "F9";
            case KeyCode::F10: return "F10"; case KeyCode::F11: return "F11"; case KeyCode::F12: return "F12";
            default: return nullptr;
        }
    };

    std::string out;

    // Special key
    // Ozel tus
    if (ev.code != KeyCode::Unknown && ev.code != KeyCode::Character) {
        if (ev.ctrl) out += "Ctrl+";
        if (ev.alt)  out += "Alt+";
        const char* n = specialName(ev.code);
        if (!n) return {};
        out += n;
        return out;
    }

    // Character input
    // Karakter girdisi
    if (ev.isChar && ev.ch != U'\0') {
        if (ev.ctrl) out += "Ctrl+";
        if (ev.alt)  out += "Alt+";
        out += ev.text.empty() ? std::string(1, (char)ev.ch) : ev.text;
        return out;
    }

    return {};
}

// Read a single byte from stdin with platform-specific blocking/timeout
// Platforma ozgu engelleme/zaman asimi ile stdin'den tek bir bayt oku
int InputHandler::readByteBlocking() {
#ifdef _WIN32
    return _getch() & 0xFF;
#else
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 100000; // 100ms

    int ret = select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv);
    if (ret <= 0) return -1;

    unsigned char c = 0;
    if (read(STDIN_FILENO, &c, 1) == 1) return c;
    return -1;
#endif
}

// ---------------------- Platform: key event extraction ----------------------

// Determine the byte length of a UTF-8 sequence from its leading byte
// Bas baytindan bir UTF-8 dizisinin bayt uzunlugunu belirle
static int utf8SeqLen(unsigned char lead) {
    if ((lead & 0x80) == 0)    return 1;
    if ((lead & 0xE0) == 0xC0) return 2;
    if ((lead & 0xF0) == 0xE0) return 3;
    if ((lead & 0xF8) == 0xF0) return 4;
    return 1;
}

// Convert a single byte to a UTF-8 string
// Tek bir bayti UTF-8 dizesine donustur
static std::string utf8FromChar(unsigned char c) {
    return std::string(1, static_cast<char>(c));
}

// Read and parse a complete key event from raw input bytes (platform-specific)
// Ham girdi baytlarindan tam bir tus olayini oku ve ayristir (platforma ozgu)
bool InputHandler::readKeyEvent(KeyEvent& out) {
    out = KeyEvent{};
#ifdef _WIN32
    int c = readByteBlocking();
    if (c < 0) return false;

    // Ctrl letters: 1..26 -> Ctrl+A..Z
    if (c >= 1 && c <= 26) {
        out.ctrl = true;
        out.code = KeyCode::Character;
        out.isChar = true;
        out.ch = static_cast<char32_t>('A' + (c - 1));
        out.text = std::string(1, static_cast<char>('A' + (c - 1)));
        return true;
    }

    if (c == 13) { out.code = KeyCode::Enter; return true; }
    if (c == 9)  { out.code = KeyCode::Tab; return true; }
    if (c == 27) { out.code = KeyCode::Escape; return true; }
    if (c == 8)  { out.code = KeyCode::Backspace; return true; }

    // Special keys: 0 or 224 prefix
    if (c == 0 || c == 224) {
        int k = readByteBlocking();
        switch (k) {
            case 72: out.code = KeyCode::ArrowUp;    return true;
            case 80: out.code = KeyCode::ArrowDown;  return true;
            case 75: out.code = KeyCode::ArrowLeft;  return true;
            case 77: out.code = KeyCode::ArrowRight; return true;
            case 71: out.code = KeyCode::Home;       return true;
            case 79: out.code = KeyCode::End;        return true;
            case 73: out.code = KeyCode::PageUp;     return true;
            case 81: out.code = KeyCode::PageDown;   return true;
            case 82: out.code = KeyCode::InsertKey;  return true;
            case 83: out.code = KeyCode::DeleteKey;  return true;
            default: return true;
        }
    }

    // Normal character
    out.isChar = true;
    out.code = KeyCode::Character;
    out.ch = static_cast<char32_t>(c);
    out.text = utf8FromChar((unsigned char)c);
    return true;

#else
    int c = readByteBlocking();
    if (c < 0) return false;

    // ESC => Alt mode or escape sequence
    if (c == 27) {
        int n1 = readByteBlocking();
        if (n1 == -1) { out.code = KeyCode::Escape; return true; }

        if (n1 != '[' && n1 != 'O') {
            out.alt = true;
            if (n1 >= 1 && n1 <= 26) {
                out.ctrl = true;
                out.isChar = true;
                out.code = KeyCode::Character;
                out.ch = static_cast<char32_t>('A' + (n1 - 1));
                out.text = std::string(1, static_cast<char>('A' + (n1 - 1)));
                return true;
            }
            out.isChar = true;
            out.code = KeyCode::Character;
            out.ch = static_cast<char32_t>(n1);
            out.text = utf8FromChar((unsigned char)n1);
            return true;
        }

        // ESC [ ... or ESC O ...
        int n2 = readByteBlocking();
        if (n1 == '[') {
            if (n2 == 'A') { out.code = KeyCode::ArrowUp;    return true; }
            if (n2 == 'B') { out.code = KeyCode::ArrowDown;  return true; }
            if (n2 == 'C') { out.code = KeyCode::ArrowRight; return true; }
            if (n2 == 'D') { out.code = KeyCode::ArrowLeft;  return true; }
            if (n2 == 'H') { out.code = KeyCode::Home;       return true; }
            if (n2 == 'F') { out.code = KeyCode::End;        return true; }
            if (n2 == '2') { int tilde = readByteBlocking(); (void)tilde; out.code = KeyCode::InsertKey;  return true; }
            if (n2 == '3') { int tilde = readByteBlocking(); (void)tilde; out.code = KeyCode::DeleteKey;  return true; }
            if (n2 == '5') { int tilde = readByteBlocking(); (void)tilde; out.code = KeyCode::PageUp;     return true; }
            if (n2 == '6') { int tilde = readByteBlocking(); (void)tilde; out.code = KeyCode::PageDown;   return true; }
            out.code = KeyCode::Unknown; return true;
        } else {
            if (n2 == 'P') { out.code = KeyCode::F1;  return true; }
            if (n2 == 'Q') { out.code = KeyCode::F2;  return true; }
            if (n2 == 'R') { out.code = KeyCode::F3;  return true; }
            if (n2 == 'S') { out.code = KeyCode::F4;  return true; }
            out.code = KeyCode::Unknown; return true;
        }
    }

    // Ctrl letters: 1..26
    if (c >= 1 && c <= 26) {
        out.ctrl = true;
        out.isChar = true;
        out.code = KeyCode::Character;
        out.ch = static_cast<char32_t>('A' + (c - 1));
        out.text = std::string(1, static_cast<char>('A' + (c - 1)));
        return true;
    }

    // Enter / Backspace / Tab
    if (c == 13 || c == 10) {
        out.isChar = false;
        out.code = KeyCode::Enter;
        out.ch = 0;
        out.text = "";
        return true;
    }
    if (c == 127) { out.code = KeyCode::Backspace; return true; }
    if (c == '\t') { out.code = KeyCode::Tab; return true; }

    // Normal character (with UTF-8 multi-byte support)
    out.isChar = true;
    out.code = KeyCode::Character;

    int seqLen = utf8SeqLen((unsigned char)c);
    if (seqLen > 1) {
        std::string mb(1, (char)c);
        for (int i = 1; i < seqLen; ++i) {
            int next = readByteBlocking();
            if (next < 0) break;
            mb += (char)next;
        }
        out.text = mb;
        out.ch = U'\0';
    } else {
        out.ch = static_cast<char32_t>(c);
        out.text = utf8FromChar((unsigned char)c);
    }
    return true;
#endif
}
