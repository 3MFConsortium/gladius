# Gladius Concurrent Architecture Plan
## Addressing Resource Conflicts and Thread Safety

### Problem Analysis

The current architecture has several critical issues:

1. **Resource Access Conflicts**: MCP save operations trigger OpenCL program recompilation for thumbnail generation, causing access conflicts
2. **UI Thread Blocking**: Long-running computations block the main render loop and UI responsiveness  
3. **OpenCL Threading Restrictions**: OpenCL contexts have strict threading requirements and resource sharing limitations
4. **MCP Server Integration**: Need to support both GUI and headless modes without conflicts
5. **No Task Queuing**: No mechanism to dispatch and queue tasks to appropriate threads

### Current Architecture Problems

```cpp
// Current problematic flow:
MCP Tool Request → Direct Application Method → OpenCL Resource Access → UI Thread Blocked
                                           ↓
                               Thumbnail Generation → Program Recompilation → Resource Conflict
```

### Proposed Solution: Task Dispatcher Architecture

## 1. Core Components

### 1.1 TaskDispatcher - Central Coordination Hub

```cpp
namespace gladius {
    enum class TaskPriority {
        Low = 0,
        Normal = 1,
        High = 2,
        Critical = 3
    };

    enum class TaskType {
        DocumentOperation,    // Save, load, create, export
        ComputeOperation,     // OpenCL rendering, slicing
        UIUpdate,            // UI refresh, thumbnail generation
        FileIO,              // File system operations
        NetworkOperation     // MCP requests, file downloads
    };

    class Task {
    public:
        virtual ~Task() = default;
        virtual void execute() = 0;
        virtual TaskType getType() const = 0;
        virtual TaskPriority getPriority() const = 0;
        virtual std::string getDescription() const = 0;
        virtual bool requiresMainThread() const = 0;
        virtual bool requiresOpenCLContext() const = 0;
    };

    class TaskDispatcher {
    private:
        // Thread pools for different task types
        std::unique_ptr<ThreadPool> m_documentThreadPool;
        std::unique_ptr<ThreadPool> m_computeThreadPool;
        std::unique_ptr<ThreadPool> m_fileIOThreadPool;
        std::unique_ptr<ThreadPool> m_networkThreadPool;
        
        // Main thread task queue for UI operations
        ThreadSafeQueue<std::unique_ptr<Task>> m_mainThreadQueue;
        
        // Resource managers
        std::unique_ptr<OpenCLResourceManager> m_openCLManager;
        std::unique_ptr<DocumentResourceManager> m_documentManager;
        
        // Synchronization
        std::mutex m_dispatchMutex;
        std::condition_variable m_taskAvailable;
        
        // State management
        std::atomic<bool> m_running{true};
        std::atomic<bool> m_headlessMode{false};

    public:
        TaskDispatcher();
        ~TaskDispatcher();
        
        // Task submission
        std::future<void> submitTask(std::unique_ptr<Task> task);
        std::future<void> submitTaskWithCallback(std::unique_ptr<Task> task, 
                                                std::function<void()> callback);
        
        // Main thread processing (called from UI thread)
        void processMainThreadTasks();
        
        // Resource acquisition
        std::unique_ptr<OpenCLToken> acquireOpenCLContext(std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));
        std::unique_ptr<DocumentToken> acquireDocumentAccess(std::chrono::milliseconds timeout = std::chrono::milliseconds(1000));
        
        // Mode management
        void setHeadlessMode(bool headless);
        bool isHeadlessMode() const;
        
        // Shutdown
        void shutdown();
    };
}
```

### 1.2 Resource Managers - Prevent Conflicts

