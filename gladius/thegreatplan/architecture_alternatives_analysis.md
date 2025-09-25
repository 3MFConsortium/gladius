# Architectural Alternatives Analysis: Coroutines vs Task Dispatching

## Overview

We need to solve the fundamental problem: **MCP save operations conflict with OpenCL resource access, causing crashes**. Let's analyze different architectural approaches and their trade-offs.

## Option 1: C++20 Coroutines Architecture

### Core Concept
Use C++20 coroutines to make asynchronous operations look synchronous, with proper resource management through RAII and co_await.

### Implementation Example

```cpp
namespace gladius::async {
    // Coroutine-based resource management
    class OpenCLAwaiter {
    private:
        SharedComputeContext m_context;
        std::chrono::milliseconds m_timeout;
        
    public:
        OpenCLAwaiter(SharedComputeContext context, std::chrono::milliseconds timeout = 5000ms)
            : m_context(context), m_timeout(timeout) {}
            
        bool await_ready() const noexcept {
            // Check if OpenCL context is immediately available
            return !m_context->isInUse();
        }
        
        void await_suspend(std::coroutine_handle<> handle) {
            // Schedule resumption when context becomes available
            m_context->scheduleWhenAvailable(handle, m_timeout);
        }
        
        OpenCLContext await_resume() {
            // Return the acquired context
            return OpenCLContext{m_context};
        }
    };
    
    // Document access awaiter
    class DocumentAwaiter {
    private:
        std::shared_ptr<Document> m_document;
        bool m_writeAccess;
        
    public:
        DocumentAwaiter(std::shared_ptr<Document> doc, bool write = false)
            : m_document(doc), m_writeAccess(write) {}
            
        bool await_ready() const noexcept {
            return m_writeAccess ? !m_document->hasActiveReaders() : true;
        }
        
        void await_suspend(std::coroutine_handle<> handle) {
            if (m_writeAccess) {
                m_document->scheduleWhenWritable(handle);
            }
        }
        
        DocumentAccess await_resume() {
            return DocumentAccess{m_document, m_writeAccess};
        }
    };
    
    // Coroutine task type
    template<typename T = void>
    struct Task {
        struct promise_type {
            Task get_return_object() {
                return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
            }
            
            std::suspend_never initial_suspend() { return {}; }
            std::suspend_never final_suspend() noexcept { return {}; }
            
            void return_void() {}
            void unhandled_exception() { std::rethrow_exception(std::current_exception()); }
        };
        
        std::coroutine_handle<promise_type> coro;
        
        Task(std::coroutine_handle<promise_type> h) : coro(h) {}
        ~Task() { if (coro) coro.destroy(); }
        
        // Move-only
        Task(const Task&) = delete;
        Task& operator=(const Task&) = delete;
        Task(Task&& other) noexcept : coro(std::exchange(other.coro, {})) {}
        Task& operator=(Task&& other) noexcept {
            if (this != &other) {
                if (coro) coro.destroy();
                coro = std::exchange(other.coro, {});
            }
            return *this;
        }
    };
    
    // Utility functions for awaiting resources
    OpenCLAwaiter acquireOpenCL(SharedComputeContext context) {
        return OpenCLAwaiter{context};
    }
    
    DocumentAwaiter acquireDocument(std::shared_ptr<Document> doc, bool write = false) {
        return DocumentAwaiter{doc, write};
    }
}

namespace gladius {
    // Coroutine-based MCP adapter
    class CoroutineApplicationMCPAdapter : public MCPApplicationInterface {
    private:
        Application* m_application;
        
    public:
        // Save operation using coroutines - looks synchronous but is async
        async::Task<bool> saveDocumentAsCoroutine(const std::string& path) {
            if (!m_application) {
                m_lastErrorMessage = "No application instance available";
                co_return false;
            }
            
            auto document = m_application->getCurrentDocument();
            if (!document) {
                m_lastErrorMessage = "No active document available";
                co_return false;
            }
            
            // Validate path
            if (path.empty() || !path.ends_with(".3mf")) {
                m_lastErrorMessage = "Invalid file path";
                co_return false;
            }
            
            try {
                // Acquire document write access - this might suspend
                auto docAccess = co_await async::acquireDocument(document, true);
                
                // Perform the save operation
                document->saveAs(std::filesystem::path(path));
                
                // If thumbnail generation is needed, acquire OpenCL context
                if (needsThumbnailGeneration()) {
                    auto openclContext = co_await async::acquireOpenCL(m_application->getComputeContext());
                    generateThumbnail(document, openclContext);
                }
                
                m_lastErrorMessage = "Document saved successfully";
                co_return true;
                
            } catch (const std::exception& e) {
                m_lastErrorMessage = "Save failed: " + std::string(e.what());
                co_return false;
            }
        }
        
        // Traditional interface wraps coroutine
        bool saveDocumentAs(const std::string& path) override {
            // Start coroutine and return immediately
            auto task = saveDocumentAsCoroutine(path);
            
            // For now, return true to indicate operation started
            // In practice, you'd have a completion callback mechanism
            m_lastErrorMessage = "Save operation started";
            return true;
        }
    };
}
```

