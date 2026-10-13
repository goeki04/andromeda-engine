#pragma once
#include "a_os_manager.hpp"

#include "a_logger.hpp"
#include "a_file_event.hpp"
#include "a_event_manager.hpp"
#include "a_EventTypes.hpp"
namespace Andromeda::OS{
    void OS_Manager::start()
    {
        m_FileWatcher = std::make_unique<FileSystemWatcher>(ASSET_PATH,
            [this](const FileAction action, const std::filesystem::path& path)
        {
            m_FileEvents.push(FileEvent{action, path.lexically_normal()});
        });
    }

    void OS_Manager::update()
    {
        for (const auto& event : m_FileEvents.dequeueAll())
        {
            EventManager::getInstance().Dispatch(FileChanged::GetStaticType(), FileChanged(event.action, event.path));
        }
    }
}
