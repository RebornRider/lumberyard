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

#include <AzTest/AzTest.h>
#include <AzCore/UnitTest/UnitTest.h>
#include <AzFramework/IO/LocalFileIO.h>
#include <AzFramework/Asset/XmlSchemaAsset.h>
#include <AzFramework/StringFunc/StringFunc.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Component/ComponentApplication.h>
#include <AzCore/UnitTest/UnitTest.h>

#include <LyShine/UiAssetTypes.h>

#include <Source/AudioControlBuilderWorker/AudioControlBuilderWorker.h>
#include <Source/CfgBuilderWorker/CfgBuilderWorker.h>
#include <Source/FontBuilderWorker/FontBuilderWorker.h>
#include <Source/ParticlePreloadLibsBuilderWorker/ParticlePreloadLibsBuilderWorker.h>
#include <Source/SchemaBuilderWorker/SchemaBuilderWorker.h>
#include <Source/XmlBuilderWorker/XmlBuilderWorker.h>

using namespace CopyDependencyBuilder;
using namespace AZ;
using namespace AssetBuilderSDK;

class CopyDependencyBuilderTest
    : public ::testing::Test
    , public UnitTest::TraceBusRedirector
    , private AzToolsFramework::AssetSystemRequestBus::Handler
{
protected:
    void SetUp() override
    {
        AZ::AllocatorInstance<AZ::SystemAllocator>::Create();

        m_app.reset(aznew AZ::ComponentApplication());
        AZ::ComponentApplication::Descriptor desc;
        desc.m_useExistingAllocator = true;
        m_app->Create(desc);

        if (AZ::g_currentPlatform == AZ::PlatformID::PLATFORM_WINDOWS_32 
            || AZ::g_currentPlatform == AZ::PlatformID::PLATFORM_WINDOWS_64)
        {
            m_currentPlatform = "pc";
        }
        else
        {
            m_currentPlatform = AZ::GetPlatformName(AZ::g_currentPlatform);
            AZStd::to_lower(m_currentPlatform.begin(), m_currentPlatform.end());
        }

        // Startup default local FileIO (hits OSAllocator) if not already setup.
        if (AZ::IO::FileIOBase::GetInstance() == nullptr)
        {
            AZ::IO::FileIOBase::SetInstance(aznew AZ::IO::LocalFileIO());
        }

        const char* dir = m_app->GetExecutableFolder();
        AZ::IO::FileIOBase::GetInstance()->SetAlias("@root@", dir);

        SerializeContext* serializeContext;
        ComponentApplicationBus::BroadcastResult(serializeContext, &ComponentApplicationRequests::GetSerializeContext);
        ASSERT_TRUE(serializeContext);

        AzFramework::VersionSearchRule::Reflect(serializeContext);
        AzFramework::XmlSchemaAttribute::Reflect(serializeContext);
        AzFramework::XmlSchemaElement::Reflect(serializeContext);
        AzFramework::MatchingRule::Reflect(serializeContext);
        AzFramework::SearchRuleDefinition::Reflect(serializeContext);
        AzFramework::DependencySearchRule::Reflect(serializeContext);
        AzFramework::XmlSchemaAsset::Reflect(serializeContext);

        AZ::Debug::TraceMessageBus::Handler::BusConnect();

        AzToolsFramework::AssetSystemRequestBus::Handler::BusConnect();
    }

    void TearDown() override
    {
        AzToolsFramework::AssetSystemRequestBus::Handler::BusDisconnect();

        AZ::Debug::TraceMessageBus::Handler::BusDisconnect();

        delete AZ::IO::FileIOBase::GetInstance();
        AZ::IO::FileIOBase::SetInstance(nullptr);

        m_app->Destroy();
        m_app = nullptr;

        AZ::AllocatorInstance<AZ::SystemAllocator>::Destroy();
    }

    AZStd::string GetFullPath(AZStd::string_view fileName)
    {
        constexpr char testFileFolder[] = "@root@/../Code/Tools/AssetProcessor/Builders/CopyDependencyBuilder/Tests/";
        return AZStd::string::format("%s%.*s", testFileFolder, fileName.size(), fileName.data());
    }

    void TestFailureCase(CopyDependencyBuilderWorker* worker, AZStd::string_view fileName, bool expectedResult = false)
    {
        AssetBuilderSDK::ProductPathDependencySet resolvedPaths;
        AZStd::vector<AssetBuilderSDK::ProductDependency> productDependencies;
        
        AssetBuilderSDK::ProcessJobRequest request;
        request.m_fullPath = GetFullPath(fileName);
        request.m_sourceFile = fileName;
        request.m_platformInfo.m_identifier = m_currentPlatform;

        bool result = worker->ParseProductDependencies(request, productDependencies, resolvedPaths);
        ASSERT_EQ(result, expectedResult);
        ASSERT_EQ(resolvedPaths.size(), 0);
        ASSERT_EQ(productDependencies.size(), 0);
    }

    void TestSuccessCase(
        CopyDependencyBuilderWorker* worker,
        AZStd::string_view fileName,
        AZStd::vector<const char*>& expectedPathDependencies = AZStd::vector<const char*>(),
        AZStd::vector<AssetBuilderSDK::ProductDependency>& expectedProductDependencies = AZStd::vector<AssetBuilderSDK::ProductDependency>())
    {
        AssetBuilderSDK::ProductPathDependencySet resolvedPaths;
        AZStd::vector<AssetBuilderSDK::ProductDependency> productDependencies;
        size_t referencedFilePathsCount = expectedPathDependencies.size();
        size_t referencedProductDependenciesCount = expectedProductDependencies.size();

        AssetBuilderSDK::ProductPathDependencySet expectedResolvedPaths;
        for (const char* path : expectedPathDependencies)
        {
            expectedResolvedPaths.emplace(path, AssetBuilderSDK::ProductPathDependencyType::ProductFile);
        }

        AssetBuilderSDK::ProcessJobRequest request;
        request.m_fullPath = GetFullPath(fileName);
        request.m_sourceFile = fileName;
        request.m_platformInfo.m_identifier = m_currentPlatform;

        bool result = worker->ParseProductDependencies(request, productDependencies, resolvedPaths);
        ASSERT_TRUE(result);
        ASSERT_EQ(resolvedPaths.size(), referencedFilePathsCount);
        ASSERT_EQ(productDependencies.size(), referencedProductDependenciesCount);
        if (referencedFilePathsCount > 0)
        {
            for (const AssetBuilderSDK::ProductPathDependency& dependency : expectedResolvedPaths)
            {
                ASSERT_TRUE(resolvedPaths.find(dependency) != resolvedPaths.end()) << "Expected path dependency is not found in the process result";
            }
        }
        if (referencedProductDependenciesCount > 0)
        {
            for (const AssetBuilderSDK::ProductDependency& dependency : productDependencies)
            {
                bool expectedDependencyExists = false;
                for (const AssetBuilderSDK::ProductDependency& expectedProductDependency : expectedProductDependencies)
                {
                    if (expectedProductDependency.m_dependencyId == dependency.m_dependencyId
                        && expectedProductDependency.m_flags == dependency.m_flags)
                    {
                        expectedDependencyExists = true;
                        break;
                    }
                }

                ASSERT_TRUE(expectedDependencyExists) << "Expected product dependency is not found in the process result";
            }
        }
    }

    void TestSuccessCase(CopyDependencyBuilderWorker* worker, AZStd::string_view fileName, const char* expectedFile)
    {
        AZStd::vector<const char*> expectedFiles;
        expectedFiles.push_back(expectedFile);
        TestSuccessCase(worker, fileName, expectedFiles);
    }

    void TestSuccessCaseNoDependencies(CopyDependencyBuilderWorker* worker, AZStd::string_view fileName)
    {
        AZStd::vector<const char*> expectedFiles;
        TestSuccessCase(worker, fileName, expectedFiles);
    }

    //////////////////////////////////////////////////////////////////////////
    // AzToolsFramework::AssetSystem::AssetSystemRequestBus::Handler overrides
    const char* GetAbsoluteDevGameFolderPath() override { return ""; }
    const char* GetAbsoluteDevRootFolderPath() override { return ""; }
    bool GetRelativeProductPathFromFullSourceOrProductPath(const AZStd::string& fullPath, AZStd::string& relativeProductPath) { return true; }
    bool GetFullSourcePathFromRelativeProductPath(const AZStd::string& relPath, AZStd::string& fullSourcePath) { return true; }
    bool GetAssetInfoById(const AZ::Data::AssetId& assetId, const AZ::Data::AssetType& assetType, AZ::Data::AssetInfo& assetInfo, AZStd::string& rootFilePath) { return true; }
    bool GetSourceInfoBySourcePath(const char* sourcePath, AZ::Data::AssetInfo& assetInfo, AZStd::string& watchFolder) { return true; }
    bool GetSourceInfoBySourceUUID(const AZ::Uuid& sourceUuid, AZ::Data::AssetInfo& assetInfo, AZStd::string& watchFolder) { return true; }
    bool GetScanFolders(AZStd::vector<AZStd::string>& scanFolders) { return true; }
    bool IsAssetPlatformEnabled(const char* platform) { return true; }
    int GetPendingAssetsForPlatform(const char* platform) { return 0; }
    bool GetAssetsProducedBySourceUUID(const AZ::Uuid& sourceUuid, AZStd::vector<AZ::Data::AssetInfo>& productsAssetInfo) { return true; }
    bool GetAssetSafeFolders(AZStd::vector<AZStd::string>& assetSafeFolders) override
    {
        char resolvedBuffer[AZ_MAX_PATH_LEN] = { 0 };
        AZ::IO::FileIOBase::GetInstance()->ResolvePath(GetFullPath("Xmls").c_str(), resolvedBuffer, AZ_MAX_PATH_LEN);
        assetSafeFolders.emplace_back(resolvedBuffer);
        return true;
    }

    // When supressing AZ_Errors to count how many occur,
    // you need to tell it you expect double the number of errors.
    const int SuppressedErrorMultiplier = 2;

    AZStd::unique_ptr<AZ::ComponentApplication> m_app;
    AZStd::string m_currentPlatform;
};

