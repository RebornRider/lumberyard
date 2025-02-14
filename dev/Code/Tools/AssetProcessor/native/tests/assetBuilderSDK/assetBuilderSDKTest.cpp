/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/
#include "AssetBuilderSDKTest.h"
#include "native/tests/BaseAssetProcessorTest.h"
#include "native/unittests/UnitTestRunner.h"
#include <AssetBuilderSDK/AssetBuilderSDK.h>


namespace AssetProcessor
{
#if defined(ENABLE_LEGACY_PLATFORMFLAGS_SUPPORT)
    TEST_F(AssetBuilderSDKTest, GetEnabledPlatformsCountUnitTest)
    {
        AssetBuilderSDK::CreateJobsRequest createJobsRequest;
        ASSERT_EQ(createJobsRequest.GetEnabledPlatformsCount(), 0);

        createJobsRequest.m_enabledPlatforms = {
            { "pc", {}
            }
        };
        ASSERT_EQ(createJobsRequest.GetEnabledPlatformsCount(), 1);

        createJobsRequest.m_enabledPlatforms = {
            { "pc", {}
            }, { "es3", {}
            }
        };
        ASSERT_EQ(createJobsRequest.GetEnabledPlatformsCount(), 2);
    }

    TEST_F(AssetBuilderSDKTest, GetEnabledPlatformAtUnitTest)
    {
        UnitTestUtils::AssertAbsorber absorb;
        AssetBuilderSDK::CreateJobsRequest createJobsRequest;
        ASSERT_EQ(createJobsRequest.GetEnabledPlatformAt(0), AssetBuilderSDK::Platform_NONE);

        createJobsRequest.m_enabledPlatforms = {
            { "pc", { }
            }
        };
        ASSERT_EQ(createJobsRequest.GetEnabledPlatformAt(0), AssetBuilderSDK::Platform_PC);
        ASSERT_EQ(createJobsRequest.GetEnabledPlatformAt(1), AssetBuilderSDK::Platform_NONE);

        createJobsRequest.m_enabledPlatforms = {
            { "es3", {}
            }
        };
        ASSERT_EQ(createJobsRequest.GetEnabledPlatformAt(0), AssetBuilderSDK::Platform_ES3);
        ASSERT_EQ(createJobsRequest.GetEnabledPlatformAt(1), AssetBuilderSDK::Platform_NONE);

        createJobsRequest.m_enabledPlatforms = {
            { "pc", {}
            }, { "es3", {}
            }
        };
        ASSERT_EQ(createJobsRequest.GetEnabledPlatformAt(0), AssetBuilderSDK::Platform_PC);
        ASSERT_EQ(createJobsRequest.GetEnabledPlatformAt(1), AssetBuilderSDK::Platform_ES3);
        ASSERT_EQ(createJobsRequest.GetEnabledPlatformAt(2), AssetBuilderSDK::Platform_NONE);

        createJobsRequest.m_enabledPlatforms = {
            { "ios", {}
            }
        };
        ASSERT_EQ(createJobsRequest.GetEnabledPlatformAt(0), AssetBuilderSDK::Platform_IOS);
        ASSERT_EQ(createJobsRequest.GetEnabledPlatformAt(1), AssetBuilderSDK::Platform_NONE);

        createJobsRequest.m_enabledPlatforms = {
            { "pc", {}
            }, { "es3", {}
            }, { "ios", {}
            }, { "osx_gl", {}
            }
        };
        ASSERT_EQ(createJobsRequest.GetEnabledPlatformAt(0), AssetBuilderSDK::Platform_PC);
        ASSERT_EQ(createJobsRequest.GetEnabledPlatformAt(1), AssetBuilderSDK::Platform_ES3);
        ASSERT_EQ(createJobsRequest.GetEnabledPlatformAt(2), AssetBuilderSDK::Platform_IOS);
        ASSERT_EQ(createJobsRequest.GetEnabledPlatformAt(3), AssetBuilderSDK::Platform_OSX);
        ASSERT_EQ(createJobsRequest.GetEnabledPlatformAt(4), AssetBuilderSDK::Platform_NONE);

        createJobsRequest.m_enabledPlatforms = {
            { "xenia", {}
            }
        };
        ASSERT_EQ(createJobsRequest.GetEnabledPlatformAt(0), AssetBuilderSDK::Platform_XENIA);
        ASSERT_EQ(createJobsRequest.GetEnabledPlatformAt(1), AssetBuilderSDK::Platform_NONE);

        createJobsRequest.m_enabledPlatforms = {
            { "pc", {}
            }, { "es3", {}
            }
        };
        ASSERT_EQ(createJobsRequest.GetEnabledPlatformAt(0), AssetBuilderSDK::Platform_PC);
        ASSERT_EQ(createJobsRequest.GetEnabledPlatformAt(1), AssetBuilderSDK::Platform_ES3);
        ASSERT_EQ(createJobsRequest.GetEnabledPlatformAt(2), AssetBuilderSDK::Platform_NONE);

        // using a deprecated API should have generated warnings.
        ASSERT_GT(absorb.m_numWarningsAbsorbed, 0);
    }