```cpp
namespace gladius {
    // OpenCL Resource Manager - Handles OpenCL context sharing
    class OpenCLResourceManager {
    private:
        SharedComputeContext m_context;
        std::mutex m_contextMutex;
        std::condition_variable m_contextAvailable;
        std::atomic<bool> m_contextInUse{false};
        std::thread::id m_currentOwner;
        
        // Per-thread command queues (OpenCL allows this)
        thread_local std::unique_ptr<cl::CommandQueue> m_threadQueue;
        
    public:
        class OpenCLToken {
            friend class OpenCLResourceManager;
        private:
            OpenCLResourceManager* m_manager;
            std::unique_lock<std::mutex> m_lock;
            
        public:
            OpenCLToken(OpenCLResourceManager* manager, std::unique_lock<std::mutex>&& lock)
                : m_manager(manager), m_lock(std::move(lock)) {}
            
            ~OpenCLToken() {
                if (m_manager) {
                    m_manager->releaseContext();
                }
            }
            
            ComputeContext& getContext() { return *m_manager->m_context; }
            cl::CommandQueue& getQueue() { return m_manager->getThreadQueue(); }
        };
        
        std::unique_ptr<OpenCLToken> acquireContext(std::chrono::milliseconds timeout);
        cl::CommandQueue& getThreadQueue();
        
    private:
        void releaseContext();
    };
    
    // Document Resource Manager - Handles document access
    class DocumentResourceManager {
    private:
        mutable std::shared_mutex m_documentMutex;
        std::atomic<int> m_readerCount{0};
        std::atomic<bool> m_writerActive{false};
        
    public:
        class DocumentToken {
            friend class DocumentResourceManager;
        private:
            DocumentResourceManager* m_manager;
            std::shared_lock<std::shared_mutex> m_readLock;
            std::unique_lock<std::shared_mutex> m_writeLock;
            bool m_isWriter;
            
        public:
            DocumentToken(DocumentResourceManager* manager, 
                         std::shared_lock<std::shared_mutex>&& readLock)
                : m_manager(manager), m_readLock(std::move(readLock)), m_isWriter(false) {}
                
            DocumentToken(DocumentResourceManager* manager, 
                         std::unique_lock<std::shared_mutex>&& writeLock)
                : m_manager(manager), m_writeLock(std::move(writeLock)), m_isWriter(true) {}
        };
        
        std::unique_ptr<DocumentToken> acquireReadAccess(std::chrono::milliseconds timeout);
        std::unique_ptr<DocumentToken> acquireWriteAccess(std::chrono::milliseconds timeout);
    };
}
```

### 1.3 Specific Task Implementations

```cpp
namespace gladius {
    // Document save task that doesn't block UI
    class SaveDocumentTask : public Task {
    private:
        std::string m_filePath;
        std::weak_ptr<Document> m_document;
        std::function<void(bool, std::string)> m_callback;
        
    public:
        SaveDocumentTask(const std::string& path, 
                        std::shared_ptr<Document> doc,
                        std::function<void(bool, std::string)> callback)
            : m_filePath(path), m_document(doc), m_callback(callback) {}
            
        void execute() override {
            auto doc = m_document.lock();
            if (!doc) {
                m_callback(false, "Document no longer available");
                return;
            }
            
            try {
                // This runs on background thread
                doc->saveAs(std::filesystem::path(m_filePath));
                m_callback(true, "Document saved successfully");
            } catch (const std::exception& e) {
                m_callback(false, std::string("Save failed: ") + e.what());
            }
        }
        
        TaskType getType() const override { return TaskType::DocumentOperation; }
        TaskPriority getPriority() const override { return TaskPriority::High; }
        std::string getDescription() const override { return "Save document to " + m_filePath; }
        bool requiresMainThread() const override { return false; }
        bool requiresOpenCLContext() const override { return false; }
    };
    
    // Thumbnail generation task (requires OpenCL but not UI thread)
    class GenerateThumbnailTask : public Task {
    private:
        std::weak_ptr<Document> m_document;
        std::function<void(std::shared_ptr<ImageBuffer>)> m_callback;
        
    public:
        void execute() override {
            auto doc = m_document.lock();
            if (!doc) return;
            
            // Acquire OpenCL context safely
            auto dispatcher = TaskDispatcher::getInstance();
            auto openclToken = dispatcher->acquireOpenCLContext();
            if (!openclToken) {
                m_callback(nullptr);
                return;
            }
            
            // Generate thumbnail using OpenCL
            auto thumbnail = generateThumbnailInternal(doc, openclToken->getContext());
            m_callback(thumbnail);
        }
        
        TaskType getType() const override { return TaskType::ComputeOperation; }
        TaskPriority getPriority() const override { return TaskPriority::Low; }
        bool requiresMainThread() const override { return false; }
        bool requiresOpenCLContext() const override { return true; }
    };
    
    // UI update task (must run on main thread)
    class UIUpdateTask : public Task {
    private:
        std::function<void()> m_updateFunction;
        
    public:
        explicit UIUpdateTask(std::function<void()> update) : m_updateFunction(update) {}
        
        void execute() override {
            m_updateFunction();
        }
        
        TaskType getType() const override { return TaskType::UIUpdate; }
        TaskPriority getPriority() const override { return TaskPriority::Normal; }
        bool requiresMainThread() const override { return true; }
        bool requiresOpenCLContext() const override { return false; }
    };
}
```

