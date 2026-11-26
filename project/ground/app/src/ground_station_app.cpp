#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

#include "raylib.h"

#include "ground_station_app.hpp"
#include "orientation_mapping.hpp"
#include "rocket/shared/packet.hpp"
#include "serial_port.hpp"

namespace rocket::ground::app {

namespace {

class GroundStationWindow {
public:
    int run() {
        InitWindow(window_width, window_height, window_title);
        SetTargetFPS(target_frame_rate);
        rocket_view_ = LoadRenderTexture(viewport_width, viewport_height);
        load_ui_font();
        refresh_ports();

        while (!WindowShouldClose()) {
            read_serial();
            draw_frame();
        }

        if (owns_ui_font_) {
            UnloadFont(ui_font_);
        }
        UnloadRenderTexture(rocket_view_);
        CloseWindow();
        return 0;
    }

private:
    static constexpr int window_width = 1'100;
    static constexpr int window_height = 720;
    static constexpr int target_frame_rate = 60;
    static constexpr int viewport_width = 650;
    static constexpr int viewport_height = 520;
    static constexpr const char *window_title = "Ground Station";
    static constexpr int visible_port_count = 6;

    struct TelemetryView {
        bool received = false;
        std::uint16_t sequence = 0;
        std::uint32_t timestamp_ms = 0;
        rocket::protocol::FlightState state = rocket::protocol::FlightState::Standby;
        std::uint8_t flags = 0;
        std::int32_t altitude_cm = 0;
        std::int16_t vertical_speed_cms = 0;
        orientation_mapping::Quaternion rotation{1.0f, 0.0f, 0.0f, 0.0f};
        std::array<float, 3> gyro_dps{};
        std::array<float, 3> acceleration_g{};
    };

    struct ModeOption {
        const char *label;
        rocket::protocol::FlightState state;
        rocket::protocol::CommandType command;
    };

    static constexpr std::array<ModeOption, 3> mode_options{{
        {"Standby", rocket::protocol::FlightState::Standby,
         rocket::protocol::CommandType::EnterStandby},
        {"Launch mode", rocket::protocol::FlightState::LaunchMode,
         rocket::protocol::CommandType::EnterLaunchMode},
        {"Debug mode", rocket::protocol::FlightState::DebugMode,
         rocket::protocol::CommandType::EnterDebugMode},
    }};

    SerialPort serial_port_;
    RenderTexture2D rocket_view_{};
    Font ui_font_ = GetFontDefault();
    bool owns_ui_font_ = false;
    bool port_dropdown_open_ = false;
    bool mode_dropdown_open_ = false;
    bool block_commands_this_frame_ = false;
    std::vector<std::string> ports_;
    std::size_t port_scroll_offset_ = 0;
    std::string selected_port_;
    TelemetryView telemetry_;
    std::uint16_t next_sequence_ = 1;
    double last_telemetry_time_ = 0.0;
    std::string connection_status_ = "Select a USB COM port";
    std::string acknowledgement_status_ = "No command sent";

    void load_ui_font() {
        constexpr std::array<const char *, 2> font_paths{
            "C:/Windows/Fonts/segoeui.ttf",
            "C:/Windows/Fonts/arial.ttf",
        };

        for (const char *path : font_paths) {
            if (!FileExists(path)) {
                continue;
            }

            Font candidate = LoadFontEx(path, 192, nullptr, 0);
            if (candidate.texture.id != 0) {
                ui_font_ = candidate;
                owns_ui_font_ = true;
                return;
            }
        }
    }

    void refresh_ports() {
        ports_ = SerialPort::available_ports();
        port_scroll_offset_ = 0;

        if (std::find(ports_.begin(), ports_.end(), selected_port_) == ports_.end()) {
            selected_port_ = ports_.empty() ? std::string{} : ports_.front();
        }

        connection_status_ = ports_.empty() ? "No serial ports found" : "Select a USB COM port";
    }

    bool send_command(rocket::protocol::CommandType command, std::uint8_t channel = 0) {
        if (!serial_port_.is_open()) {
            connection_status_ = "Connect to the ground firmware first";
            return false;
        }

        std::ostringstream message;
        message << "CMD " << next_sequence_ << ' ' << static_cast<unsigned>(command) << ' '
                << static_cast<unsigned>(channel);
        if (!serial_port_.write_line(message.str())) {
            connection_status_ = "USB write failed";
            return false;
        }

        acknowledgement_status_ = "Command sent: " + std::to_string(next_sequence_);
        ++next_sequence_;
        return true;
    }

