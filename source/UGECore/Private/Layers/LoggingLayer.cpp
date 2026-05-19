#include <Layers/LoggingLayer.h>
#include <ServiceLocator.h>

std::expected<std::unique_ptr<LoggingLayer>, std::string> LoggingLayer::Create()
{
    return std::unique_ptr<LoggingLayer>(new LoggingLayer());
}



void LoggingLayer::Log(std::string Msg)
{
    // Strip trailing CR/LF
    while (!Msg.empty() && (Msg.back() == '\n' || Msg.back() == '\r'))
        Msg.pop_back();
    // Replace internal newlines with spaces
    for (auto& Ch : Msg)
        if (Ch == '\n' || Ch == '\r') Ch = ' ';

    if (Msg.empty()) return;

    m_lines.push_back(std::move(Msg));

    if (onLog)
        onLog(m_lines.back());
}

void LoggingLayer::Clear()
{
    m_lines.clear();
    if (onLog)
        onLog({});
}

void LoggingLayer::RegisterWithServiceLocator()
{
    ServiceLocator::Provide(this);
}

