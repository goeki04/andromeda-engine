#pragma once
#include "a_ISubsystem.hpp"
#include "a_FileSystemWatcher.hpp"
#include "a_threadSafeQueue.hpp"
namespace Andromeda::OS
{
    class OS_Manager : public ISubsystem
    {
    public:
        void start() override;
        void update() override;
        const char* getSubsystemName() const override
        {
            return "OS_Manager";
        }
    private:
        ThreadSafeQueue<FileEvent> m_FileEvents;
        std::unique_ptr<FileSystemWatcher> m_FileWatcher;
    };
}