## 2. Integration with Existing Components

### 2.1 Modified Application Class

```cpp
namespace gladius {
    class Application {
    private:
        std::unique_ptr<TaskDispatcher> m_taskDispatcher;
        std::unique_ptr<ui::MainWindow> m_mainWindow;
        std::unique_ptr<mcp::MCPServer> m_mcpServer;
        std::unique_ptr<ApplicationMCPAdapter> m_mcpAdapter;
        bool m_headlessMode = false;
        
    public:
        Application() {
            m_taskDispatcher = std::make_unique<TaskDispatcher>();
        }
        
        void setHeadlessMode(bool headless) {
            m_headlessMode = headless;
            if (m_taskDispatcher) {
                m_taskDispatcher->setHeadlessMode(headless);
            }
        }
        
        TaskDispatcher& getTaskDispatcher() { return *m_taskDispatcher; }
        
        void run() {
            if (m_headlessMode) {
                runHeadless();
            } else {
                runWithGUI();
            }
        }
        
    private:
        void runHeadless() {
            // Headless mode - just process MCP requests and background tasks
            while (isRunning()) {
                // Let task dispatcher handle everything
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        
        void runWithGUI() {
            // GUI mode - process main thread tasks in render loop
            m_mainWindow = std::make_unique<ui::MainWindow>(/* ... */);
            
            while (!shouldClose()) {
                // Process main thread tasks (UI updates, immediate operations)
                m_taskDispatcher->processMainThreadTasks();
                
                // Render UI
                m_mainWindow->render();
                
                // Handle events
                glfwPollEvents();
            }
        }
    };
}
```

### 2.2 Modified ApplicationMCPAdapter

```cpp
namespace gladius {
    class ApplicationMCPAdapter : public MCPApplicationInterface {
    private:
        Application* m_application;
        TaskDispatcher* m_taskDispatcher;
        
    public:
        bool saveDocumentAs(const std::string& path) override {
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
            
            // Submit save task to background thread
            auto saveTask = std::make_unique<SaveDocumentTask>(
                path, 
                document,
                [this](bool success, const std::string& message) {
                    m_lastErrorMessage = message;
                    // Optionally trigger UI update if not in headless mode
                    if (!m_taskDispatcher->isHeadlessMode()) {
                        auto uiUpdate = std::make_unique<UIUpdateTask>([this]() {
                            // Update UI to reflect save status
                            // This runs on main thread
                        });
                        m_taskDispatcher->submitTask(std::move(uiUpdate));
                    }
                }
            );
            
            auto future = m_taskDispatcher->submitTask(std::move(saveTask));
            
            // For MCP, we return immediately - the operation happens asynchronously
            // The callback will update the error message when complete
            m_lastErrorMessage = "Save operation submitted";
            return true;  // Task submitted successfully
        }
        
        bool createFunctionFromExpression(const std::string& name,
                                        const std::string& expression,
                                        const std::string& outputType,
                                        const std::vector<FunctionArgument>& arguments,
                                        const std::string& outputName) override {
            // This can be synchronous as it doesn't require heavy computation
            // or conflicting resource access
            
            // Get document access token
            auto docToken = m_taskDispatcher->acquireDocumentAccess();
            if (!docToken) {
                m_lastErrorMessage = "Could not acquire document access";
                return false;
            }
            
            try {
                // Create function directly - this is fast and doesn't conflict
                auto document = m_application->getCurrentDocument();
                if (!document) {
                    m_lastErrorMessage = "No active document";
                    return false;
                }
                
                // Parse and create function
                ExpressionParser parser;
                auto parsedExpr = parser.parse(expression);
                
                ExpressionToGraphConverter converter;
                auto functionNode = converter.convert(parsedExpr, name, outputType, arguments, outputName);
                
                document->addFunction(functionNode);
                
                m_lastErrorMessage = "Function created successfully";
                return true;
            } catch (const std::exception& e) {
                m_lastErrorMessage = "Failed to create function: " + std::string(e.what());
                return false;
            }
        }
    };
}
```