    void read_serial() {
        if (!serial_port_.is_open()) {
            return;
        }

        for (int count = 0; count < 32; ++count) {
            std::string line;
            if (!serial_port_.read_line(line)) {
                return;
            }
            process_serial_line(line);
        }
    }

    void process_serial_line(const std::string &line) {
        std::istringstream input(line);
        std::string kind;
        input >> kind;

        if (kind == "TEL") {
            read_telemetry(input);
        } else if (kind == "ACK") {
            read_acknowledgement(input);
        } else if (kind == "ERR") {
            std::string error;
            std::getline(input, error);
            connection_status_ = "Ground firmware error:" + error;
        }
    }

    void read_telemetry(std::istringstream &input) {
        unsigned int sequence = 0;
        unsigned int state = 0;
        unsigned int flags = 0;
        unsigned long timestamp_ms = 0;
        long altitude_cm = 0;
        int vertical_speed_cms = 0;
        std::array<int, 4> rotation{};
        std::array<int, 3> gyro{};
        std::array<int, 3> acceleration{};

        if (!(input >> sequence >> timestamp_ms >> state >> flags >> altitude_cm >>
              vertical_speed_cms >> rotation[0] >> rotation[1] >> rotation[2] >> rotation[3] >>
              gyro[0] >> gyro[1] >> gyro[2] >> acceleration[0] >> acceleration[1] >>
              acceleration[2])) {
            return;
        }

        telemetry_.received = true;
        telemetry_.sequence = static_cast<std::uint16_t>(sequence);
        telemetry_.timestamp_ms = static_cast<std::uint32_t>(timestamp_ms);
        telemetry_.state = static_cast<rocket::protocol::FlightState>(state);
        telemetry_.flags = static_cast<std::uint8_t>(flags);
        telemetry_.altitude_cm = static_cast<std::int32_t>(altitude_cm);
        telemetry_.vertical_speed_cms = static_cast<std::int16_t>(vertical_speed_cms);
        for (std::size_t index = 0; index < rotation.size(); ++index) {
            telemetry_.rotation[index] = static_cast<float>(rotation[index]) / 32767.0f;
        }
        for (std::size_t index = 0; index < gyro.size(); ++index) {
            telemetry_.gyro_dps[index] = static_cast<float>(gyro[index]) / 10.0f;
            telemetry_.acceleration_g[index] = static_cast<float>(acceleration[index]) / 1000.0f;
        }

        last_telemetry_time_ = GetTime();
        connection_status_ = "Telemetry received";
    }

    void read_acknowledgement(std::istringstream &input) {
        unsigned int sequence = 0;
        unsigned int command = 0;
        unsigned int response = 0;
        unsigned int state = 0;
        unsigned int channel = 0;
        if (!(input >> sequence >> command >> response >> state >> channel)) {
            return;
        }

        telemetry_.state = static_cast<rocket::protocol::FlightState>(state);
        const char *result = "unknown result";
        if (response == static_cast<unsigned>(rocket::protocol::AckResponse::Accepted)) {
            result = "accepted";
        } else if (response == static_cast<unsigned>(rocket::protocol::AckResponse::Rejected)) {
            result = "rejected";
        } else if (response == static_cast<unsigned>(rocket::protocol::AckResponse::Simulated)) {
            result = "simulated only; no output fired";
        }

        acknowledgement_status_ = "Command " + std::to_string(sequence) + ": " + result;
        if (command == static_cast<unsigned>(rocket::protocol::CommandType::TestPyroChannel)) {
            acknowledgement_status_ = "Pyro channel " + std::to_string(channel) + ": " + result;
        }
    }