TEST_F(CopyDependencyBuilderTest, TestCfgBuilderWorker_EmptyCfg_NoDependenciesNoErrors)
{
    ProductPathDependencySet resolvedPaths;
    bool result = CfgBuilderWorker::ParseProductDependenciesFromCfgContents(
        "arbitraryFileName",
        "",
        resolvedPaths);

    ASSERT_TRUE(result);
    ASSERT_EQ(resolvedPaths.size(), 0);
}

struct CfgTestHelper
{
    AZStd::string m_command;
    ProductPathDependency m_expectedDependency;
};

AZStd::string ConstructCfgFromHelpers(const AZStd::vector<CfgTestHelper>& helpers)
{
    AZStd::string result;
    for (const CfgTestHelper& helper : helpers)
    {
        result += AZStd::string::format("%s=%s\n", helper.m_command.c_str(), helper.m_expectedDependency.m_dependencyPath.c_str());
    }
    return result;
}

TEST_F(CopyDependencyBuilderTest, TestXmlBuilderWorker_VegetationDescriptorProduct_AssetTypeValid)
{
    const char vegDescriptorTestName[] = "somefile.vegdescriptorlist";
    AZ::Data::AssetType vegDescriptorListType("{60961B36-E3CA-4877-B197-1462C1363F6E}"); // DescriptorListAsset in Vegetation Gem

    XmlBuilderWorker testBuilder;

    AZ::Data::AssetType parsedAssetType = testBuilder.GetAssetType(vegDescriptorTestName);
    ASSERT_EQ(parsedAssetType, vegDescriptorListType);
}

TEST_F(CopyDependencyBuilderTest, TestXmlBuilderWorker_InvalidExtensionProduct_AssetTypeNull)
{
    const char nullTestType[] = "somefile.vegdescriptorlist2";
    AZ::Data::AssetType nullType = AZ::Data::AssetType::CreateNull();

    XmlBuilderWorker testBuilder;

    AZ::Data::AssetType parsedAssetType = testBuilder.GetAssetType(nullTestType);
    ASSERT_EQ(parsedAssetType, nullType);
}

TEST_F(CopyDependencyBuilderTest, TestCfgBuilderWorker_ValidCommands_CorrectDependencies)
{
    // Testing every row of the supported config files and supported extensions won't accomplish
    // much besides forcing someone to keep two lists in sync. If these test cases work (source extension, product extensions), then the system works.
    AZStd::vector<CfgTestHelper> commands =
    {
        { "game_load_screen_uicanvas_path", { "somefile.uicanvas", ProductPathDependencyType::ProductFile }},
        { "sys_splashscreen", { "arbitraryFile.bmp", ProductPathDependencyType::SourceFile } },
    };

    ProductPathDependencySet resolvedPaths;
    bool result = CfgBuilderWorker::ParseProductDependenciesFromCfgContents(
        "arbitraryFileName",
        ConstructCfgFromHelpers(commands),
        resolvedPaths);

    ASSERT_TRUE(result);
    ASSERT_EQ(resolvedPaths.size(), commands.size());
    for (CfgTestHelper& helper : commands)
    {
        // Paths are stored in lowercase in the database
        AZStd::to_lower(helper.m_expectedDependency.m_dependencyPath.begin(), helper.m_expectedDependency.m_dependencyPath.end());
        ProductPathDependencySet::iterator foundFile = resolvedPaths.find(helper.m_expectedDependency);
        ASSERT_NE(foundFile, resolvedPaths.end());
    }
}

TEST_F(CopyDependencyBuilderTest, TestCfgBuilderWorker_CommentedCommand_NoDependenciesNoError)
{
    const AZStd::vector<CfgTestHelper> commands =
    {
        // Test product file types
        { "--game_load_screen_uicanvas_path", { "somefile.uicanvas", ProductPathDependencyType::ProductFile }},
        { "--sys_splashscreen", { "arbitraryFile.bmp", ProductPathDependencyType::SourceFile } },
    };

    ProductPathDependencySet resolvedPaths;
    bool result = CfgBuilderWorker::ParseProductDependenciesFromCfgContents(
        "arbitraryFileName",
        ConstructCfgFromHelpers(commands),
        resolvedPaths);

    ASSERT_TRUE(result);
    // Both commands were commented out, so there should be no resolved paths.
    ASSERT_EQ(resolvedPaths.size(), 0);
}

