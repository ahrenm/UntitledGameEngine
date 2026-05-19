#include <Layers/AppLayer.h>
#include <Layers/LoggingLayer.h>
#include <ServiceLocator.h>

void AppLayer::Log(std::string Msg) const
{
    if (auto* Logger = ServiceLocator::TryGet<LoggingLayer>())
        Logger->Log(std::move(Msg));
}