## 3. OpenCL Context Management

### 3.1 Thread-Safe OpenCL Usage

```cpp
namespace gladius {
    class ThreadSafeComputeCore {
    private:
        SharedComputeContext m_context;
        TaskDispatcher* m_dispatcher;
        
        // Thread-local storage for per-thread resources
        thread_local std::unique_ptr<cl::CommandQueue> m_threadQueue;
        thread_local bool m_queueInitialized = false;
        
    public:
        void renderAsync(const RenderParameters& params, 
                        std::function<void(std::shared_ptr<ImageBuffer>)> callback) {
            
            auto renderTask = std::make_unique<RenderTask>(
                params,
                [this, callback](const RenderParameters& p) -> std::shared_ptr<ImageBuffer> {
                    // This runs on compute thread pool
                    auto openclToken = m_dispatcher->acquireOpenCLContext();
                    if (!openclToken) return nullptr;
                    
                    // Use thread-local queue for this computation
                    auto& queue = getThreadQueue(openclToken->getContext());
                    
                    // Perform rendering
                    return performRender(p, openclToken->getContext(), queue);
                },
                callback
            );
            
            m_dispatcher->submitTask(std::move(renderTask));
        }
        
    private:
        cl::CommandQueue& getThreadQueue(ComputeContext& context) {
            if (!m_queueInitialized) {
                m_threadQueue = std::make_unique<cl::CommandQueue>(context.createQueue());
                m_queueInitialized = true;
            }
            return *m_threadQueue;
        }
    };
}
```

## 4. Implementation Strategy

### Phase 1: Core Infrastructure (1-2 weeks)
1. **TaskDispatcher Implementation**
   - Basic task queue and thread pools
   - Resource managers for OpenCL and Document access
   - Main thread task processing integration

2. **Resource Conflict Resolution**
   - OpenCLResourceManager with per-thread queues
   - DocumentResourceManager with reader/writer locks
   - Token-based resource acquisition

### Phase 2: Task Refactoring (1-2 weeks)
1. **Document Operations**
   - SaveDocumentTask, LoadDocumentTask, ExportDocumentTask
   - Asynchronous execution with callbacks
   - Progress reporting for long operations

2. **Compute Operations**
   - RenderTask, SliceTask, ThumbnailTask
   - OpenCL context sharing between tasks
   - Background compilation without UI blocking

### Phase 3: MCP Integration (1 week)
1. **ApplicationMCPAdapter Updates**
   - Async task submission for heavy operations
   - Proper error handling and status reporting
   - Headless mode support

2. **UI Integration**
   - Main thread task processing in render loop
   - Progress indicators for background operations
   - Responsive UI during heavy computations

### Phase 4: Testing and Optimization (1 week)
1. **Integration Testing**
   - MCP tools under heavy load
   - Concurrent operation scenarios
   - Resource starvation testing

2. **Performance Optimization**
   - Thread pool tuning
   - Task priority optimization
   - Memory usage monitoring

## 5. Benefits

### 5.1 Immediate Benefits
- **No More UI Blocking**: Heavy operations run on background threads
- **Resource Conflict Resolution**: Proper synchronization prevents crashes
- **MCP Responsiveness**: MCP server can handle requests without blocking
- **Headless Mode Support**: Can run without GUI initialization

### 5.2 Long-term Benefits
- **Scalability**: Easy to add new task types and thread pools
- **Maintainability**: Clear separation of concerns
- **Testability**: Each component can be tested independently
- **Performance**: Better resource utilization and parallelization

## 6. Risks and Mitigation

### 6.1 Complexity Risk
- **Risk**: Increased codebase complexity
- **Mitigation**: Clear interfaces, extensive documentation, unit tests

### 6.2 Deadlock Risk
- **Risk**: Resource deadlocks between threads
- **Mitigation**: Timeout-based resource acquisition, deadlock detection

### 6.3 Race Condition Risk
- **Risk**: Data races in shared resources
- **Mitigation**: Proper synchronization primitives, thread-safe data structures

## 7. Architecture Alternatives Analysis