### Coroutine Benefits
- **Readable Code**: Async operations look like synchronous code
- **Natural Resource Management**: RAII works perfectly with co_await
- **Composability**: Easy to chain async operations
- **Exception Safety**: Standard exception handling works
- **Performance**: Zero-cost abstractions, minimal overhead

### Coroutine Drawbacks
- **C++20 Requirement**: Needs modern compiler support
- **Learning Curve**: Coroutines are complex to implement correctly
- **Debugging**: Harder to debug suspended coroutines
- **Ecosystem**: Limited library support compared to task-based systems

## Option 2: Actor Model Architecture

### Core Concept
Each major component becomes an actor that processes messages. No shared state, all communication through message passing.

```cpp
namespace gladius::actors {
    template<typename MessageType>
    class Actor {
    private:
        ThreadSafeQueue<MessageType> m_messageQueue;
        std::thread m_processingThread;
        std::atomic<bool> m_running{true};
        
    protected:
        virtual void handleMessage(const MessageType& message) = 0;
        
    public:
        Actor() : m_processingThread(&Actor::processMessages, this) {}
        virtual ~Actor() {
            m_running = false;
            if (m_processingThread.joinable()) {
                m_processingThread.join();
            }
        }
        
        void sendMessage(MessageType message) {
            m_messageQueue.push(std::move(message));
        }
        
    private:
        void processMessages() {
            while (m_running) {
                auto message = m_messageQueue.pop();
                if (message) {
                    handleMessage(*message);
                }
            }
        }
    };
    
    // Document messages
    struct DocumentMessage {
        enum Type { Save, Load, CreateFunction, Export };
        Type type;
        std::string path;
        std::function<void(bool, std::string)> callback;
    };
    
    // OpenCL messages
    struct OpenCLMessage {
        enum Type { Render, GenerateThumbnail, CompileProgram };
        Type type;
        std::any parameters;
        std::function<void(std::any)> callback;
    };
    
    // Document actor - handles all document operations
    class DocumentActor : public Actor<DocumentMessage> {
    private:
        std::shared_ptr<Document> m_document;
        
    protected:
        void handleMessage(const DocumentMessage& message) override {
            switch (message.type) {
                case DocumentMessage::Save:
                    try {
                        m_document->saveAs(std::filesystem::path(message.path));
                        message.callback(true, "Document saved successfully");
                    } catch (const std::exception& e) {
                        message.callback(false, "Save failed: " + std::string(e.what()));
                    }
                    break;
                // ... other cases
            }
        }
    };
    
    // OpenCL actor - handles all OpenCL operations
    class OpenCLActor : public Actor<OpenCLMessage> {
    private:
        SharedComputeContext m_context;
        
    protected:
        void handleMessage(const OpenCLMessage& message) override {
            switch (message.type) {
                case OpenCLMessage::Render:
                    // Perform rendering
                    break;
                case OpenCLMessage::GenerateThumbnail:
                    // Generate thumbnail
                    break;
                // ... other cases
            }
        }
    };
}
```

