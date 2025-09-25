# Implementation Guide: Task Dispatcher Architecture

## Overview

This document provides the step-by-step implementation guide for resolving Gladius's concurrent architecture issues, particularly the MCP save operation conflicts with OpenCL resource access.

## Critical Problem Summary

**Current Issue**: MCP `save_as` tool crashes due to:
1. Save operation triggers thumbnail generation 
2. Thumbnail generation requires OpenCL program recompilation
3. OpenCL resources are accessed from multiple threads without synchronization
4. UI thread blocks during long operations
5. No task queuing mechanism exists

## Phase 1: Immediate Fix (High Priority)

### Step 1: Create Basic Task Infrastructure

Create these new files in `gladius/src/threading/`:

#### A. ThreadSafeQueue Template

```cpp
// gladius/src/threading/ThreadSafeQueue.h
#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>

namespace gladius::threading {
    template<typename T>
    class ThreadSafeQueue {
    private:
        mutable std::mutex m_mutex;
        std::queue<T> m_queue;
        std::condition_variable m_condition;
        bool m_shutdown = false;

    public:
        void push(T item) {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_shutdown) {
                m_queue.push(std::move(item));
                m_condition.notify_one();
            }
        }

        std::optional<T> pop() {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_condition.wait(lock, [this] { return !m_queue.empty() || m_shutdown; });
            
            if (m_shutdown && m_queue.empty()) {
                return std::nullopt;
            }
            
            T result = std::move(m_queue.front());
            m_queue.pop();
            return result;
        }

        bool tryPop(T& item) {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_queue.empty()) {
                return false;
            }
            item = std::move(m_queue.front());
            m_queue.pop();
            return true;
        }

        void shutdown() {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_shutdown = true;
            m_condition.notify_all();
        }

        bool empty() const {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_queue.empty();
        }
    };
}
```

#### B. Simple Task System

```cpp
// gladius/src/threading/Task.h
#pragma once
#include <functional>
#include <string>
#include <future>

namespace gladius::threading {
    enum class TaskPriority {
        Low = 0,
        Normal = 1,
        High = 2,
        Critical = 3
    };

    class Task {
    public:
        virtual ~Task() = default;
        virtual void execute() = 0;
        virtual TaskPriority getPriority() const { return TaskPriority::Normal; }
        virtual std::string getDescription() const { return "Generic Task"; }
    };

    // Simple function wrapper task
    class FunctionTask : public Task {
    private:
        std::function<void()> m_function;
        std::string m_description;
        TaskPriority m_priority;

    public:
        FunctionTask(std::function<void()> func, 
                    const std::string& desc = "Function Task",
                    TaskPriority priority = TaskPriority::Normal)
            : m_function(std::move(func)), m_description(desc), m_priority(priority) {}

        void execute() override {
            m_function();
        }

        TaskPriority getPriority() const override { return m_priority; }
        std::string getDescription() const override { return m_description; }
    };
}
```

#### C. Basic TaskDispatcher

```cpp
// gladius/src/threading/TaskDispatcher.h
#pragma once
#include "Task.h"
#include "ThreadSafeQueue.h"
#include <thread>
#include <atomic>
#include <future>
#include <memory>

namespace gladius::threading {
    class TaskDispatcher {
    private:
        ThreadSafeQueue<std::unique_ptr<Task>> m_backgroundQueue;
        ThreadSafeQueue<std::unique_ptr<Task>> m_mainThreadQueue;
        
        std::vector<std::thread> m_workerThreads;
        std::atomic<bool> m_running{true};
        std::atomic<bool> m_headlessMode{false};

        static std::unique_ptr<TaskDispatcher> s_instance;
        static std::once_flag s_initFlag;

    public:
        TaskDispatcher();
        ~TaskDispatcher();

        static TaskDispatcher& getInstance();
        
        // Submit task to background thread
        std::future<void> submitBackgroundTask(std::unique_ptr<Task> task);
        
        // Submit task that must run on main thread
        void submitMainThreadTask(std::unique_ptr<Task> task);
        
        // Process main thread tasks (call from UI thread)
        void processMainThreadTasks(int maxTasks = 10);
        
        // Utility functions
        template<typename Func>
        std::future<void> submitFunction(Func&& func, const std::string& description = "") {
            auto task = std::make_unique<FunctionTask>(std::forward<Func>(func), description);
            return submitBackgroundTask(std::move(task));
        }
        
        template<typename Func>
        void submitMainThreadFunction(Func&& func, const std::string& description = "") {
            auto task = std::make_unique<FunctionTask>(std::forward<Func>(func), description);
            submitMainThreadTask(std::move(task));
        }
        
        void setHeadlessMode(bool headless) { m_headlessMode = headless; }
        bool isHeadlessMode() const { return m_headlessMode; }
        
        void shutdown();

    private:
        void workerThreadFunction();
    };
}
```

### Step 2: Implement OpenCL Resource Protection