    void update_port_controls() {
        block_commands_this_frame_ = false;
        const Rectangle port_button{20, 58, 220, 36};
        const bool select_clicked = draw_button(
            port_button, selected_port_.empty() ? "Select COM port" : selected_port_.c_str());
        if (select_clicked) {
            port_dropdown_open_ = !port_dropdown_open_;
            mode_dropdown_open_ = false;
            block_commands_this_frame_ = true;
        }

        if (draw_button({250, 58, 100, 36}, "Refresh")) {
            refresh_ports();
            block_commands_this_frame_ = true;
        }

        if (serial_port_.is_open()) {
            if (draw_button({360, 58, 120, 36}, "Disconnect")) {
                serial_port_.close();
                connection_status_ = "Disconnected";
                block_commands_this_frame_ = true;
            }
        } else if (draw_button({360, 58, 100, 36}, "Connect", !selected_port_.empty())) {
            if (serial_port_.open(selected_port_)) {
                connection_status_ = "Connected to " + selected_port_;
            } else {
                connection_status_ = "Could not open " + selected_port_;
            }
            block_commands_this_frame_ = true;
        }

        if (port_dropdown_open_) {
            block_commands_this_frame_ = true;
            const int wheel = static_cast<int>(GetMouseWheelMove());
            const std::size_t max_offset =
                ports_.size() > visible_port_count ? ports_.size() - visible_port_count : 0;
            if (wheel > 0 && port_scroll_offset_ > 0) {
                --port_scroll_offset_;
            } else if (wheel < 0 && port_scroll_offset_ < max_offset) {
                ++port_scroll_offset_;
            }

            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !select_clicked) {
                const Vector2 mouse = GetMousePosition();
                const int row_height = 30;
                const int list_top = 98;
                bool row_selected = false;
                for (int row = 0; row < visible_port_count; ++row) {
                    const std::size_t port_index = port_scroll_offset_ + row;
                    const Rectangle row_bounds{20, static_cast<float>(list_top + row * row_height),
                                               220, static_cast<float>(row_height)};
                    if (port_index < ports_.size() && CheckCollisionPointRec(mouse, row_bounds)) {
                        if (selected_port_ != ports_[port_index] && serial_port_.is_open()) {
                            serial_port_.close();
                        }
                        selected_port_ = ports_[port_index];
                        port_dropdown_open_ = false;
                        if (!serial_port_.is_open()) {
                            connection_status_ =
                                "Selected " + selected_port_ + "; connect to use it";
                        }
                        row_selected = true;
                        break;
                    }
                }

                const Rectangle list_bounds{20, static_cast<float>(list_top), 220,
                                            static_cast<float>(visible_port_count * row_height)};
                if (!row_selected && !CheckCollisionPointRec(mouse, list_bounds)) {
                    port_dropdown_open_ = false;
                }
            }
        }
    }

    void draw_port_dropdown() const {
        if (!port_dropdown_open_) {
            return;
        }

        constexpr int row_height = 30;
        constexpr int list_top = 98;
        DrawRectangle(20, list_top, 220, visible_port_count * row_height, RAYWHITE);
        DrawRectangleLines(20, list_top, 220, visible_port_count * row_height, DARKGRAY);

        if (ports_.empty()) {
            draw_text("No COM ports found", 30, list_top + 7, 17, GRAY);
            return;
        }

        for (int row = 0; row < visible_port_count; ++row) {
            const std::size_t port_index = port_scroll_offset_ + row;
            if (port_index >= ports_.size()) {
                break;
            }

            const Rectangle bounds{21, static_cast<float>(list_top + row * row_height + 1), 218,
                                   static_cast<float>(row_height - 1)};
            if (ports_[port_index] == selected_port_) {
                DrawRectangleRec(bounds, SKYBLUE);
            }
            draw_text(ports_[port_index].c_str(), 30, bounds.y + 5, 17, DARKGRAY);
        }

        if (ports_.size() > visible_port_count) {
            draw_text("Scroll to see more ports", 30,
                      list_top + visible_port_count * row_height - 22, 13, GRAY);
        }
    }

    static const char *flight_state_name(rocket::protocol::FlightState state) {
        switch (state) {
        case rocket::protocol::FlightState::Standby:
            return "Standby";
        case rocket::protocol::FlightState::LaunchMode:
            return "Launch mode";
        case rocket::protocol::FlightState::DebugMode:
            return "Debug mode";
        case rocket::protocol::FlightState::InFlight:
            return "In flight";
        default:
            return "Unknown";
        }
    }

    void draw_text(const char *text, float x, float y, float size, Color color) const {
        DrawTextEx(ui_font_, text, {x, y}, size, 1.0f, color);
    }

    bool draw_button(Rectangle bounds, const char *label, bool enabled = true) const {
        const bool hovered = CheckCollisionPointRec(GetMousePosition(), bounds);
        const Color background = !enabled ? LIGHTGRAY : (hovered ? SKYBLUE : DARKBLUE);
        DrawRectangleRec(bounds, background);
        DrawRectangleLinesEx(bounds, 1.0f, DARKGRAY);
        draw_text(label, bounds.x + 9, bounds.y + 7, 18, enabled ? RAYWHITE : GRAY);
        return enabled && hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    }