### Actor Model Benefits
- **No Shared State**: Eliminates race conditions by design
- **Clear Isolation**: Each actor is independent
- **Fault Tolerance**: One actor failure doesn't affect others
- **Scalability**: Easy to distribute across threads/processes

### Actor Model Drawbacks
- **Message Overhead**: All communication requires serialization
- **Complexity**: Requires complete architecture rewrite
- **Debugging**: Harder to trace execution across actors
- **Performance**: Message passing overhead for simple operations

## Option 3: Reactive Streams (RxCpp)

### Core Concept
Use reactive programming with observables and operators to handle async operations.

```cpp
namespace gladius::reactive {
    using namespace rxcpp;
    
    class ReactiveApplicationMCPAdapter : public MCPApplicationInterface {
    private:
        Application* m_application;
        rxcpp::subjects::subject<DocumentOperation> m_documentOperations;
        rxcpp::subjects::subject<OpenCLOperation> m_openCLOperations;
        
    public:
        ReactiveApplicationMCPAdapter(Application* app) : m_application(app) {
            // Set up processing pipelines
            setupDocumentPipeline();
            setupOpenCLPipeline();
        }
        
        bool saveDocumentAs(const std::string& path) override {
            if (!validatePath(path)) return false;
            
            // Emit save operation
            DocumentOperation op{DocumentOperation::Save, path, m_application->getCurrentDocument()};
            m_documentOperations.get_subscriber().on_next(op);
            
            m_lastErrorMessage = "Save operation queued";
            return true;
        }
        
    private:
        void setupDocumentPipeline() {
            m_documentOperations
                .get_observable()
                .observe_on(rxcpp::observe_on_event_loop())  // Background thread
                .subscribe([this](const DocumentOperation& op) {
                    try {
                        switch (op.type) {
                            case DocumentOperation::Save:
                                op.document->saveAs(std::filesystem::path(op.path));
                                // Emit thumbnail generation if needed
                                if (needsThumbnail()) {
                                    m_openCLOperations.get_subscriber().on_next(
                                        OpenCLOperation{OpenCLOperation::GenerateThumbnail, op.document}
                                    );
                                }
                                break;
                        }
                    } catch (const std::exception& e) {
                        // Handle error
                    }
                });
        }
        
        void setupOpenCLPipeline() {
            m_openCLOperations
                .get_observable()
                .observe_on(rxcpp::observe_on_computation())  // Compute thread pool
                .subscribe([this](const OpenCLOperation& op) {
                    // Handle OpenCL operations with proper resource management
                });
        }
    };
}
```

### Reactive Benefits
- **Composable**: Easy to chain and transform operations
- **Backpressure**: Built-in handling of overload
- **Error Handling**: Robust error propagation
- **Testing**: Easy to test with marble diagrams

### Reactive Drawbacks
- **Learning Curve**: Reactive programming paradigm shift
- **Dependencies**: Requires RxCpp library
- **Debugging**: Can be difficult to debug complex streams
- **Overhead**: Functional composition overhead

## Option 4: Hybrid Task + Coroutine Architecture

### Core Concept
Combine the best of both: Use coroutines for clean async syntax, but with a simple task dispatcher underneath.

```cpp
namespace gladius::hybrid {
    // Simple scheduler for coroutines
    class CoroutineScheduler {
    private:
        std::vector<std::thread> m_workers;
        ThreadSafeQueue<std::coroutine_handle<>> m_readyCoroutines;
        std::atomic<bool> m_running{true};
        
    public:
        CoroutineScheduler(size_t numWorkers = 4) {
            for (size_t i = 0; i < numWorkers; ++i) {
                m_workers.emplace_back([this] { workerLoop(); });
            }
        }
        
        void schedule(std::coroutine_handle<> coro) {
            m_readyCoroutines.push(coro);
        }
        
        void scheduleOnMainThread(std::coroutine_handle<> coro) {
            // Add to main thread queue (processed by UI thread)
            getMainThreadQueue().push(coro);
        }
        
    private:
        void workerLoop() {
            while (m_running) {
                auto coro = m_readyCoroutines.pop();
                if (coro && !coro->done()) {
                    coro->resume();
                }
            }
        }
    };
    
    // Resource awaiters that work with the scheduler
    class OpenCLAwaiter {
        // ... implementation that uses CoroutineScheduler
    };
    
    // High-level coroutine interface
    async::Task<bool> saveDocumentWithThumbnail(std::shared_ptr<Document> doc, const std::string& path) {
        // Acquire document write access
        auto docAccess = co_await acquireDocument(doc, true);
        
        // Save document (this happens on background thread)
        doc->saveAs(std::filesystem::path(path));
        
        // Generate thumbnail (this might switch to OpenCL thread)
        auto openclContext = co_await acquireOpenCL(getGlobalComputeContext());
        auto thumbnail = generateThumbnail(doc, openclContext);
        
        // Update UI (this switches to main thread)
        co_await switchToMainThread();
        updateUIWithThumbnail(thumbnail);
        
        co_return true;
    }
}
```

