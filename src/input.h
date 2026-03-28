#pragma once
#include <functional>
#include <string>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <mutex>

// Asynchronous keyboard input handler with chord binding support.
// Akor baglama destekli asenkron klavye girdi isleyicisi.
// Runs input reading in a separate thread, dispatches events via callbacks.
// Girdi okumayi ayri bir thread'de calistirir, olaylari geri cagirimlarla dagitir.
// Supports special keys, modifiers (Ctrl/Alt), and chord patterns like "Ctrl+S".
// Ozel tuslari, degistiricileri (Ctrl/Alt) ve "Ctrl+S" gibi akor kaliplarini destekler.
class InputHandler {
public:
    // Key code enumeration for special keys
    // Ozel tuslar icin tus kodu numaralandirmasi
    enum class KeyCode {
        Unknown = 0, Character,
        Enter, Escape, Backspace, Tab,
        ArrowUp, ArrowDown, ArrowLeft, ArrowRight,
        Home, End, PageUp, PageDown, DeleteKey, InsertKey,
        F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12
    };

    // Represents a single keyboard event with key info and modifiers
    // Tus bilgisi ve degistiricilerle tek bir klavye olayini temsil eder
    struct KeyEvent {
        KeyCode code = KeyCode::Unknown;   // Special key code / Ozel tus kodu
        char32_t ch = U'\0';               // Character codepoint (if character input) / Karakter kod noktasi (karakter girdisiyse)
        bool isChar = false;               // True if this is a typing event / Yazma olayiysa true
        bool ctrl = false;                 // Ctrl modifier pressed / Ctrl degistiricisi basildi
        bool alt = false;                  // Alt modifier pressed / Alt degistiricisi basildi
        bool shift = false;                // Shift modifier (unreliable in terminal) / Shift degistiricisi (terminalde guvenilmez)
        std::string text;                  // UTF-8 representation of the character / Karakterin UTF-8 temsili
    };

    // Callback type for key events
    // Tus olaylari icin geri cagirim turu
    using KeyCallback = std::function<void(const KeyEvent&)>;

    // Set callback for all key-down events (special keys + characters)
    // Tum tus-basma olaylari icin geri cagirim ayarla (ozel tuslar + karakterler)
    void setOnKeyDown(KeyCallback cb);

    // Set callback for character input only (typing events)
    // Yalnizca karakter girdisi icin geri cagirim ayarla (yazma olaylari)
    void setOnCharInput(KeyCallback cb);

    // Bind a callback to a specific chord pattern (e.g., "Ctrl+S", "Alt+Left")
    // Belirli bir akor kalibina geri cagirim bagla (orn: "Ctrl+S", "Alt+Left")
    void bindChord(const std::string& chord, KeyCallback cb);

    // Start the input reading thread
    // Girdi okuma thread'ini baslat
    void start();

    // Stop the input reading thread (non-blocking with 100ms timeout)
    // Girdi okuma thread'ini durdur (100ms zaman asimi ile bloklamamaz)
    void stop();

    // Generate a chord string from a key event (e.g., "Ctrl+S", "Enter")
    // Bir tus olayindan akor dizesi olustur (orn: "Ctrl+S", "Enter")
    static std::string toChordString(const KeyEvent& ev);

    ~InputHandler();

private:
    std::thread worker_;                                       // Input reading thread / Girdi okuma thread'i
    std::atomic<bool> running_{false};                         // Thread lifecycle flag / Thread yasam dongusu bayragi

    std::mutex cbMutex_;                                       // Protects callbacks / Geri cagirimlari korur
    KeyCallback onKeyDown_;                                    // Key-down callback / Tus-basma geri cagrimi
    KeyCallback onCharInput_;                                  // Char input callback / Karakter girdisi geri cagrimi
    std::unordered_map<std::string, KeyCallback> chordHandlers_;  // Chord -> callback map / Akor -> geri cagirim haritasi

    // Platform-independent input reading loop
    // Platform bagimsiz girdi okuma dongusu
    void loop();

    // Dispatch a key event to all registered callbacks
    // Bir tus olayini tum kayitli geri cagirimlara dagit
    void dispatch(const KeyEvent& ev);

    // Try to match and dispatch a chord binding
    // Bir akor baglamasini eslestirmeye ve dagitmaya calis
    void tryDispatchChord(const KeyEvent& ev);

    // Read a single byte from stdin (blocking with timeout)
    // stdin'den tek bir byte oku (zaman asimli bloklama)
    static int readByteBlocking();

    // Read a complete key event from the input stream
    // Girdi akisindan tam bir tus olayi oku
    static bool readKeyEvent(KeyEvent& out);
};