TEST_F(CopyDependencyBuilderTest, TestCfgBuilderWorker_ValidCommandInvalidValue_NoDependenciesError)
{
    const AZStd::vector<CfgTestHelper> commands =
    {
        { "game_load_screen_uicanvas_path", { "Invalid string with illegal characters!", ProductPathDependencyType::ProductFile }}
    };

    ProductPathDependencySet resolvedPaths;
    AZ_TEST_START_ASSERTTEST;
    bool result = CfgBuilderWorker::ParseProductDependenciesFromCfgContents(
        "arbitraryFileName",
        ConstructCfgFromHelpers(commands),
        resolvedPaths);
    // Expected: 1 error, on the illegal characters in the command's value.
    AZ_TEST_STOP_ASSERTTEST(1 * SuppressedErrorMultiplier);

    ASSERT_FALSE(result);
    ASSERT_EQ(resolvedPaths.size(), 0);
}

TEST_F(CopyDependencyBuilderTest, TestCfgBuilderWorker_ValidCommandEmptyValue_NoDependenciesError)
{
    const AZStd::vector<CfgTestHelper> commands =
    {
        { "game_load_screen_uicanvas_path", { "", ProductPathDependencyType::ProductFile }}
    };

    ProductPathDependencySet resolvedPaths;
    AZ_TEST_START_ASSERTTEST;
    bool result = CfgBuilderWorker::ParseProductDependenciesFromCfgContents(
        "arbitraryFileName",
        ConstructCfgFromHelpers(commands),
        resolvedPaths);
    // Expected: 1 error, on the empty value.
    AZ_TEST_STOP_ASSERTTEST(1 * SuppressedErrorMultiplier);

    ASSERT_FALSE(result);
    ASSERT_EQ(resolvedPaths.size(), 0);
}

TEST_F(CopyDependencyBuilderTest, TestCfgBuilderWorker_UnhandledCommandValidData_NoDependenciesNoError)
{
    const AZStd::vector<CfgTestHelper> commands =
    {
        { "command_that_does_not_exist", { "thislookslikea.file", ProductPathDependencyType::ProductFile }}
    };

    ProductPathDependencySet resolvedPaths;
    bool result = CfgBuilderWorker::ParseProductDependenciesFromCfgContents(
        "arbitraryFileName",
        ConstructCfgFromHelpers(commands),
        resolvedPaths);

    ASSERT_TRUE(result);
    ASSERT_EQ(resolvedPaths.size(), 0);
}

TEST_F(CopyDependencyBuilderTest, TestCfgBuilderWorker_ValidCommandsInvalidExtension_ErrorNoDependencies)
{
    // Testing every row of the supported config files and supported extensions won't accomplish
    // much besides forcing someone to keep two lists in sync. If these test cases work (source extension, product extensions), then the system works.
    AZStd::vector<CfgTestHelper> commands =
    {
        { "game_load_screen_uicanvas_path", { "somefile.badextension", ProductPathDependencyType::ProductFile }}
    };

    ProductPathDependencySet resolvedPaths;
    AZ_TEST_START_ASSERTTEST;
    bool result = CfgBuilderWorker::ParseProductDependenciesFromCfgContents(
        "arbitraryFileName",
        ConstructCfgFromHelpers(commands),
        resolvedPaths);
    // Expected: 1 error, on the invalid extension.
    AZ_TEST_STOP_ASSERTTEST(1 * SuppressedErrorMultiplier);

    ASSERT_FALSE(result);
    ASSERT_EQ(resolvedPaths.size(), 0);
}

TEST_F(CopyDependencyBuilderTest, TestFontfamilyAsset_MultipleDependencies_OutputProductDependencies)
{
    // Tests processing a FontFamilyExample.fontfamily file containing multiple dependencies
    // Should output 4 dependencies
    AZStd::vector<const char*> expectedPaths = {
        "Fonts/fontexample-regular.font",
        "Fonts/fontexample-bold.font",
        "Fonts/fontexample-italic.font",
        "Fonts/fontexample-bolditalic.font"
    };

    AZStd::string fileName = "Fonts/FontFamilyExample.fontfamily"; 

    FontBuilderWorker builderWorker;
    TestSuccessCase(&builderWorker, fileName, expectedPaths);
}

TEST_F(CopyDependencyBuilderTest, TestFontAsset_SingleDependency_OutputProductDependency)
{
    // Tests processing a FontExample.font file containing 1 dependency
    // Should output 1 dependency

    AZStd::string fileName = "Fonts/FontExample.font";

    FontBuilderWorker builderWorker;
    TestSuccessCase(&builderWorker, fileName, "Fonts/FontExample.ttf");
}

TEST_F(CopyDependencyBuilderTest, TestFontAsset_NoDependency_OutputNoProductDependencies)
{
    // Tests processing a FontExampleNoDependency.font file containing 0 dependency
    // Should output 0 dependencies and return true

    AZStd::string fileName = "Fonts/FontExampleNoDependency.font";

    FontBuilderWorker builderWorker;
    TestSuccessCaseNoDependencies(&builderWorker, fileName);
}

TEST_F(CopyDependencyBuilderTest, TestFontAsset_InvalidFilePath_OutputNoProductDependencies)
{
    // Tests passing an invalid file path in
    // Should output 0 dependency and return false

    AZStd::string fileName = "Fonts/InvalidPathExample.font";

    FontBuilderWorker builderWorker;
    TestFailureCase(&builderWorker, fileName);
}

TEST_F(CopyDependencyBuilderTest, TestFontAsset_EmptyFile_OutputNoProductDependencies)
{
    // Tests passing an empty file in
    // Should output 0 dependency and return false

    AZStd::string fileName = "Fonts/EmptyFontExample.font";

    FontBuilderWorker builderWorker;
    TestFailureCase(&builderWorker, fileName);
}

TEST_F(CopyDependencyBuilderTest, TestAudioControl_EmptyFile_NoProductDependencies)
{
    // Tests passing an empty file in
    // Should output 0 dependency and return false
    AZStd::string fileName = "AudioControls/EmptyControl.xml";
    AudioControlBuilderWorker builderWorker;
    TestFailureCase(&builderWorker, fileName);
}

TEST_F(CopyDependencyBuilderTest, TestAudioControl_NoPreloadsDefined_NoProductDependencies)
{
    AZStd::string fileName = "AudioControls/TestControlNoPreloads.xml";
    AudioControlBuilderWorker builderWorker;
    TestSuccessCaseNoDependencies(&builderWorker, fileName);
}

TEST_F(CopyDependencyBuilderTest, TestAudioControl_MissingConfigGroupNameAttribute_NoProductDependencies)
{
    AZStd::string fileName = "AudioControls/TestControlMissingConfigGroupNameAttribute.xml";
    AudioControlBuilderWorker builderWorker;
    TestSuccessCaseNoDependencies(&builderWorker, fileName);
}

TEST_F(CopyDependencyBuilderTest, TestAudioControl_MissingPlatformNameAttribute_NoProductDependencies)
{
    AZStd::string fileName = "AudioControls/TestControlMissingPlatformNameAttributeOnePreload.xml";
    AudioControlBuilderWorker builderWorker;
    TestSuccessCaseNoDependencies(&builderWorker, fileName);
}