### 7.1 C++20 Coroutines (Recommended Future Direction)
**Advantages:**
- **Readable Async Code**: `co_await acquireOpenCL()` looks synchronous but is async
- **RAII Resource Management**: Perfect integration with C++ destructors
- **Zero-Cost Abstractions**: Minimal runtime overhead
- **Composable**: Easy to chain complex async operations

**Implementation Preview:**
```cpp
async::Task<bool> saveDocumentCoroutine(Document& doc, const std::string& path) {
    auto docAccess = co_await acquireDocument(doc, true);
    doc.saveAs(path);
    
    auto openclContext = co_await acquireOpenCL();
    auto thumbnail = generateThumbnail(doc, openclContext);
    
    co_await switchToMainThread();
    updateUIThumbnail(thumbnail);
    co_return true;
}
```

**Challenges:**
- C++20 requirement (compiler support)
- Complex implementation of awaiters and schedulers
- Debugging suspended coroutines

### 7.2 Actor Model
**Advantages:**
- **No Shared State**: Eliminates race conditions by design
- **Fault Isolation**: Component failures don't cascade
- **Clear Message Contracts**: Well-defined interfaces

**Challenges:**
- Complete architecture rewrite required
- Message serialization overhead
- Complex debugging across actors

### 7.3 Reactive Streams (RxCpp)
**Advantages:**
- **Composable Pipelines**: Chain operations with operators
- **Backpressure Handling**: Built-in overload protection
- **Error Propagation**: Robust error handling

**Challenges:**
- External dependency (RxCpp)
- Reactive programming learning curve
- Debugging complex stream chains

### 7.4 **RECOMMENDED: Hybrid Task + Coroutine Architecture**

**Strategy**: Start with simple TaskDispatcher, evolve to coroutines

#### Phase 1: Task Dispatcher (Immediate Fix)
```cpp
// Fix MCP crash immediately
TaskDispatcher::getInstance().submitFunction([doc, path]() {
    doc->saveAs(path);  // Background thread, no UI blocking
});
```

#### Phase 2: Add Coroutine Layer
```cpp
// Add coroutine syntax for new operations
async::Task<bool> saveWithThumbnail(Document& doc) {
    auto access = co_await acquireDocument(doc);
    doc.save();
    
    auto openclCtx = co_await acquireOpenCL();
    generateThumbnail(doc, openclCtx);
    co_return true;
}
```

#### Phase 3: Full Migration
```cpp
// Eventually migrate all async operations
async::Task<void> completeWorkflow() {
    auto doc = co_await loadDocument("input.3mf");
    auto result = co_await processDocument(doc);
    co_await saveDocument(doc, "output.3mf");
    co_await updateUI(result);
}
```

**Why Hybrid is Optimal:**
1. **Immediate Stability**: Fixes MCP crash with simple tasks
2. **Future-Proof**: Gradual migration to modern C++ coroutines
3. **Low Risk**: Incremental changes, not a complete rewrite
4. **Best Syntax**: Eventually achieve clean coroutine code
5. **No Dependencies**: Pure C++ solution

## 8. Implementation Recommendation

### Start with TaskDispatcher (This Document's Approach)
- ✅ **Immediate Problem Resolution**: Fix MCP save crashes
- ✅ **Low Risk**: Incremental integration with existing code
- ✅ **Proven Pattern**: Well-understood threading model
- ✅ **Testing**: Can validate each component independently

### Evolve to Hybrid Coroutine Architecture
- 🔄 **Phase 2 Addition**: Layer coroutines on top of task system
- 🔄 **Gradual Migration**: Convert operations one at a time
- 🔄 **Modern C++**: Eventually achieve clean async syntax
- 🔄 **Performance**: Zero-cost abstractions for async operations

## Conclusion

The proposed TaskDispatcher architecture provides an **immediate solution** to the critical MCP save crash while establishing the foundation for a **future coroutine-based architecture**. 

**Key Decision**: Start with the task-based approach detailed in this document because:
1. It solves the immediate crisis (MCP tool crashes)
2. It's implementable with current C++ standards
3. It provides a stable foundation for future coroutine integration
4. It allows incremental migration without architectural rewrites

The **long-term vision** is a hybrid system where:
- Heavy operations use clean coroutine syntax
- Resource management leverages RAII and co_await
- UI operations remain responsive through proper thread separation
- OpenCL operations are properly synchronized across threads

This approach provides both **immediate stability** and a **path to modern async C++**.
