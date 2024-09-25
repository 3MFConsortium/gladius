#pragma once

namespace gladius
{
    enum class CompilationState
    {
        UpToDate,
        CompilationRequested,
        CompilationInProgress
    };

    class ModelState
    {
    public:
        void signalCompilationRequired()
        {
            m_compilationState = CompilationState::CompilationRequested;
        }

        void signalCompilationFinished()
        {
            if (m_compilationState == CompilationState::CompilationInProgress)
            {
                m_compilationState = CompilationState::UpToDate;
            }
        }

        void signalCompilationStarted()
        {
            m_compilationState = CompilationState::CompilationInProgress;
        }

        [[nodiscard]] bool isCompilationRequired() const
        {
            if (m_compilationState == CompilationState::CompilationRequested)
            {
                return true;
            }

            return false;
        }

        [[nodiscard]] bool isModelUpToDate() const
        {
            return m_compilationState == CompilationState::UpToDate;
        }

    private:
        CompilationState m_compilationState{CompilationState::UpToDate};
    };
}