TEST_F(CopyDependencyBuilderTest, TestAudioControl_MissingAtlPlatformsNode_NoProductDependencies)
{
    AZStd::string fileName = "AudioControls/TestControlMissingAtlPlatformsNode.xml";
    AudioControlBuilderWorker builderWorker;
    TestSuccessCaseNoDependencies(&builderWorker, fileName);
}

TEST_F(CopyDependencyBuilderTest, TestAudioControl_MissingPlatformNode_NoProductDependencies)
{
    AZStd::string fileName = "AudioControls/TestControlMissingPlatformNode.xml";
    AudioControlBuilderWorker builderWorker;
    TestSuccessCaseNoDependencies(&builderWorker, fileName);
}

TEST_F(CopyDependencyBuilderTest, TestAudioControl_MissingWwiseFileNode_NoProductDependencies)
{
    AZStd::string fileName = "AudioControls/TestControlMissingWwiseFileNode.xml";
    AudioControlBuilderWorker builderWorker;
    TestSuccessCaseNoDependencies(&builderWorker, fileName);
}

TEST_F(CopyDependencyBuilderTest, TestAudioControl_OnePreloadOneBank_OneProductDependency)
{
    AZStd::string fileName = "AudioControls/TestControlOnePreloadOneBank.xml";
    AudioControlBuilderWorker builderWorker;
    TestSuccessCase(&builderWorker, fileName, "sounds/wwise/test_bank1.bnk");
}

TEST_F(CopyDependencyBuilderTest, TestAudioControl_OnePreloadMultipleBanks_MultipleProductDependencies)
{
    AZStd::vector<const char*> expectedPaths = {
        "sounds/wwise/test_bank1.bnk",
        "sounds/wwise/test_bank2.bnk"
    };
    AZStd::string fileName = "AudioControls/TestControlOnePreloadMultipleBanks.xml";
    AudioControlBuilderWorker builderWorker;
    TestSuccessCase(&builderWorker, fileName, expectedPaths);
}

TEST_F(CopyDependencyBuilderTest, TestAudioControl_MultiplePreloadsOneBankEach_MultipleProductDependencies)
{
    AZStd::vector<const char*> expectedPaths = {
        "sounds/wwise/test_bank1.bnk",
        "sounds/wwise/test_bank2.bnk"
    };
    AZStd::string fileName = "AudioControls/TestControlMultiplePreloadOneBank.xml";
    AudioControlBuilderWorker builderWorker;
    TestSuccessCase(&builderWorker, fileName, expectedPaths);
}

TEST_F(CopyDependencyBuilderTest, TestAudioControl_MultiplePreloadsMultipleBanksEach_MultipleProductDependencies)
{
    AZStd::vector<const char*> expectedPaths = {
        "sounds/wwise/test_bank1.bnk",
        "sounds/wwise/test_bank2.bnk",
        "sounds/wwise/test_bank3.bnk",
        "sounds/wwise/test_bank4.bnk"
    };
    AZStd::string fileName = "AudioControls/TestControlMultiplePreloadsMultipleBanks.xml";
    AudioControlBuilderWorker builderWorker;
    TestSuccessCase(&builderWorker, fileName, expectedPaths);
}

TEST_F(CopyDependencyBuilderTest, TestAudioControl_NoConfigGroups_NoProductDependencies)
{
    AZStd::string fileName = "AudioControls/TestControlNoConfigGroups.xml";
    AudioControlBuilderWorker builderWorker;
    TestSuccessCaseNoDependencies(&builderWorker, fileName);
}

TEST_F(CopyDependencyBuilderTest, TestAudioControl_WrongConfigGroup_NoProductDependencies)
{
    AZStd::string fileName = "AudioControls/TestControlWrongConfigGroup.xml";
    AudioControlBuilderWorker builderWorker;
    TestSuccessCaseNoDependencies(&builderWorker, fileName);
}

// Verifies that preload files can depend on other preload files.
TEST_F(CopyDependencyBuilderTest, TestParticlePreloadLib_SinglePreloadDependency_OutputProductDependency)
{
    AZStd::string fileName = "Libs/Particles/PreloadWithPreloadRef.txt";

    ParticlePreloadLibsBuilderWorker builderWorker;
    // This builder uses AzFramework::StringFunc::Path::Join,
    // which uses \\ as a path separator.
    TestSuccessCase(&builderWorker, fileName, "libs/particles/PreloadLibsWithXmlExt.txt");
}

// The preload system makes sure that any references to a preload lib have a txt extension, this tests
// the case where the reference already has it.
TEST_F(CopyDependencyBuilderTest, TestParticlePreloadLib_SinglePreloadDependencyWithTxtExtension_OutputProductDependency)
{
    AZStd::string fileName = "Libs/Particles/PreloadWithPreloadRefTxtExt.txt";

    ParticlePreloadLibsBuilderWorker builderWorker;
    TestSuccessCase(&builderWorker, fileName, "libs/particles/PreloadWithPreloadRef.txt");
}

// The preload system makes sure that any references to a preload lib have a txt extension, this tests
// the case where the reference has a different extension. In ParticleManager.cpp, CParticleManager::LoadLibrary calls
// PathUtil::Make when building preload file paths, which eventually calls ReplaceExtension. This means a reference to a preload
// file will replace any extension in the reference with a txt file.
TEST_F(CopyDependencyBuilderTest, TestParticlePreloadLib_SinglePreloadDependencyWithOtherExtension_OutputProductDependency)
{
    AZStd::string fileName = "Libs/Particles/PreloadWithPreloadRefOtherExt.txt";

    ParticlePreloadLibsBuilderWorker builderWorker;
    TestSuccessCase(&builderWorker, fileName, "libs/particles/PreloadWithPreloadRefTxtExt.txt");
}

TEST_F(CopyDependencyBuilderTest, TestParticlePreloadLib_SingleDependencyWithXmlExtension_OutputProductDependency)
{
    AZStd::string fileName = "Libs/Particles/PreloadLibsWithXmlExt.txt";

    ParticlePreloadLibsBuilderWorker builderWorker;
    TestSuccessCase(&builderWorker, fileName, "libs/particles/SomeParticleLibrary.xml");
}

// See note on TestParticlePreloadLib_SinglePreloadDependencyWithOtherExtension_OutputProductDependency for details.
// Any file extension is replaced with XML for preload libraries that aren't other preload libraries.
TEST_F(CopyDependencyBuilderTest, TestParticlePreloadLib_SingleDependencyWithOtherExtension_OutputProductDependency)
{
    AZStd::string fileName = "Libs/Particles/PreloadLibsWithOtherExt.txt";

    ParticlePreloadLibsBuilderWorker builderWorker;
    TestSuccessCase(&builderWorker, fileName, "libs/particles/AnotherParticleLibrary.xml");
}

