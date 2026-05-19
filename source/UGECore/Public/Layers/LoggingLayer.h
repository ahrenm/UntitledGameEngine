#pragma once
#include "AppLayer.h"
#include <LayerRegistry.h>
#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// ── LoggingLayer ──────────────────────────────────────────────────────────────
// Owns the application-wide log line buffer.
// Construct and push into Application::layers_ before all other layers so it
// is available via ServiceLocator::get<LoggingLayer>() during init.
//
// Wire external subsystems with makeSink():
//   sdl_->setLogFunction(logging.makeSink());
//
// Register an observer so dependent systems (e.g. PanelViewModel) can react
// to new lines without LoggingLayer depending on them:
//   logging.onLog = panel.bindLogData(logging.lines());
class LoggingLayer : public AppLayer
{
public:
    REGISTER_LAYER("logging", 1.0f, LoggingLayer)

    [[nodiscard]] static std::expected<std::unique_ptr<LoggingLayer>, std::string> Create();

    // Fired after each call to Log() with the new line. May be null.
    std::function<void(const std::string&)> onLog;

    // Sanitises newlines and appends a line; no-ops on empty messages.
    void Log(std::string Msg);

    // Clears all stored lines and notifies the observer with an empty string.
    void Clear();

    [[nodiscard]] const std::vector<std::string>& Lines() const { return m_lines; }
    [[nodiscard]]       std::vector<std::string>& Lines()       { return m_lines; }

    // Returns a sink callable suitable for passing to SetLogFunction().
    [[nodiscard]] std::function<void(std::string)> MakeSink()
    {
        return [this](std::string Msg) { Log(std::move(Msg)); };
    }

    // ── AppLayer ──────────────────────────────────────────────────────────────
    void RegisterWithServiceLocator() override;

private:
    LoggingLayer() = default;

    std::vector<std::string> m_lines;
};
