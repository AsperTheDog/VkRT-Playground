#pragma once
#include <functional>
#include <vector>

class CallOnDestroy
{
public:
    CallOnDestroy() = default;

    explicit CallOnDestroy(const std::function<void()>& func)
    {
        defer(func);
    }

    ~CallOnDestroy()
    {
        for (std::function<void()>& func : m_DeferedFunctions)
        {
            func();
        }
    }

    void defer(const std::function<void()>& func)
    {
        m_DeferedFunctions.push_back(func);
    }

private:
    std::vector<std::function<void()>> m_DeferedFunctions;
};