TEST_F(CopyDependencyBuilderTest, TestParticlePreloadLib_MultipleReferences_OutputAllProductDependencies)
{
    AZStd::vector<const char*> expectedPaths = {
        "libs/particles/PreloadFile.txt",
        "libs/particles/ParticleFile.xml",
        "libs/particles/PreloadOtherExt.txt",
        "libs/particles/ParticleFileOtherExt.xml",
        "libs/particles/Preload.txt",
        "libs/particles/Particle.xml",
        "libs/particles/PreloadOppositeExt.txt",
        "libs/particles/ParticleOppositeExt.xml"
    };
    AZStd::string fileName = "Libs/Particles/PreloadMultipleReferences.txt";
    ParticlePreloadLibsBuilderWorker builderWorker;
    TestSuccessCase(&builderWorker, fileName, expectedPaths);
}

TEST_F(CopyDependencyBuilderTest, TestParticlePreloadLib_DependenciesWithPath_OutputProductDependencies)
{
    AZStd::vector<const char*> expectedPaths = {
        "libs/particles/Directory/Preload.txt",
        "libs/particles/Folder/ParticleFile.xml"
    };

    AZStd::string fileName = "Libs/Particles/PreloadWithSubfolders.txt";
    ParticlePreloadLibsBuilderWorker builderWorker;
    TestSuccessCase(&builderWorker, fileName, expectedPaths);
}

TEST_F(CopyDependencyBuilderTest, TestParticlePreloadLib_EmptyFile_NoDependencies)
{
    AZStd::string fileName = "Libs/Particles/PreloadEmptyFile.txt";

    ParticlePreloadLibsBuilderWorker builderWorker;
    TestSuccessCaseNoDependencies(&builderWorker, fileName);
}

TEST_F(CopyDependencyBuilderTest, TestParticlePreloadLib_OnlyWhitespace_NoDependencies)
{
    AZStd::string fileName = "Libs/Particles/PreloadOnlyWhitespace.txt";

    ParticlePreloadLibsBuilderWorker builderWorker;
    TestSuccessCaseNoDependencies(&builderWorker, fileName);
}

// Wildcards are not yet supported in preload files.
TEST_F(CopyDependencyBuilderTest, TestParticlePreloadLib_Wildcard_ErrorOccurs)
{
    AZStd::string fileName = "Libs/Particles/PreloadWildcard.txt";

    ParticlePreloadLibsBuilderWorker builderWorker;
    // The function doesn't fail if a wildcard is encountered, because other
    // dependencies could be valid. It only generates an error.
    AZ_TEST_START_TRACE_SUPPRESSION;
    TestSuccessCaseNoDependencies(&builderWorker, fileName);
    // One error occurs, on the wildcard.
    AZ_TEST_STOP_TRACE_SUPPRESSION(1 * SuppressedErrorMultiplier);
}

TEST_F(CopyDependencyBuilderTest, TestParticlePreloadLib_AtSymbolOnly_ErrorOccurs)
{
    AZStd::string fileName = "Libs/Particles/PreloadOnlyAtSymbol.txt";

    ParticlePreloadLibsBuilderWorker builderWorker;
    // The function doesn't fail if a wildcard is encountered, because other
    // dependencies could be valid. It only generates an error.
    AZ_TEST_START_TRACE_SUPPRESSION;
    TestSuccessCaseNoDependencies(&builderWorker, fileName);
    // One error occurs, on the @ symbol by itself on a line.
    AZ_TEST_STOP_TRACE_SUPPRESSION(1 * SuppressedErrorMultiplier);
}

// This file has a wildcard (not yet supported) and an @ symbol on a line by itself, as well as some valid dependencies.
TEST_F(CopyDependencyBuilderTest, TestParticlePreloadLib_MultipleReferencesAndMultipleErrors_ErrorAndValidDependencies)
{
    AZStd::vector<const char*> expectedPaths = {
        "libs/particles/PreloadLib.txt",
        "libs/particles/SomeLibrary.xml"
    };
    AZStd::string fileName = "Libs/Particles/PreloadErrorsAndMultipleRefs.txt";
    ParticlePreloadLibsBuilderWorker builderWorker;
    AZ_TEST_START_TRACE_SUPPRESSION;
    TestSuccessCase(&builderWorker, fileName, expectedPaths);
    // Two errors occur: A wildcard symbol is in the file, and an @ on its own line.
    AZ_TEST_STOP_TRACE_SUPPRESSION(2 * SuppressedErrorMultiplier);
}

TEST_F(CopyDependencyBuilderTest, TestXmlAsset_ExcludedSourceFilePath_NoProductDependencies)
{
    AZStd::vector<const char*> expectedPaths;
    AZStd::string fileName = "Xmls/ExcludedFilePathExample.xml";
    XmlBuilderWorker builderWorker;
    builderWorker.AddSchemaFileDirectory(GetFullPath("Xmls/Schema/WithVersionConstraints"));
    TestSuccessCase(&builderWorker, fileName, expectedPaths);
}

TEST_F(CopyDependencyBuilderTest, TestXmlAsset_InvalidSchemaFormat_NoProductDependencies)
{
    AZStd::vector<const char*> expectedPaths;
    AZStd::string fileName = "Xmls/XmlExample.xml";
    XmlBuilderWorker builderWorker;
    AZStd::string fullPath = GetFullPath("Xmls/Schema/Invalid/InvalidFormat");
    builderWorker.AddSchemaFileDirectory(fullPath);
    AZ_TEST_START_TRACE_SUPPRESSION;
    // The expected result is true because the invalid schema doesn't mean this XML file itself has failed to parse, it may be matched
    // by other schemas.
    TestFailureCase(&builderWorker, fileName, /*expectedResult*/ true);
    // Three errors occur: RapidXML parse error (unexpected end of data), ObjectStream XML parse error and schema file loading error
    AZ_TEST_STOP_TRACE_SUPPRESSION(3 * SuppressedErrorMultiplier);
}

TEST_F(CopyDependencyBuilderTest, TestXmlAsset_InvalidSourceFilePath_NoProductDependencies)
{
    AZStd::string fileName = "Xmls/InvalidFilePathExample.xml";
    XmlBuilderWorker builderWorker;
    builderWorker.AddSchemaFileDirectory(GetFullPath("Xmls/Schema/WithoutVersionConstraints/FullFeatured"));
    AZ_TEST_START_TRACE_SUPPRESSION;
    TestFailureCase(&builderWorker, fileName);
    // One error occurs: Cannot open the source file
    AZ_TEST_STOP_TRACE_SUPPRESSION(1 * SuppressedErrorMultiplier);
}

TEST_F(CopyDependencyBuilderTest, TestXmlAsset_InvalidSourceFileVersionNumberFormat_NoProductDependencies)
{
    AZStd::vector<const char*> expectedPaths;
    AZStd::string fileName = "Xmls/XmlExampleInvalidVersionNumberFormat.xml";
    XmlBuilderWorker builderWorker;
    builderWorker.AddSchemaFileDirectory(GetFullPath("Xmls/Schema/WithVersionConstraints"));
    TestSuccessCase(&builderWorker, fileName, expectedPaths);
}

TEST_F(CopyDependencyBuilderTest, TestXmlAsset_NoMatchedSchema_NoProductDependencies)
{
    AZStd::vector<const char*> expectedPaths;
    AZStd::string fileName = "Xmls/NoMatchedSchemaExample.xml";
    XmlBuilderWorker builderWorker;
    builderWorker.AddSchemaFileDirectory(GetFullPath("Xmls/Schema/WithoutVersionConstraints/FullFeatured"));
    TestSuccessCase(&builderWorker, fileName, expectedPaths);
}