```cpp
// gladius/src/threading/OpenCLResourceManager.h
#pragma once
#include "../ComputeContext.h"
#include <mutex>
#include <memory>
#include <chrono>

namespace gladius::threading {
    class OpenCLResourceManager {
    private:
        SharedComputeContext m_context;
        std::mutex m_accessMutex;
        std::atomic<bool> m_inUse{false};
        
    public:
        class OpenCLLock {
            friend class OpenCLResourceManager;
        private:
            OpenCLResourceManager* m_manager;
            std::unique_lock<std::mutex> m_lock;
            
        public:
            OpenCLLock(OpenCLResourceManager* manager, std::unique_lock<std::mutex>&& lock)
                : m_manager(manager), m_lock(std::move(lock)) {}
            
            ComputeContext& getContext() { return *m_manager->m_context; }
            
            bool isValid() const { return m_lock.owns_lock(); }
        };
        
        explicit OpenCLResourceManager(SharedComputeContext context)
            : m_context(std::move(context)) {}
        
        std::unique_ptr<OpenCLLock> acquireContext(
            std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));
        
        bool isContextInUse() const { return m_inUse.load(); }
    };
}
```

### Step 3: Modify ApplicationMCPAdapter for Async Operations

```cpp
// In ApplicationMCPAdapter.cpp - modify saveDocumentAs method:

bool ApplicationMCPAdapter::saveDocumentAs(const std::string& path) {
    if (!m_application) {
        m_lastErrorMessage = "No application instance available";
        return false;
    }

    auto document = m_application->getCurrentDocument();
    if (!document) {
        m_lastErrorMessage = "No active document available";
        return false;
    }

    // Validate path
    if (path.empty()) {
        m_lastErrorMessage = "File path cannot be empty";
        return false;
    }

    if (!path.ends_with(".3mf")) {
        m_lastErrorMessage = "File must have .3mf extension";
        return false;
    }

    // Submit save operation to background thread
    auto& dispatcher = threading::TaskDispatcher::getInstance();
    
    auto saveOperation = [this, path, document]() {
        try {
            // This runs on background thread, avoiding UI blocking
            document->saveAs(std::filesystem::path(path));
            
            // Update status on main thread if not headless
            if (!threading::TaskDispatcher::getInstance().isHeadlessMode()) {
                threading::TaskDispatcher::getInstance().submitMainThreadFunction([this, path]() {
                    m_lastErrorMessage = "Document saved successfully to " + path;
                    // Could trigger UI update here if needed
                });
            } else {
                m_lastErrorMessage = "Document saved successfully to " + path;
            }
        } catch (const std::exception& e) {
            m_lastErrorMessage = "Save failed: " + std::string(e.what());
        }
    };

    // Submit the save operation
    dispatcher.submitFunction(saveOperation, "Save document to " + path);
    
    m_lastErrorMessage = "Save operation started";
    return true; // Operation submitted successfully
}
```

### Step 4: Integrate with GLView Main Loop

```cpp
// In GLView.cpp - modify render() method to process main thread tasks:

void GLView::render() {
    FrameMark;

    glfwMakeContextCurrent(m_window);
    applyFullscreenMode();
    
    // Process main thread tasks before rendering
    threading::TaskDispatcher::getInstance().processMainThreadTasks();
    
    m_render();
    glFlush();
    glFinish();
    glPopMatrix();

    displayUI();
    
    // ... rest of render method
}
```

### Step 5: Update CMakeLists.txt

```cmake
# Add to gladius/src/CMakeLists.txt:

# Threading support
set(THREADING_SOURCES
    threading/ThreadSafeQueue.h
    threading/Task.h
    threading/TaskDispatcher.h
    threading/TaskDispatcher.cpp
    threading/OpenCLResourceManager.h
    threading/OpenCLResourceManager.cpp
)

# Add to your main target sources
target_sources(gladius PRIVATE ${THREADING_SOURCES})
```

## Testing the Fix

### Test 1: Basic Task Dispatcher
```cpp
// Test that background tasks don't block main thread
auto& dispatcher = TaskDispatcher::getInstance();
dispatcher.submitFunction([]() {
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "Background task completed" << std::endl;
});
// Main thread should continue immediately
```

### Test 2: MCP Save Operation
```bash
# Test the MCP save operation that was crashing
curl -X POST http://localhost:8080/mcp/tools/call \
  -H "Content-Type: application/json" \
  -d '{
    "name": "mcp_gladius_save_document_as",
    "arguments": {
      "path": "/tmp/test_gyroid.3mf"
    }
  }'
```

### Test 3: Concurrent Operations
```cpp
// Test multiple concurrent save operations
for (int i = 0; i < 5; i++) {
    adapter.saveDocumentAs("/tmp/test_" + std::to_string(i) + ".3mf");
}
// Should not crash or block UI
```

## Expected Results

After implementing Phase 1:

1. **No UI Blocking**: Save operations run on background threads
2. **No Resource Conflicts**: OpenCL operations are properly synchronized  
3. **MCP Responsiveness**: MCP server responds immediately, operations happen async
4. **Stable Operation**: No crashes from concurrent resource access

## Next Phases

- **Phase 2**: Advanced resource management with timeouts and priorities
- **Phase 3**: Full OpenCL context isolation per thread
- **Phase 4**: Comprehensive task types for all heavy operations

This phased approach provides immediate stability while building toward a fully concurrent architecture.
