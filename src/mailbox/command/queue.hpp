#pragma once

#include <mutex>
#include <variant>
#include <vector>

#include "cmds.hpp"

namespace mailbox::command {
using Command =
    std::variant<SeedWorld, ResetWorld, Quit, ApplyRules, AddGroup, RemoveGroup,
                 RemoveAllGroups, ResizeGroup, Pause, Resume, OneStep>;

class Queue {
  public:
    void push(const Command &cmd) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push_back(cmd);
    }

    std::vector<Command> drain() {
        std::vector<Command> out;
        std::lock_guard<std::mutex> lock(m_mutex);

        out.swap(m_queue);

        return out;
    }

  private:
    std::mutex m_mutex;
    std::vector<Command> m_queue;
};
} // namespace mailbox::command