    void draw_controls() {
        if (block_commands_this_frame_) {
            return;
        }

        const Rectangle mode_button{20, 120, 220, 36};
        const Rectangle mode_menu{20, 158, 220, 102};
        std::string mode_label = "Mode: ";
        mode_label += flight_state_name(telemetry_.state);
        mode_label += "  v";

        const bool mode_button_clicked = draw_button(mode_button, mode_label.c_str());
        if (mode_button_clicked) {
            mode_dropdown_open_ = !mode_dropdown_open_;
        }

        if (mode_dropdown_open_) {
            block_commands_this_frame_ = true;
            if (!mode_button_clicked && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                const Vector2 mouse = GetMousePosition();
                bool option_selected = false;
                for (std::size_t index = 0; index < mode_options.size(); ++index) {
                    const Rectangle option_bounds{21.0f, 159.0f + static_cast<float>(index) * 34.0f,
                                                  218.0f, 33.0f};
                    if (CheckCollisionPointRec(mouse, option_bounds)) {
                        send_command(mode_options[index].command);
                        mode_dropdown_open_ = false;
                        option_selected = true;
                        break;
                    }
                }

                if (!option_selected && !CheckCollisionPointRec(mouse, mode_button) &&
                    !CheckCollisionPointRec(mouse, mode_menu)) {
                    mode_dropdown_open_ = false;
                }
            }
            return;
        }

        if (mode_button_clicked) {
            block_commands_this_frame_ = true;
            return;
        }

        const bool debug_mode = telemetry_.state == rocket::protocol::FlightState::DebugMode;
        if (draw_button({256, 120, 144, 36}, "Test pyro 0", debug_mode)) {
            send_command(rocket::protocol::CommandType::TestPyroChannel, 0);
        }
        if (draw_button({410, 120, 144, 36}, "Test pyro 1", debug_mode)) {
            send_command(rocket::protocol::CommandType::TestPyroChannel, 1);
        }
        if (draw_button({564, 120, 150, 36}, "Zero rotation", serial_port_.is_open())) {
            send_command(rocket::protocol::CommandType::ZeroRotation);
        }
    }

    void render_rocket_view() const {
        BeginTextureMode(rocket_view_);
        ClearBackground(Color{245, 247, 250, 255});

        Camera3D camera{};
        camera.position = {4.0f, 3.0f, 4.0f};
        camera.target = {0.0f, 0.1f, 0.0f};
        camera.up = {0.0f, 1.0f, 0.0f};
        camera.fovy = 45.0f;
        camera.projection = CAMERA_PERSPECTIVE;

        BeginMode3D(camera);
        DrawGrid(10, 0.5f);
        draw_rocket();
        EndMode3D();
        EndTextureMode();
    }

    void draw_rocket_view() const {
        const Rectangle viewport{20, 170, static_cast<float>(viewport_width),
                                 static_cast<float>(viewport_height)};
        DrawTextureRec(
            rocket_view_.texture,
            {0.0f, 0.0f, static_cast<float>(viewport_width), -static_cast<float>(viewport_height)},
            {viewport.x, viewport.y}, WHITE);
        DrawRectangleLinesEx(viewport, 1.0f, LIGHTGRAY);
        draw_text("Rocket orientation", 34, 180, 22, DARKGRAY);
    }

    void draw_mode_dropdown() const {
        if (!mode_dropdown_open_) {
            return;
        }

        constexpr Rectangle menu_bounds{20, 158, 220, 102};
        DrawRectangleRec(menu_bounds, RAYWHITE);
        DrawRectangleLinesEx(menu_bounds, 1.0f, DARKGRAY);

        for (std::size_t index = 0; index < mode_options.size(); ++index) {
            const Rectangle option_bounds{21.0f, 159.0f + static_cast<float>(index) * 34.0f, 218.0f,
                                          33.0f};
            const bool selected = telemetry_.state == mode_options[index].state;
            const bool hovered = CheckCollisionPointRec(GetMousePosition(), option_bounds);
            const Color background = selected ? SKYBLUE : (hovered ? LIGHTGRAY : RAYWHITE);
            DrawRectangleRec(option_bounds, background);
            draw_text(mode_options[index].label, option_bounds.x + 10.0f, option_bounds.y + 7.0f,
                      18.0f, DARKGRAY);
        }
    }