TEST_F(CopyDependencyBuilderTest, TestXmlAsset_SchemaMissingRules_NoProductDependencies)
{
    AZStd::vector<const char*> expectedPaths;
    AZStd::string fileName = "Xmls/XmlExample.xml";
    XmlBuilderWorker builderWorker;
    builderWorker.AddSchemaFileDirectory(GetFullPath("Xmls/Schema/Invalid/MissingRules"));
    AZ_TEST_START_TRACE_SUPPRESSION;
    TestFailureCase(&builderWorker, fileName, true);
    // One error occurs: Matching rules are missing
    AZ_TEST_STOP_TRACE_SUPPRESSION(1 * SuppressedErrorMultiplier);
}

TEST_F(CopyDependencyBuilderTest, TestXmlAsset_SchemaEmptyAttributeValue_OutputProductDependencies)
{
    AZStd::vector<const char*> expectedPaths = {
        "dependency2.txt",
        "dependency3.txt",
        "dependency4.txt",
        "dependency5.txt",
        "dependency6.txt",
        "dependency7.txt"
    };

    AZStd::string fileName = "Xmls/XmlExampleEmptyAttributeValue.xml";
    XmlBuilderWorker builderWorker;
    builderWorker.AddSchemaFileDirectory(GetFullPath("Xmls/Schema/WithoutVersionConstraints/FullFeatured"));
    TestSuccessCase(&builderWorker, fileName, expectedPaths);
}

TEST_F(CopyDependencyBuilderTest, TestXmlAsset_DependencySearchRuleForSpecificAttribute_OutputProductDependencies)
{
    AZStd::vector<const char*> expectedPaths = {
        "dependency1.txt",
        "dependency2.txt",
        "dependency6.txt",
        "dependency7.txt"
    };

    AZStd::string fileName = "Xmls/XmlExampleWithoutVersion.xml";
    XmlBuilderWorker builderWorker;
    builderWorker.AddSchemaFileDirectory(GetFullPath("Xmls/Schema/WithoutVersionConstraints/SpecificAttribute"));
    TestSuccessCase(&builderWorker, fileName, expectedPaths);
}

TEST_F(CopyDependencyBuilderTest, TestXmlAsset_DependencySearchRuleForSpecificElement_OutputProductDependencies)
{
    AZStd::vector<const char*> expectedPaths = {
        "dependency3.txt",
        "dependency4.txt",
        "dependency5.txt"
    };

    AZStd::string fileName = "Xmls/XmlExampleWithoutVersion.xml";
    XmlBuilderWorker builderWorker;
    builderWorker.AddSchemaFileDirectory(GetFullPath("Xmls/Schema/WithoutVersionConstraints/SpecificElement"));
    TestSuccessCase(&builderWorker, fileName, expectedPaths);
}

TEST_F(CopyDependencyBuilderTest, TestXmlAsset_DependencySearchRuleRelativeToXmlRootNode_OutputProductDependencies)
{
    AZStd::vector<const char*> expectedPaths = {
        "dependency1.txt",
        "dependency2.txt"
    };

    AZStd::string fileName = "Xmls/XmlExampleWithoutVersion.xml";
    XmlBuilderWorker builderWorker;
    builderWorker.AddSchemaFileDirectory(GetFullPath("Xmls/Schema/WithoutVersionConstraints/RelativeToXmlRootNode"));
    TestSuccessCase(&builderWorker, fileName, expectedPaths);
}

TEST_F(CopyDependencyBuilderTest, TestXmlAsset_DependencySearchRuleWithExpectedExtension_OutputProductDependencies)
{
    AZStd::string fileName = "Xmls/XmlExampleWithoutExtension.xml";
    XmlBuilderWorker builderWorker;
    builderWorker.AddSchemaFileDirectory(GetFullPath("Xmls/Schema/WithoutVersionConstraints/ExpectedExtension"));
    TestSuccessCase(&builderWorker, fileName, "dependency2.txt");
}

// The schema supports different extensions at the same location in the file.
// This matches the behavior of systems like materials referencing textures: They can reference the source (png/tif) or product (dds)
TEST_F(CopyDependencyBuilderTest, TestXmlAsset_MultipleOverlappingOptionalExtensions_OutputProductDependencies)
{
    AZStd::vector<const char*> expectedPaths = {
        "Extension1.ext1",
        "Extension2.ext2"
    };
    AZStd::string fileName = "Xmls/XmlExampleMultipleMatchingExtensions.xml";
    XmlBuilderWorker builderWorker;
    builderWorker.AddSchemaFileDirectory(GetFullPath("Xmls/Schema/WithoutVersionConstraints/MultipleExtensionsSamePath"));
    TestSuccessCase(&builderWorker, fileName, expectedPaths);
}

TEST_F(CopyDependencyBuilderTest, TestXmlAsset_DependencySearchRuleWithOptionalAttribute_OutputProductDependencies)
{
    AZStd::vector<const char*> expectedPaths = {
        "dependency1.txt",
        "dependency2.txt"
    };

    AZStd::string fileName = "Xmls/XmlExampleWithoutVersion.xml";
    XmlBuilderWorker builderWorker;
    builderWorker.AddSchemaFileDirectory(GetFullPath("Xmls/Schema/WithoutVersionConstraints/OptionalAttribute"));
    TestSuccessCase(&builderWorker, fileName, expectedPaths);
}

TEST_F(CopyDependencyBuilderTest, TestXmlAsset_DependencySearchRuleWithMissingRequiredAttribute_NoProductDependencies)
{
    AZStd::vector<const char*> expectedPaths;

    AZStd::string fileName = "Xmls/XmlExampleWithoutVersion.xml";
    XmlBuilderWorker builderWorker;
    builderWorker.AddSchemaFileDirectory(GetFullPath("Xmls/Schema/WithoutVersionConstraints/RequiredAttribute"));
    TestSuccessCase(&builderWorker, fileName, expectedPaths);
}

TEST_F(CopyDependencyBuilderTest, TestXmlAsset_DependencySearchRuleWithOptionalElement_OutputProductDependencies)
{
    AZStd::vector<const char*> expectedPaths = {
        "dependency1.txt",
        "dependency2.txt"
    };

    AZStd::string fileName = "Xmls/XmlExampleWithoutVersion.xml";
    XmlBuilderWorker builderWorker;
    builderWorker.AddSchemaFileDirectory(GetFullPath("Xmls/Schema/WithoutVersionConstraints/OptionalElement"));
    TestSuccessCase(&builderWorker, fileName, expectedPaths);
}

TEST_F(CopyDependencyBuilderTest, TestXmlAsset_DependencySearchRuleWithMissingRequiredElement_NoProductDependencies)
{
    AZStd::vector<const char*> expectedPaths;

    AZStd::string fileName = "Xmls/XmlExampleWithoutVersion.xml";
    XmlBuilderWorker builderWorker;
    builderWorker.AddSchemaFileDirectory(GetFullPath("Xmls/Schema/WithoutVersionConstraints/RequiredElements"));
    TestSuccessCase(&builderWorker, fileName, expectedPaths);
}