## Comparison Matrix

| Aspect | Task Dispatcher | C++20 Coroutines | Actor Model | Reactive Streams | Hybrid |
|--------|----------------|------------------|-------------|------------------|---------|
| **Implementation Complexity** | Medium | High | High | Medium | Medium-High |
| **Code Readability** | Good | Excellent | Good | Good | Excellent |
| **Performance** | Good | Excellent | Medium | Medium | Excellent |
| **Resource Management** | Manual | Automatic (RAII) | Manual | Manual | Automatic |
| **Error Handling** | Callbacks | Exceptions | Messages | Streams | Exceptions |
| **Debugging** | Easy | Medium | Hard | Medium | Medium |
| **C++ Version Required** | C++11 | C++20 | C++11 | C++11 | C++20 |
| **External Dependencies** | None | None | None | RxCpp | None |
| **Learning Curve** | Low | High | High | Medium | Medium |
| **Thread Safety** | Manual locks | Built-in | Built-in | Built-in | Built-in |
| **Composability** | Medium | Excellent | Good | Excellent | Excellent |
| **Migration Effort** | Low | Medium | High | Medium | Medium |

## Recommendation: Hybrid Approach

**Best Option: Hybrid Task + Coroutine Architecture**

### Why Hybrid is Optimal:

1. **Immediate Problem Solving**: Can fix the MCP crash with simple task dispatching first
2. **Future-Proof**: Add coroutines incrementally for better syntax
3. **Best of Both Worlds**: Task simplicity + coroutine elegance
4. **Gradual Migration**: Start with tasks, migrate to coroutines over time
5. **No External Dependencies**: Pure C++20 solution

### Implementation Strategy:

#### Phase 1: Simple Task Dispatcher (Immediate Fix)
```cpp
// Fix the crash quickly with basic task system
TaskDispatcher::getInstance().submitFunction([doc, path]() {
    doc->saveAs(path);  // Runs on background thread
});
```

#### Phase 2: Add Coroutine Layer
```cpp
// Add coroutine syntax for new code
Task<bool> saveDocument(Document& doc, const std::string& path) {
    auto access = co_await acquireDocumentAccess(doc);
    doc.saveAs(path);
    co_return true;
}
```

#### Phase 3: Full Integration
```cpp
// Eventually migrate everything to clean coroutine syntax
Task<void> completeWorkflow() {
    auto doc = co_await loadDocument("input.3mf");
    auto result = co_await processDocument(doc);
    co_await saveDocument(doc, "output.3mf");
    co_await updateUI(result);
}
```

### Why Not Pure Coroutines Initially?

1. **Risk Management**: The MCP crash needs immediate fixing
2. **Complexity**: Coroutines require careful implementation of awaiters and schedulers
3. **Testing**: Task dispatcher can be tested incrementally
4. **Compiler Support**: Not all target compilers may support C++20 fully

### Final Architecture:

```cpp
namespace gladius {
    // Start with this (Phase 1)
    class TaskDispatcher { /* ... simple implementation ... */ };
    
    // Add this layer (Phase 2)
    class CoroutineScheduler { /* ... works with TaskDispatcher ... */ };
    
    // Use this interface (Phase 3)
    template<typename T>
    using Task = std::coroutine<T>;  // Clean coroutine interface
}
```

This approach provides the immediate stability needed while building toward an elegant, modern C++ async architecture.