    void draw_rocket() const {
        const auto rocket_rotation = orientation_mapping::to_rocket_model(telemetry_.rotation);
        float w = rocket_rotation[0];
        float x = rocket_rotation[1];
        float y = rocket_rotation[2];
        float z = rocket_rotation[3];
        const float norm = std::sqrt(w * w + x * x + y * y + z * z);
        if (norm > 0.001f) {
            w /= norm;
            x /= norm;
            y /= norm;
            z /= norm;
        } else {
            w = 1.0f;
            x = y = z = 0.0f;
        }

        const auto rotate_model_vector = [w, x, y, z](Vector3 vector) {
            return Vector3{
                (1.0f - 2.0f * (y * y + z * z)) * vector.x + 2.0f * (x * y - w * z) * vector.y +
                    2.0f * (x * z + w * y) * vector.z,
                2.0f * (x * y + w * z) * vector.x + (1.0f - 2.0f * (x * x + z * z)) * vector.y +
                    2.0f * (y * z - w * x) * vector.z,
                2.0f * (x * z - w * y) * vector.x + 2.0f * (y * z + w * x) * vector.y +
                    (1.0f - 2.0f * (x * x + y * y)) * vector.z,
            };
        };

        const Vector3 body_base = rotate_model_vector({0.0f, -0.9f, 0.0f});
        const Vector3 body_top = rotate_model_vector({0.0f, 0.65f, 0.0f});
        const Vector3 nose_tip = rotate_model_vector({0.0f, 1.15f, 0.0f});
        DrawCylinderEx(body_base, body_top, 0.22f, 0.22f, 20, BLUE);
        DrawCylinderEx(body_top, nose_tip, 0.22f, 0.0f, 20, RED);

        const Vector3 marker_base = rotate_model_vector({0.205f, -0.55f, 0.0f});
        const Vector3 marker_tip = rotate_model_vector({0.205f, 0.55f, 0.0f});
        DrawCylinderEx(marker_base, marker_tip, 0.035f, 0.035f, 8, GOLD);
    }

    void draw_telemetry() const {
        constexpr int panel_x = 700;
        draw_text("Telemetry", panel_x, 174, 26, DARKGRAY);
        draw_text(TextFormat("USB: %s", connection_status_.c_str()), panel_x, 210, 18, DARKGRAY);
        draw_text(TextFormat("Mode: %s", flight_state_name(telemetry_.state)), panel_x, 240, 22,
                  DARKBLUE);

        if (!telemetry_.received) {
            draw_text("Waiting for telemetry...", panel_x, 292, 19, GRAY);
        } else {
            draw_text(TextFormat("Sequence: %u", static_cast<unsigned>(telemetry_.sequence)),
                      panel_x, 282, 18, DARKGRAY);
            draw_text(TextFormat("Rocket time: %lu ms",
                                 static_cast<unsigned long>(telemetry_.timestamp_ms)),
                      panel_x, 310, 18, DARKGRAY);
            draw_text(TextFormat("Age: %.2f s", GetTime() - last_telemetry_time_), panel_x, 338, 18,
                      DARKGRAY);

            if ((telemetry_.flags & rocket::protocol::OrientationValid) != 0) {
                draw_text("Orientation", panel_x, 382, 18, DARKGRAY);
                draw_text(
                    TextFormat("w % .3f   x % .3f", telemetry_.rotation[0], telemetry_.rotation[1]),
                    panel_x, 410, 18, DARKGRAY);
                draw_text(
                    TextFormat("y % .3f   z % .3f", telemetry_.rotation[2], telemetry_.rotation[3]),
                    panel_x, 438, 18, DARKGRAY);
            }

            if ((telemetry_.flags & rocket::protocol::ImuDataValid) != 0) {
                draw_text(TextFormat("Gyro dps:  % .1f  % .1f  % .1f", telemetry_.gyro_dps[0],
                                     telemetry_.gyro_dps[1], telemetry_.gyro_dps[2]),
                          panel_x, 478, 17, DARKGRAY);
                draw_text(TextFormat("Accel g:   % .2f  % .2f  % .2f", telemetry_.acceleration_g[0],
                                     telemetry_.acceleration_g[1], telemetry_.acceleration_g[2]),
                          panel_x, 506, 17, DARKGRAY);
            } else {
                draw_text("Waiting for IMU samples", panel_x, 478, 17, GRAY);
            }
        }

        draw_text("Command status", panel_x, 630, 18, DARKGRAY);
        draw_text(acknowledgement_status_.c_str(), panel_x, 658, 17, DARKBLUE);
    }

    void draw_frame() {
        BeginDrawing();

        render_rocket_view();

        ClearBackground(RAYWHITE);
        update_port_controls();
        draw_controls();
        draw_rocket_view();
        draw_telemetry();
        draw_mode_dropdown();
        draw_port_dropdown();
        EndDrawing();
    }
};

} // namespace

int GroundStationApp::run() {
    GroundStationWindow window;
    return window.run();
}

} // namespace rocket::ground::app
