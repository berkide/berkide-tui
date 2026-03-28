#pragma once
#include <string>
#include <vector>

// Double-buffered terminal renderer using ANSI escape codes.
// ANSI kacis kodlari kullanan cift tamponlu terminal olusturucu.
// Minimizes flicker by only flushing changed lines (diff flush).
// Yalnizca degisen satirlari gondererek titresimi en aza indirir (fark gonderi).
class BerkTerm {
public:
    BerkTerm();

    // Start a new frame: hide cursor, update terminal size
    // Yeni bir cerceve baslat: imleci gizle, terminal boyutunu guncelle
    void beginFrame();

    // Flush diff to terminal and show caret
    // Farklari terminale gonder ve imleci goster
    void endFrame();

    // Full clear (used on first launch)
    // Tam temizlik (ilk acilista kullanilir)
    void clear();

    // Set the title bar (row 1)
    // Baslik cubugunu ayarla (satir 1)
    void setTitle(const std::string& title);

    // Draw a content line at given row (2..rows-1), optionally inverted
    // Verilen satira icerik ciz (2..rows-1), istege bagli ters cevrilmis
    void drawLinePadded(int row, const std::string& text, bool invert = false);

    // Draw the status/footer bar (last row)
    // Durum/alt bilgi cubugunu ciz (son satir)
    void drawStatus(const std::string& left, const std::string& right);

    // Place the visible caret at given 1-based position
    // Gorunur imleci verilen 1-tabanli konuma yerlestir
    void placeCaret(int row, int col);

    int rows() const { return rows_; }
    int cols() const { return cols_; }

private:
    // Get terminal dimensions from the OS
    // Isletim sisteminden terminal boyutlarini al
    void updateWindowSize();

    // Resize back buffers to match terminal size
    // Arka tamponlari terminal boyutuna gore yeniden boyutlandir
    void ensureBackBuffers();

    // ANSI cursor positioning (1-based row, col)
    // ANSI imleç konumlandirma (1-tabanli satir, sutun)
    void gotoRC(int r, int c) const;
    void hideCursor() const;
    void showCursor() const;

private:
    int rows_ = 24;
    int cols_ = 80;

    std::vector<std::string> last_;    // Previous frame / Onceki cerceve
    std::vector<std::string> curr_;    // Current frame / Simdiki cerceve

    bool firstClear_ = true;
    int caretRow_ = 1;
    int caretCol_ = 1;
};
