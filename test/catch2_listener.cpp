#include <catch2/listener.hpp>
#include <iostream>

using namespace Catch;

struct CoutTestListener : TestEventListenerBase {
    using TestEventListenerBase::TestEventListenerBase;

    void testCaseStarting(const TestCaseInfo& info) override {
        std::cout << "=== TEST CASE START: " << info.name << " ===" << std::endl;
    }

    void testCaseEnded(const TestCaseStats& stats) override {
        if (stats.testInfo)
            std::cout << "=== TEST CASE END: " << stats.testInfo->name << " ===" << std::endl;
    }
};

CATCH_REGISTER_LISTENER(CoutTestListener)