TEST_F(CopyDependencyBuilderTest, TestXmlAsset_SourceFileWithoutVersionSchemaWithVersionConstraints_NoProductDependencies)
{
    AZStd::vector<const char*> expectedPaths;

    AZStd::string fileName = "Xmls/XmlExampleWithoutVersion.xml";
    XmlBuilderWorker builderWorker;
    builderWorker.AddSchemaFileDirectory(GetFullPath("Xmls/Schema/WithVersionConstraints"));
    TestSuccessCase(&builderWorker, fileName, expectedPaths);
}

TEST_F(CopyDependencyBuilderTest, TestXmlAsset_SourceFileWithVersionOutOfRangeSchemaWithVersionConstraints_NoProductDependencies)
{
    AZStd::vector<const char*> expectedPaths;
    AZStd::string fileName = "Xmls/XmlExampleVersionOutOfRange.xml";
    XmlBuilderWorker builderWorker;
    builderWorker.AddSchemaFileDirectory(GetFullPath("Xmls/Schema/WithVersionConstraints"));
    TestSuccessCase(&builderWorker, fileName, expectedPaths);
}

TEST_F(CopyDependencyBuilderTest, TestXmlAsset_SourceFileWithVersionSchemaWithVersionConstraints_OutputProductDependencies)
{
    AZStd::vector<const char*> expectedPaths = {
        "dependency1.txt",
        "dependency2.txt",
        "dependency3.txt",
        "dependency4.txt",
        "dependency5.txt",
        "dependency6.txt",
        "dependency7.txt"
    };

    AZStd::string fileName = "Xmls/XmlExample.xml";
    XmlBuilderWorker builderWorker;
    builderWorker.AddSchemaFileDirectory(GetFullPath("Xmls/Schema/WithVersionConstraints"));
    TestSuccessCase(&builderWorker, fileName, expectedPaths);
}

TEST_F(CopyDependencyBuilderTest, TestXmlAsset_SourceFileWithOneVersionPartSchemaWithVersionConstraints_OutputProductDependencies)
{
    AZStd::vector<const char*> expectedPaths = {
        "dependency1.txt",
        "dependency2.txt",
        "dependency3.txt",
        "dependency4.txt",
        "dependency5.txt",
        "dependency6.txt",
        "dependency7.txt"
    };

    AZStd::string fileName = "Xmls/XmlExampleWithOneVersionPart.xml";
    XmlBuilderWorker builderWorker;
    builderWorker.AddSchemaFileDirectory(GetFullPath("Xmls/Schema/WithVersionConstraints"));
    TestSuccessCase(&builderWorker, fileName, expectedPaths);
}

TEST_F(CopyDependencyBuilderTest, TestXmlAsset_SourceFileWithTwoVersionPartsSchemaWithVersionConstraints_OutputProductDependencies)
{
    AZStd::vector<const char*> expectedPaths = {
        "dependency1.txt",
        "dependency2.txt",
        "dependency3.txt",
        "dependency4.txt",
        "dependency5.txt",
        "dependency6.txt",
        "dependency7.txt"
    };

    AZStd::string fileName = "Xmls/XmlExampleWithTwoVersionParts.xml";
    XmlBuilderWorker builderWorker;
    builderWorker.AddSchemaFileDirectory(GetFullPath("Xmls/Schema/WithVersionConstraints"));
    TestSuccessCase(&builderWorker, fileName, expectedPaths);
}

TEST_F(CopyDependencyBuilderTest, TestXmlAsset_SourceFileWithThreeVersionPartsSchemaWithVersionConstraints_OutputProductDependencies)
{
    AZStd::vector<const char*> expectedPaths = {
        "dependency1.txt",
        "dependency2.txt",
        "dependency3.txt",
        "dependency4.txt",
        "dependency5.txt",
        "dependency6.txt",
        "dependency7.txt"
    };

    AZStd::string fileName = "Xmls/XmlExampleWithThreeVersionParts.xml";
    XmlBuilderWorker builderWorker;
    builderWorker.AddSchemaFileDirectory(GetFullPath("Xmls/Schema/WithVersionConstraints"));
    TestSuccessCase(&builderWorker, fileName, expectedPaths);
}

TEST_F(CopyDependencyBuilderTest, TestXmlAsset_SourceFileWithInvalidVersionPartsCountSchemaWithVersionConstraints_OutputNoProductDependencies)
{
    AZStd::vector<const char*> expectedPaths;

    AZStd::string fileName = "Xmls/XmlExampleWithInvalidVersionPartsCount.xml";
    XmlBuilderWorker builderWorker;
    builderWorker.AddSchemaFileDirectory(GetFullPath("Xmls/Schema/WithVersionConstraints"));
    TestSuccessCase(&builderWorker, fileName, expectedPaths);
}

TEST_F(CopyDependencyBuilderTest, TestXmlAsset_SourceFileWithInvalidVersionPartsSeparatorSchemaWithVersionConstraints_OutputNoProductDependencies)
{
    AZStd::vector<const char*> expectedPaths;

    AZStd::string fileName = "Xmls/XmlExampleWithInvalidVersionPartsSeparator.xml";
    XmlBuilderWorker builderWorker;
    builderWorker.AddSchemaFileDirectory(GetFullPath("Xmls/Schema/WithVersionConstraints"));
    TestSuccessCase(&builderWorker, fileName, expectedPaths);
}

TEST_F(CopyDependencyBuilderTest, TestXmlAsset_SourceFileWithVersionSchemaWithoutVersionConstraints_OutputProductDependencies)
{
    AZStd::vector<const char*> expectedPaths = {
        "dependency1.txt",
        "dependency2.txt",
        "dependency3.txt",
        "dependency4.txt",
        "dependency5.txt",
        "dependency6.txt",
        "dependency7.txt"
    };

    AZStd::string fileName = "Xmls/XmlExample.xml";
    XmlBuilderWorker builderWorker;
    builderWorker.AddSchemaFileDirectory(GetFullPath("Xmls/Schema/WithoutVersionConstraints/FullFeatured"));
    TestSuccessCase(&builderWorker, fileName, expectedPaths);
}

TEST_F(CopyDependencyBuilderTest, TestXmlAsset_SourceFileWithoutVersionSchemaWithoutVersionConstraints_OutputProductDependencies)
{
    AZStd::vector<const char*> expectedPaths = {
        "dependency1.txt",
        "dependency2.txt",
        "dependency3.txt",
        "dependency4.txt",
        "dependency5.txt",
        "dependency6.txt",
        "dependency7.txt"
    };

    AZStd::string fileName = "Xmls/XmlExampleWithoutVersion.xml";
    XmlBuilderWorker builderWorker;
    builderWorker.AddSchemaFileDirectory(GetFullPath("Xmls/Schema/WithoutVersionConstraints/FullFeatured"));
    TestSuccessCase(&builderWorker, fileName, expectedPaths);
}