    TEST_F(AssetBuilderSDKTest, IsPlatformEnabledUnitTest)
    {
        UnitTestUtils::AssertAbsorber absorb;
        AssetBuilderSDK::CreateJobsRequest createJobsRequest;
        ASSERT_FALSE(createJobsRequest.IsPlatformEnabled(AssetBuilderSDK::Platform_PC));

        createJobsRequest.m_enabledPlatforms = {
            { "pc", {}
            }
        };
        ASSERT_TRUE(createJobsRequest.IsPlatformEnabled(AssetBuilderSDK::Platform_PC));
        ASSERT_FALSE(createJobsRequest.IsPlatformEnabled(AssetBuilderSDK::Platform_ES3));

        createJobsRequest.m_enabledPlatforms = {
            { "pc", {}
            }, { "es3", {}
            }
        };
        ASSERT_TRUE(createJobsRequest.IsPlatformEnabled(AssetBuilderSDK::Platform_PC));
        ASSERT_TRUE(createJobsRequest.IsPlatformEnabled(AssetBuilderSDK::Platform_ES3));

        createJobsRequest.m_enabledPlatforms = {
            { "pc", {}
            }, { "es3", {}
            }
        };
        ASSERT_TRUE(createJobsRequest.IsPlatformEnabled(AssetBuilderSDK::Platform_PC));
        ASSERT_TRUE(createJobsRequest.IsPlatformEnabled(AssetBuilderSDK::Platform_ES3));

        // using a deprecated API should have generated warnings.
        ASSERT_GT(absorb.m_numWarningsAbsorbed, 0);
    }

    TEST_F(AssetBuilderSDKTest, IsPlatformValidUnitTest)
    {
        AssetBuilderSDK::CreateJobsRequest createJobsRequest;
        UnitTestUtils::AssertAbsorber absorb;

        ASSERT_TRUE(createJobsRequest.IsPlatformValid(AssetBuilderSDK::Platform_PC));
        ASSERT_TRUE(createJobsRequest.IsPlatformValid(AssetBuilderSDK::Platform_ES3));
        ASSERT_TRUE(createJobsRequest.IsPlatformValid(AssetBuilderSDK::Platform_IOS));
        ASSERT_TRUE(createJobsRequest.IsPlatformValid(AssetBuilderSDK::Platform_OSX));
        ASSERT_TRUE(createJobsRequest.IsPlatformValid(AssetBuilderSDK::Platform_XENIA));
        ASSERT_TRUE(createJobsRequest.IsPlatformValid(AssetBuilderSDK::Platform_PROVO));
        ASSERT_TRUE(createJobsRequest.IsPlatformValid(AssetBuilderSDK::Platform_SALEM));
        //64 is 0x040 which currently is the next valid platform value which is invalid as of now, if we ever add a new platform entry to the Platform enum
        //we will have to update this failure unit test
        ASSERT_FALSE(createJobsRequest.IsPlatformValid(static_cast<AssetBuilderSDK::Platform>(128)));
        // using a deprecated API should have generated warnings.
        ASSERT_GT(absorb.m_numWarningsAbsorbed, 0);
    }
#endif // defined(ENABLE_LEGACY_PLATFORMFLAGS_SUPPORT)
};
