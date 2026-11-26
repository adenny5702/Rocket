#pragma once

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef NOGDI
#define NOGDI
#endif
#define CloseWindow RocketWin32CloseWindow
#define ShowCursor RocketWin32ShowCursor
#include <windows.h>
#undef ShowCursor
#undef CloseWindow
#ifdef DrawText
#undef DrawText
#endif
#ifdef DrawTextEx
#undef DrawTextEx
#endif

namespace rocket::ground::app {

class SerialPort {
public:
    SerialPort() = default;
    SerialPort(const SerialPort &) = delete;
    SerialPort &operator=(const SerialPort &) = delete;

    ~SerialPort() {
        close();
    }

    static std::vector<std::string> available_ports() {
        std::vector<std::string> ports;
        std::array<char, 1'024> target{};
        for (unsigned int number = 1; number <= 256; ++number) {
            const std::string name = "COM" + std::to_string(number);
            if (QueryDosDeviceA(name.c_str(), target.data(), static_cast<DWORD>(target.size())) !=
                0) {
                ports.push_back(name);
            }
        }
        std::sort(ports.begin(), ports.end());
        return ports;
    }

    bool open(std::string device) {
        close();

        if (device.rfind("\\\\.\\", 0) != 0) {
            device = "\\\\.\\" + device;
        }
        handle_ = CreateFileA(device.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle_ == INVALID_HANDLE_VALUE) {
            return false;
        }

        DCB settings{};
        settings.DCBlength = sizeof(settings);
        if (!GetCommState(handle_, &settings)) {
            close();
            return false;
        }
        settings.BaudRate = CBR_115200;
        settings.ByteSize = 8;
        settings.Parity = NOPARITY;
        settings.StopBits = ONESTOPBIT;
        settings.fBinary = TRUE;
        settings.fDtrControl = DTR_CONTROL_ENABLE;
        if (!SetCommState(handle_, &settings)) {
            close();
            return false;
        }

        COMMTIMEOUTS timeouts{};
        timeouts.ReadIntervalTimeout = MAXDWORD;
        if (!SetCommTimeouts(handle_, &timeouts)) {
            close();
            return false;
        }
        PurgeComm(handle_, PURGE_RXCLEAR | PURGE_TXCLEAR);

        return true;
    }

    void close() {
        if (handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(handle_);
            handle_ = INVALID_HANDLE_VALUE;
        }
        pending_.clear();
    }

    [[nodiscard]] bool is_open() const {
        return handle_ != INVALID_HANDLE_VALUE;
    }

    bool write_line(std::string_view line) {
        if (!is_open()) {
            return false;
        }

        std::string bytes(line);
        bytes.push_back('\n');
        std::size_t offset = 0;
        while (offset < bytes.size()) {
            const auto written = write_bytes(bytes.data() + offset, bytes.size() - offset);
            if (written <= 0) {
                return false;
            }
            offset += static_cast<std::size_t>(written);
        }
        return true;
    }

    bool read_line(std::string &line) {
        while (true) {
            const auto newline = pending_.find('\n');
            if (newline != std::string::npos) {
                line.assign(pending_, 0, newline);
                pending_.erase(0, newline + 1);
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                return true;
            }

            std::array<char, 256> buffer{};
            const auto count = read_bytes(buffer.data(), buffer.size());
            if (count <= 0) {
                return false;
            }

            pending_.append(buffer.data(), static_cast<std::size_t>(count));
        }
    }

private:
    std::string pending_;
    HANDLE handle_ = INVALID_HANDLE_VALUE;

    long write_bytes(const char *data, std::size_t size) {
        DWORD written = 0;
        if (!WriteFile(handle_, data, static_cast<DWORD>(size), &written, nullptr)) {
            return -1;
        }
        return static_cast<long>(written);
    }

    long read_bytes(char *data, std::size_t size) {
        DWORD count = 0;
        if (!ReadFile(handle_, data, static_cast<DWORD>(size), &count, nullptr)) {
            return -1;
        }
        return static_cast<long>(count);
    }
};

} // namespace rocket::ground::app