TEST_F(CopyDependencyBuilderTest, TestXmlAsset_CreateJobsWithValidSourceFile_OutputSourceDependencies)
{
    AssetBuilderSDK::CreateJobsRequest request;
    AssetBuilderSDK::CreateJobsResponse response;

    request.m_sourceFile = "Tests/Xmls/XmlExampleWithoutVersion.xml";
    request.m_watchFolder = "@root@/../Code/Tools/AssetProcessor/Builders/CopyDependencyBuilder";

    XmlBuilderWorker builderWorker;
    builderWorker.AddSchemaFileDirectory(GetFullPath("Xmls/Schema/WithoutVersionConstraints/FullFeatured"));
    builderWorker.CreateJobs(request, response);

    ASSERT_TRUE(response.m_sourceFileDependencyList.size() == 1);
    AZStd::vector<AZStd::string> splitedPathList;
    AzFramework::StringFunc::Tokenize(response.m_sourceFileDependencyList[0].m_sourceFileDependencyPath.c_str(), splitedPathList, AZStd::string::format("%c%c",AZ_CORRECT_FILESYSTEM_SEPARATOR, AZ_WRONG_FILESYSTEM_SEPARATOR).c_str());
    ASSERT_EQ(splitedPathList.back(), "Schema.xmlschema");
}

TEST_F(CopyDependencyBuilderTest, TestXmlAsset_ProductPathRelativeToSourceAssetFolder_OutputProductDependencies)
{
    AZStd::string product = GetFullPath("Xmls/dependency1.txt");
    AZStd::vector<const char*> expectedPaths = {
        product.c_str()
    };

    AZStd::string fileName = "Xmls/XmlExample.xml";
    XmlBuilderWorker builderWorker;
    builderWorker.AddSchemaFileDirectory(GetFullPath("Xmls/Schema/WithoutVersionConstraints/PathRelativeToSourceAssetFolder"));
    TestSuccessCase(&builderWorker, fileName, expectedPaths);
}

TEST_F(CopyDependencyBuilderTest, TestXmlAsset_ProductDependencyWithAssetId_OutputProductDependencies)
{
    AZStd::string product = GetFullPath("Xmls/dependency1.txt");
    AZStd::vector<const char*> expectedPaths;

    AZ::Data::AssetId expectedAssetId;
    expectedAssetId.m_guid = AZ::Uuid("00000000-0000-0000-0000-000000000000");
    expectedAssetId.m_subId = 0;

    AZStd::vector<AssetBuilderSDK::ProductDependency> expectedProductDependencies;
    expectedProductDependencies.emplace_back(AssetBuilderSDK::ProductDependency(expectedAssetId, {}));

    AZStd::string fileName = "Xmls/XmlExampleWithAssetId.xml";
    XmlBuilderWorker builderWorker;
    builderWorker.AddSchemaFileDirectory(GetFullPath("Xmls/Schema/WithoutVersionConstraints/ProductDependencyWithAssetId"));
    TestSuccessCase(&builderWorker, fileName, expectedPaths, expectedProductDependencies);
}

TEST_F(CopyDependencyBuilderTest, TestSchemaAsset_ValidMatchingRules_OutputReverseSourceDependencies)
{
    AZStd::vector<AZStd::string> expectedPaths = {
        "NoMatchedSchemaExample.xml",
        "XmlExample.xml",
        "XmlExampleEmptyAttributeValue.xml",
        "XmlExampleInvalidVersionNumberFormat.xml",
        "XmlExampleMultipleMatchingExtensions.xml",
        "XmlExampleVersionOutOfRange.xml",
        "XmlExampleWithoutExtension.xml",
        "XmlExampleWithoutVersion.xml",
        "XmlExampleWithOneVersionPart.xml",
        "XmlExampleWithTwoVersionParts.xml",
        "XmlExampleWithThreeVersionParts.xml",
        "XmlExampleWithInvalidVersionPartsSeparator.xml",
        "XmlExampleWithInvalidVersionPartsCount.xml",
        "XmlExampleWithAssetId.xml",
    };

    AZStd::string fileName = "Xmls/XmlExampleWithoutVersion.xml";
    SchemaBuilderWorker builderWorker;
    AssetBuilderSDK::ProcessJobRequest request;
    AssetBuilderSDK::ProcessJobResponse response;
    request.m_fullPath = GetFullPath("Xmls/Schema/WithoutVersionConstraints/FullFeatured/Schema.xmlschema");
    request.m_sourceFile = "Xmls/Schema/WithoutVersionConstraints/FullFeatured/Schema.xmlschema";
    request.m_platformInfo.m_identifier = m_currentPlatform;

    builderWorker.ProcessJob(request, response);

    ASSERT_TRUE(response.m_sourcesToReprocess.size() == expectedPaths.size());
    for (const AZStd::string& reverseSourceDependency : response.m_sourcesToReprocess)
    {
        AZStd::string fileName = reverseSourceDependency;
        AZStd::vector<AZStd::string> splitedPathList;
        AzFramework::StringFunc::Tokenize(reverseSourceDependency.c_str(), splitedPathList, AZStd::string::format("%c%c", AZ_CORRECT_FILESYSTEM_SEPARATOR, AZ_WRONG_FILESYSTEM_SEPARATOR).c_str());
        ASSERT_TRUE(AZStd::find(expectedPaths.begin(), expectedPaths.end(), splitedPathList.back()) != expectedPaths.end());

    }
}

TEST_F(CopyDependencyBuilderTest, TestSchemaAsset_InvalidFormat_OutputReverseSourceDependencies)
{
    AZStd::string fileName = "Xmls/XmlExampleWithoutVersion.xml";
    SchemaBuilderWorker builderWorker;
    AssetBuilderSDK::ProcessJobRequest request;
    AssetBuilderSDK::ProcessJobResponse response;
    request.m_fullPath = GetFullPath("Xmls/Schema/Invalid/InvalidFormat/Schema.xmlschema");
    request.m_sourceFile = "Xmls/Schema/Invalid/InvalidFormat/Schema.xmlschema";
    request.m_platformInfo.m_identifier = m_currentPlatform;

    AZ_TEST_START_TRACE_SUPPRESSION;
    builderWorker.ProcessJob(request, response);
    // Three errors: One from LoadObjectFromFileInPlace, one is from GetReverseSourceDependencies and the other from ProcessJob
    AZ_TEST_STOP_TRACE_SUPPRESSION(3 * SuppressedErrorMultiplier);

    ASSERT_TRUE(response.m_sourcesToReprocess.size() == 0);
}

TEST_F(CopyDependencyBuilderTest, TestSchemaAsset_SchemaMissingRules_OutputReverseSourceDependencies)
{
    AZStd::string fileName = "Xmls/XmlExampleWithoutVersion.xml";
    SchemaBuilderWorker builderWorker;
    AssetBuilderSDK::ProcessJobRequest request;
    AssetBuilderSDK::ProcessJobResponse response;
    request.m_fullPath = GetFullPath("Xmls/Schema/Invalid/MissingRules/Schema.xmlschema");
    request.m_sourceFile = "Xmls/Schema/Invalid/MissingRules/Schema.xmlschema";
    request.m_platformInfo.m_identifier = m_currentPlatform;

    AZ_TEST_START_TRACE_SUPPRESSION;
    builderWorker.ProcessJob(request, response);
    AZ_TEST_STOP_TRACE_SUPPRESSION(SuppressedErrorMultiplier);
     
    ASSERT_TRUE(response.m_sourcesToReprocess.size() == 0);
}

AZ_UNIT_TEST_HOOK();
