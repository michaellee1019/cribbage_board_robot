#pragma once

#include <epd3c/GxEPD2_290_C90c.h>

#include <algorithm>

// The Waveshare 2.9" (B) V4 demo uses SSD1680 display mode 2 for B/W
// partial updates. GxEPD2's C90c refresh methods use the color waveform.
// https://github.com/waveshareteam/e-Paper/blob/master/RaspberryPi_JetsonNano/c/lib/e-Paper/EPD_2in9b_V4.c
class LeaderboardPanel final : public GxEPD2_290_C90c {
public:
    using GxEPD2_290_C90c::GxEPD2_290_C90c;

    bool fullRefresh(const uint8_t* pixels, const uint8_t* red) {
        writeImage(pixels, red, 0, 0, WIDTH, HEIGHT);
        if (!activate(0xf7, 30000)) {
            return false;
        }

        // Mode 2 scans the panel even when writes use a smaller RAM window.
        // After showing the color frame, make both differential planes agree
        // everywhere, including at the red pixels, as in Waveshare's base-image demo.
        constexpr int lastByte = WIDTH / 8 - 1;
        constexpr int lastRow = HEIGHT - 1;
        setWindow(0, lastByte, 0, lastRow);
        writePlane(0x24, pixels, 0, lastByte, 0, lastRow, red);
        writePlane(0x26, pixels, 0, lastByte, 0, lastRow, red);
        return true;
    }

    bool partialRefresh(const uint8_t* previous, const uint8_t* next) {
        constexpr int stride = WIDTH / 8;
        int firstByte = stride;
        int lastByte = -1;
        int firstRow = HEIGHT;
        int lastRow = -1;
        for (int row = 0; row < HEIGHT; ++row) {
            for (int column = 0; column < stride; ++column) {
                const int offset = row * stride + column;
                if (previous[offset] != next[offset]) {
                    firstByte = std::min(firstByte, column);
                    lastByte = std::max(lastByte, column);
                    firstRow = std::min(firstRow, row);
                    lastRow = row;
                }
            }
        }
        if (lastRow < 0) {
            return true;
        }

        setWindow(firstByte, lastByte, firstRow, lastRow);
        writePlane(0x24, next, firstByte, lastByte, firstRow, lastRow);

        // Mode 2 compares the old and new black/white images. Supply power
        // around the same waveform used by Waveshare's partial refresh.
        if (!activate(0xc0, 1000) || !activate(0x1c, 6000) || !activate(0x03, 1000)) {
            return false;
        }
        // Keep unchanged areas neutral on the next panel-wide scan.
        writePlane(0x26, next, firstByte, lastByte, firstRow, lastRow);
        return true;
    }

private:
    void setWindow(int firstByte, int lastByte, int firstRow, int lastRow) {
        const uint8_t columns[]{0x44, static_cast<uint8_t>(firstByte), static_cast<uint8_t>(lastByte)};
        const uint8_t rows[]{0x45, lowByte(firstRow), highByte(firstRow),
                             lowByte(lastRow), highByte(lastRow)};
        _writeCommandData(columns, sizeof(columns));
        _writeCommandData(rows, sizeof(rows));
    }

    bool activate(uint8_t sequence, uint32_t timeoutMs) {
        const uint8_t update[]{0x22, sequence};
        _writeCommandData(update, sizeof(update));
        _writeCommand(0x20);
        delay(1);
        const uint32_t start = millis();
        while (digitalRead(_busy) == HIGH) {
            if (millis() - start >= timeoutMs) {
                return false;
            }
            delay(1);
        }
        return true;
    }

    void writePlane(uint8_t command, const uint8_t* pixels, int firstByte, int lastByte,
                    int firstRow, int lastRow, const uint8_t* red = nullptr) {
        const uint8_t x[]{0x4e, static_cast<uint8_t>(firstByte)};
        const uint8_t y[]{0x4f, lowByte(firstRow), highByte(firstRow)};
        _writeCommandData(x, sizeof(x));
        _writeCommandData(y, sizeof(y));
        _writeCommand(command);
        _startTransfer();
        for (int row = firstRow; row <= lastRow; ++row) {
            const int offset = row * (WIDTH / 8);
            for (int column = firstByte; column <= lastByte; ++column) {
                const int index = offset + column;
                _transfer(red ? pixels[index] & red[index] : pixels[index]);
            }
        }
        _endTransfer();
    }
};
