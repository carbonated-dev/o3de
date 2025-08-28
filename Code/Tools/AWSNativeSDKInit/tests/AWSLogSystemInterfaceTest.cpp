/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzCore/Console/Console.h>
#include <AzCore/Console/IConsole.h>
#include <AzCore/Interface/Interface.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzCore/UnitTest/TestTypes.h>

#include <AWSNativeSDKInit/AWSLogSystemInterface.h>

#include <aws/core/utils/logging/LogLevel.h>

using namespace AWSNativeSDKInit;

class AWSLogSystemInterfaceTest
    : public UnitTest::LeakDetectionFixture
    , public AZ::Debug::TraceMessageBus::Handler
{
public:
    bool OnPreAssert(const char*, int, const char*, const char*) override
    {
        return true;
    }

    bool OnPreError(const char*, const char*, int, const char*, const char*) override
    {
        m_error = true;
        return true;
    }

    bool OnPreWarning(const char*, const char*, int, const char*, const char*) override
    {
        m_warning = true;
        return true;
    }

    bool OnPrintf(const char*, const char*) override
    {
        m_printf = true;
        return true;
    }

    void SetUp() override
    {
        BusConnect();
        if (!AZ::Interface<AZ::IConsole>::Get())
        {
            m_console = AZStd::make_unique<AZ::Console>();
            m_console->LinkDeferredFunctors(AZ::ConsoleFunctorBase::GetDeferredHead());
            AZ::Interface<AZ::IConsole>::Register(m_console.get());

#if defined(CARBONATED)
            // Console commands execution was delayed (within #if defined (CARBONATED) fence) until
            //   Console::EnableToDispatchConsoleCommands()
            // is called after ComponentApplication finishes loading all modules and registering all their commands,
            // in commit 0f6633b678d826beacb5f4c222556e97fe94e816 to Carbonated repo. This patch fixes Unit Test execution.
            m_console->EnableToDispatchConsoleCommands(); // Enable dispatching console commands.
#endif

        }
    }

    void TearDown() override
    {
        if (m_console)
        {
            AZ::Interface<AZ::IConsole>::Unregister(m_console.get());
            m_console.reset();
        }
        BusDisconnect();
    }

    bool m_error = false;
    bool m_warning = false;
    bool m_printf = false;

private:
    AZStd::unique_ptr<AZ::Console> m_console;
};

TEST_F(AWSLogSystemInterfaceTest, LogStream_LogFatalMessage_GetExpectedNotification)
{
    AWSLogSystemInterface logSystem(Aws::Utils::Logging::LogLevel::Trace);
    Aws::OStringStream testString;
    logSystem.LogStream(Aws::Utils::Logging::LogLevel::Fatal, "test", testString);
    ASSERT_TRUE(m_error);
    ASSERT_FALSE(m_warning);
    ASSERT_FALSE(m_printf);
}

TEST_F(AWSLogSystemInterfaceTest, LogStream_LogErrorMessage_GetExpectedNotification)
{
    AWSLogSystemInterface logSystem(Aws::Utils::Logging::LogLevel::Trace);
    Aws::OStringStream testString;
    logSystem.LogStream(Aws::Utils::Logging::LogLevel::Error, "test", testString);
    ASSERT_TRUE(m_error);
    ASSERT_FALSE(m_warning);
    ASSERT_FALSE(m_printf);
}

TEST_F(AWSLogSystemInterfaceTest, LogStream_LogWarningMessage_GetExpectedNotification)
{
    AWSLogSystemInterface logSystem(Aws::Utils::Logging::LogLevel::Trace);
    Aws::OStringStream testString;
    logSystem.LogStream(Aws::Utils::Logging::LogLevel::Warn, "test", testString);
    ASSERT_FALSE(m_error);
    ASSERT_TRUE(m_warning);
    ASSERT_FALSE(m_printf);
}

TEST_F(AWSLogSystemInterfaceTest, LogStream_LogInfoMessage_GetExpectedNotification)
{
    AWSLogSystemInterface logSystem(Aws::Utils::Logging::LogLevel::Trace);
    Aws::OStringStream testString;
    logSystem.LogStream(Aws::Utils::Logging::LogLevel::Info, "test", testString);
    ASSERT_FALSE(m_error);
    ASSERT_FALSE(m_warning);
    ASSERT_TRUE(m_printf);
}

TEST_F(AWSLogSystemInterfaceTest, LogStream_LogDebugMessage_GetExpectedNotification)
{
    AWSLogSystemInterface logSystem(Aws::Utils::Logging::LogLevel::Trace);
    Aws::OStringStream testString;
    logSystem.LogStream(Aws::Utils::Logging::LogLevel::Debug, "test", testString);
    ASSERT_FALSE(m_error);
    ASSERT_FALSE(m_warning);
    ASSERT_TRUE(m_printf);
}

TEST_F(AWSLogSystemInterfaceTest, LogStream_LogTraceMessage_GetExpectedNotification)
{
    AWSLogSystemInterface logSystem(Aws::Utils::Logging::LogLevel::Trace);
    Aws::OStringStream testString;
    logSystem.LogStream(Aws::Utils::Logging::LogLevel::Trace, "test", testString);
    ASSERT_FALSE(m_error);
    ASSERT_FALSE(m_warning);
    ASSERT_TRUE(m_printf);
}

TEST_F(AWSLogSystemInterfaceTest, LogStream_OverrideWarnAndLogInfoMessage_GetExpectedNotification)
{
    AWSLogSystemInterface logSystem(Aws::Utils::Logging::LogLevel::Trace);
    Aws::OStringStream testString;
    AZ::Interface<AZ::IConsole>::Get()->PerformCommand("bg_awsLogLevel 3");
    logSystem.LogStream(Aws::Utils::Logging::LogLevel::Info, "test", testString);
    ASSERT_FALSE(m_error);
    ASSERT_FALSE(m_warning);
    ASSERT_FALSE(m_printf);
}

TEST_F(AWSLogSystemInterfaceTest, LogStream_OverrideWarnAndLogeErrorMessage_GetExpectedNotification)
{
    AWSLogSystemInterface logSystem(Aws::Utils::Logging::LogLevel::Trace);
    Aws::OStringStream testString;
    AZ::Interface<AZ::IConsole>::Get()->PerformCommand("bg_awsLogLevel 3");
    logSystem.LogStream(Aws::Utils::Logging::LogLevel::Error, "test", testString);
    ASSERT_TRUE(m_error);
    ASSERT_FALSE(m_warning);
    ASSERT_FALSE(m_printf);
}

TEST_F(AWSLogSystemInterfaceTest, LogStream_OverrideOffAndLogInfoMessage_GetExpectedNotification)
{
    AWSLogSystemInterface logSystem(Aws::Utils::Logging::LogLevel::Trace);
    Aws::OStringStream testString;
    AZ::Interface<AZ::IConsole>::Get()->PerformCommand("bg_awsLogLevel 0");
    logSystem.LogStream(Aws::Utils::Logging::LogLevel::Info, "test", testString);
    ASSERT_FALSE(m_error);
    ASSERT_FALSE(m_warning);
    ASSERT_FALSE(m_printf);
}
