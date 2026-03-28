#include "BerkTerm.h"
#include <iostream>
#include <algorithm>

#ifdef _WIN32
  #include <windows.h>
#else
  #include <sys/ioctl.h>
  #include <unistd.h>
#endif

// Counts visible length ignoring ANSI escape sequences
// ANSI kacis dizilerini yoksayarak gorunur uzunlugu sayar
static int visibleLength(const std::string& s) {
    int len = 0;
    bool inEscape = false;
    for (char c : s) {
        if (inEscape) {
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) inEscape = false;
            continue;
        }
        if (c == '\x1b') { inEscape = true; continue; }
        ++len;
    }
    return len;
}

// Pad a string with spaces to the given width (respecting ANSI codes)
// Verilen genislige kadar boslukla doldur (ANSI kodlarina dikkat ederek)
static std::string padRight(const std::string& s, int width) {
    int vis = visibleLength(s);
    if (vis >= width) return s;
    return s + std::string(width - vis, ' ');
}

BerkTerm::BerkTerm() {
#ifdef _WIN32
    // Enable Virtual Terminal Processing on Windows 10+
    // Windows 10+ uzerinde Sanal Terminal Islemesini etkinlestir
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
#endif
    updateWindowSize();
    ensureBackBuffers();
}

// Get terminal dimensions from the platform
// Platformdan terminal boyutlarini al
void BerkTerm::updateWindowSize() {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info)) {
        cols_ = info.srWindow.Right - info.srWindow.Left + 1;
        rows_ = info.srWindow.Bottom - info.srWindow.Top + 1;
    }
#else
    winsize ws{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        if (ws.ws_col) cols_ = ws.ws_col;
        if (ws.ws_row) rows_ = ws.ws_row;
    }
#endif
    cols_ = std::max(40, cols_);
    rows_ = std::max(10, rows_);
}

// Resize back buffers when terminal size changes
// Terminal boyutu degistiginde arka tamponlari yeniden boyutlandir
void BerkTerm::ensureBackBuffers() {
    last_.assign(rows_, std::string(cols_, ' '));
    curr_.assign(rows_, std::string(cols_, ' '));
}

void BerkTerm::gotoRC(int r, int c) const { std::cout << "\x1b[" << r << ";" << c << "H"; }
void BerkTerm::hideCursor() const { std::cout << "\x1b[?25l"; }
void BerkTerm::showCursor() const { std::cout << "\x1b[?25h"; }

// Full screen clear with ANSI reset
// ANSI sifirlama ile tam ekran temizligi
void BerkTerm::clear() {
    std::cout << "\x1b[0m\x1b[2J\x1b[H";
    std::cout.flush();
    firstClear_ = false;
}

// Begin a new render frame
// Yeni bir render cercevesi baslat
void BerkTerm::beginFrame() {
    updateWindowSize();
    if ((int)curr_.size() != rows_ || (int)curr_[0].size() != cols_)
        ensureBackBuffers();
    std::fill(curr_.begin(), curr_.end(), std::string(cols_, ' '));
    hideCursor();
    if (firstClear_) clear();
}

// End frame: flush only changed lines, then show caret
// Cerceve sonu: yalnizca degisen satirlari gonder, sonra imleci goster
void BerkTerm::endFrame() {
    for (int r = 0; r < rows_; ++r) {
        if (curr_[r] != last_[r]) {
            gotoRC(r + 1, 1);
            std::cout << curr_[r];
            last_[r] = curr_[r];
        }
    }
    gotoRC(caretRow_, caretCol_);
    showCursor();
    std::cout.flush();
}

// Set the title bar text (first row, orange background)
// Baslik cubugu metnini ayarla (ilk satir, turuncu arka plan)
void BerkTerm::setTitle(const std::string& title) {
    curr_[0] = "\x1b[48;5;208m" + padRight(title, cols_) + "\x1b[0m";
}

// Draw a padded content line at given row
// Verilen satira dolgulu icerik satiri ciz
void BerkTerm::drawLinePadded(int row, const std::string& text, bool invert) {
    if (row < 2 || row > rows_ - 1) return;
    std::string line = padRight(text, cols_);
    if (invert)
        line = "\x1b[7m" + line + "\x1b[0m";
    else
        line = "\x1b[0m" + line;
    curr_[row - 1] = line;
}

// Draw the status bar (last row, orange background)
// Durum cubugunu ciz (son satir, turuncu arka plan)
void BerkTerm::drawStatus(const std::string& left, const std::string& right) {
    std::string s = left;
    if (!right.empty() && (int)right.size() + 1 < cols_) {
        int gap = cols_ - (int)right.size() - 1;
        if ((int)s.size() >= gap) s = s.substr(0, gap);
        else s += std::string(gap - (int)s.size(), ' ');
        s += right;
    }
    curr_[rows_ - 1] = "\x1b[48;5;208m" + padRight(s, cols_) + "\x1b[0m";
}

// Set the caret position (1-based, clamped to screen)
// Imlec konumunu ayarla (1-tabanli, ekrana kenetlenmis)
void BerkTerm::placeCaret(int row, int col) {
    caretRow_ = std::clamp(row, 1, rows_);
    caretCol_ = std::clamp(col, 1, cols_);
}
