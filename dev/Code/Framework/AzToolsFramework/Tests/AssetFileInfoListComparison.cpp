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

#include <AzCore/UnitTest/TestTypes.h>
#include <AzToolsFramework/Asset/AssetSeedManager.h>
#include <AzFramework/Asset/AssetRegistry.h>
#include <AzCore/IO/FileIO.h>
#include <AzFramework/IO/LocalFileIO.h>
#include <AzFramework/Asset/AssetCatalog.h>
#include <AzFramework/StringFunc/StringFunc.h>
#include <AzTestShared/Utils/Utils.h>
#include <AzToolsFramework/Application/ToolsApplication.h>
#include <AzToolsFramework/Asset/AssetBundler.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/std/containers/unordered_set.h>
#include <AzFramework/Platform/PlatformDefaults.h>
#include <AzToolsFramework/AssetCatalog/PlatformAddressedAssetCatalog.h>


namespace // anonymous
{
    constexpr int TotalAssets = 6;
    constexpr int TotalTempFiles = 3;
    constexpr char TempFiles[TotalTempFiles][AZ_MAX_PATH_LEN] = { "firstAssetFileInfoList.assetlist", "secondAssetFileInfoList.assetlist", "assetFileInfoList.assetlist" };

    enum FileIndex
    {
        FirstAssetFileInfoList,
        SecondAssetFileInfoList,
        ResultAssetFileInfoList
    };
}


namespace UnitTest
{
    class AssetFileInfoListComparisonTest
        : public AllocatorsFixture
        , public AZ::Data::AssetCatalogRequestBus::Handler
    {
    public:
        void SetUp() override
        {
            using namespace AZ::Data;
            m_application = new AzToolsFramework::ToolsApplication();
            AzToolsFramework::AssetSeedManager assetSeedManager;
            AzFramework::AssetRegistry assetRegistry;

            m_localFileIO = aznew AZ::IO::LocalFileIO();

            m_priorFileIO = AZ::IO::FileIOBase::GetInstance();
            AZ::IO::FileIOBase::SetInstance(m_localFileIO);

            AZ::IO::FileIOBase::GetInstance()->SetAlias("@assets@", GetTestFolderPath().c_str());
            AZStd::string assetRoot = AzToolsFramework::PlatformAddressedAssetCatalog::GetAssetRootForPlatform(AzFramework::PlatformId::PC);

            for (int idx = 0; idx < TotalAssets; idx++)
            {
                m_assets[idx] = AssetId(AZ::Uuid::CreateRandom(), 0);
                AZ::Data::AssetInfo info;
                info.m_relativePath = AZStd::string::format("Asset%d.txt", idx);
                info.m_assetId = m_assets[idx];
                assetRegistry.RegisterAsset(m_assets[idx], info);

                AzFramework::StringFunc::Path::Join(assetRoot.c_str(), info.m_relativePath.c_str(), m_assetsPath[idx]);
                if (m_fileStreams[idx].Open(m_assetsPath[idx].c_str(), AZ::IO::OpenMode::ModeWrite | AZ::IO::OpenMode::ModeBinary | AZ::IO::OpenMode::ModeCreatePath))
                {
                    m_fileStreams[idx].Write(info.m_relativePath.size(), info.m_relativePath.data());
                }
                else
                {
                    GTEST_FATAL_FAILURE_(AZStd::string::format("Unable to create temporary file ( %s ) in AssetSeedManager unit tests.\n", m_assetsPath[idx].c_str()).c_str());
                }
            }

            // asset1 -> asset2
            assetRegistry.RegisterAssetDependency(m_assets[1], AZ::Data::ProductDependency(m_assets[2], 0));
            // asset2 -> asset3
            assetRegistry.RegisterAssetDependency(m_assets[2], AZ::Data::ProductDependency(m_assets[3], 0));
            // asset3 -> asset4
            assetRegistry.RegisterAssetDependency(m_assets[3], AZ::Data::ProductDependency(m_assets[4], 0));

            m_application->Start(AzFramework::Application::Descriptor());

            AZ::SerializeContext* context;
            AZ::ComponentApplicationBus::BroadcastResult(context, &AZ::ComponentApplicationBus::Events::GetSerializeContext);
            ASSERT_TRUE(context) << "No serialize context.\n";

            AzToolsFramework::AssetSeedManager::Reflect(context);

            // Asset Catalog does not expose its internal asset registry and the only way to set it is through LoadCatalog API
            // Currently I am serializing the asset registry to disk
            // and invoking the LoadCatalog API to populate the asset catalog created by the azframework app.

            AZStd::string pcCatalogFile = AzToolsFramework::PlatformAddressedAssetCatalog::GetCatalogRegistryPathForPlatform(AzFramework::PlatformId::PC);

            ASSERT_TRUE(AzFramework::AssetCatalog::SaveCatalog(pcCatalogFile.c_str(), &assetRegistry)) << "Unable to save the asset catalog file.\n";

            m_pcCatalog = new AzToolsFramework::PlatformAddressedAssetCatalog(AzFramework::PlatformId::PC);

            assetSeedManager.AddSeedAsset(m_assets[0], AzFramework::PlatformFlags::Platform_PC);
            assetSeedManager.AddSeedAsset(m_assets[1], AzFramework::PlatformFlags::Platform_PC);

            assetSeedManager.SaveAssetFileInfo(TempFiles[FileIndex::FirstAssetFileInfoList], AzFramework::PlatformFlags::Platform_PC);

            // Modify contents of asset2
            int fileIndex = 2;
            if (m_fileStreams[fileIndex].Open(m_assetsPath[fileIndex].c_str(), AZ::IO::OpenMode::ModeWrite | AZ::IO::OpenMode::ModeBinary | AZ::IO::OpenMode::ModeCreatePath))
            {
                AZStd::string fileContent = AZStd::string::format("new Asset%d.txt", fileIndex);// changing file content
                m_fileStreams[fileIndex].Write(fileContent.size(), fileContent.c_str());
            }
            else
            {
                GTEST_FATAL_FAILURE_(AZStd::string::format("Unable to open asset file.\n").c_str());
            }

            // Modify contents of asset 4
            fileIndex = 4;
            if (m_fileStreams[fileIndex].Open(m_assetsPath[fileIndex].c_str(), AZ::IO::OpenMode::ModeWrite | AZ::IO::OpenMode::ModeBinary | AZ::IO::OpenMode::ModeCreatePath))
            {
                AZStd::string fileContent = AZStd::string::format("new Asset%d.txt", fileIndex);// changing file content
                m_fileStreams[fileIndex].Write(fileContent.size(), fileContent.c_str());
            }
            else
            {
                GTEST_FATAL_FAILURE_(AZStd::string::format("Unable to open asset file.\n").c_str());
            }

            assetSeedManager.RemoveSeedAsset(m_assets[0], AzFramework::PlatformFlags::Platform_PC);
            assetSeedManager.AddSeedAsset(m_assets[5], AzFramework::PlatformFlags::Platform_PC);

            assetSeedManager.SaveAssetFileInfo(TempFiles[FileIndex::SecondAssetFileInfoList], AzFramework::PlatformFlags::Platform_PC);
        }

        void TearDown() override
        {
            AZ::IO::FileIOBase* fileIO = AZ::IO::FileIOBase::GetInstance();

            // Delete all temporary files
            for (int idx = 0; idx < TotalTempFiles; idx++)
            {
                if (fileIO->Exists(TempFiles[idx]))
                {
                    fileIO->Remove(TempFiles[idx]);
                }
            }

            // Deleting all temporary assets files
            for (int idx = 0; idx < TotalAssets; idx++)
            {
                // we need to close the handle before we try to remove the file
                m_fileStreams[idx].Close();
                if (fileIO->Exists(m_assetsPath[idx].c_str()))
                {
                    fileIO->Remove(m_assetsPath[idx].c_str());
                }
            }

            auto pcCatalogFile = AzToolsFramework::PlatformAddressedAssetCatalog::GetCatalogRegistryPathForPlatform(AzFramework::PlatformId::PC);
            if (fileIO->Exists(pcCatalogFile.c_str()))
            {
                fileIO->Remove(pcCatalogFile.c_str());
            }

            delete m_pcCatalog;
            delete m_localFileIO;
            m_localFileIO = nullptr;
            AZ::IO::FileIOBase::SetInstance(m_priorFileIO);
            m_application->Stop();
            delete m_application;

        }

        void AssetFileInfoValidation_DeltaComparison_Valid()
        {
            // First AssetFileInfoList {0,1,2,3,4} , Second AssetFileInfoList {1,2*,3,4*,5} where * indicate that hash has changed for that asset
            AzToolsFramework::AssetFileInfoListComparison assetFileInfoListComparison;
            AzToolsFramework::AssetFileInfoListComparison::ComparisonData comparisonData(AzToolsFramework::AssetFileInfoListComparison::ComparisonType::Delta, TempFiles[FileIndex::ResultAssetFileInfoList]);
            assetFileInfoListComparison.AddComparisonStep(comparisonData);

            ASSERT_TRUE(assetFileInfoListComparison.CompareAndSaveResults({ TempFiles[FileIndex::FirstAssetFileInfoList] }, { TempFiles[FileIndex::SecondAssetFileInfoList] }).IsSuccess()) << "Delta operation failed.\n";

            // AssetFileInfo should contain {2*, 4*, 5}
            AzToolsFramework::AssetFileInfoList assetFileInfoList;
           
            ASSERT_TRUE(AZ::Utils::LoadObjectFromFileInPlace(TempFiles[FileIndex::ResultAssetFileInfoList], assetFileInfoList)) << "Unable to read the asset file info list.\n";

            EXPECT_EQ(assetFileInfoList.m_fileInfoList.size(), 3);

            // Verifying that the hash of the file are correct. They must be from the second AssetFileInfoList.
            AzToolsFramework::AssetFileInfoList secondAssetFileInfoList;
            ASSERT_TRUE(AZ::Utils::LoadObjectFromFileInPlace(TempFiles[FileIndex::SecondAssetFileInfoList], secondAssetFileInfoList)) << "Unable to read the asset file info list.\n";

            AZStd::unordered_map<AZ::Data::AssetId, AzToolsFramework::AssetFileInfo> assetIdToAssetFileInfoMap;

            for (const AzToolsFramework::AssetFileInfo& assetFileInfo : secondAssetFileInfoList.m_fileInfoList)
            {
                assetIdToAssetFileInfoMap[assetFileInfo.m_assetId] = AZStd::move(assetFileInfo);
            }

            for (const AzToolsFramework::AssetFileInfo& assetFileInfo : assetFileInfoList.m_fileInfoList)
            {
                auto found = assetIdToAssetFileInfoMap.find(assetFileInfo.m_assetId);
                if (found != assetIdToAssetFileInfoMap.end())
                {
                    // checking the file hash
                    for (int idx = 0; idx < AzToolsFramework::AssetFileInfo::s_arraySize; idx++)
                    {
                        if (found->second.m_hash[idx] != assetFileInfo.m_hash[idx])
                        {
                            GTEST_FATAL_FAILURE_(AZStd::string::format("Invalid file hash.\n").c_str());
                            break;
                        }
                    }
                }
            }

            // Verifying that correct assetId are present in the assetFileInfo list 
            AZStd::unordered_set<AZ::Data::AssetId> expectedAssetIds{ m_assets[2], m_assets[4], m_assets[5] };

            for (const AzToolsFramework::AssetFileInfo& assetFileInfo : assetFileInfoList.m_fileInfoList)
            {
                auto found = expectedAssetIds.find(assetFileInfo.m_assetId);
                if (found != expectedAssetIds.end())
                {
                    expectedAssetIds.erase(found);
                }
            }

            EXPECT_EQ(expectedAssetIds.size(), 0);
        }


        void AssetFileInfoValidation_UnionComparison_Valid()
        {
            // First AssetFileInfoList {0,1,2,3,4} , Second AssetFileInfoList {1,2*,3,4*,5} where * indicate that hash has changed for that asset
            AzToolsFramework::AssetFileInfoListComparison assetFileInfoListComparison;
            AzToolsFramework::AssetFileInfoListComparison::ComparisonData comparisonData(AzToolsFramework::AssetFileInfoListComparison::ComparisonType::Union, TempFiles[FileIndex::ResultAssetFileInfoList]);
            assetFileInfoListComparison.AddComparisonStep(comparisonData);
            ASSERT_TRUE(assetFileInfoListComparison.CompareAndSaveResults({ TempFiles[FileIndex::FirstAssetFileInfoList] }, { TempFiles[FileIndex::SecondAssetFileInfoList] }).IsSuccess()) << "Union operation failed.\n";

            // AssetFileInfo should contain {0, 1, 2*, 3, 4*, 5}
            AzToolsFramework::AssetFileInfoList assetFileInfoList;
            ASSERT_TRUE(AZ::Utils::LoadObjectFromFileInPlace(TempFiles[FileIndex::ResultAssetFileInfoList], assetFileInfoList)) << "Unable to read the asset file info list.\n";

            EXPECT_EQ(assetFileInfoList.m_fileInfoList.size(), 6);

            //Verifying that the hash of the files are correct.
            AzToolsFramework::AssetFileInfoList firstAssetFileInfoList;
            ASSERT_TRUE(AZ::Utils::LoadObjectFromFileInPlace(TempFiles[FileIndex::FirstAssetFileInfoList], firstAssetFileInfoList)) << "Unable to read the asset file info list.\n";

            AZStd::unordered_map<AZ::Data::AssetId, AzToolsFramework::AssetFileInfo> firstAssetIdToAssetFileInfoMap;

            for (const AzToolsFramework::AssetFileInfo& assetFileInfo : firstAssetFileInfoList.m_fileInfoList)
            {
                firstAssetIdToAssetFileInfoMap[assetFileInfo.m_assetId] = AZStd::move(assetFileInfo);
            }
            
            AzToolsFramework::AssetFileInfoList secondAssetFileInfoList;
            ASSERT_TRUE(AZ::Utils::LoadObjectFromFileInPlace(TempFiles[FileIndex::SecondAssetFileInfoList], secondAssetFileInfoList)) << "Unable to read the asset file info list.\n";

            AZStd::unordered_map<AZ::Data::AssetId, AzToolsFramework::AssetFileInfo> secondAssetIdToAssetFileInfoMap;

            for (const AzToolsFramework::AssetFileInfo& assetFileInfo : secondAssetFileInfoList.m_fileInfoList)
            {
                secondAssetIdToAssetFileInfoMap[assetFileInfo.m_assetId] = AZStd::move(assetFileInfo);
            }

            for (const AzToolsFramework::AssetFileInfo& assetFileInfo : assetFileInfoList.m_fileInfoList)
            {
                auto foundFirst = firstAssetIdToAssetFileInfoMap.find(assetFileInfo.m_assetId);
                auto foundSecond = secondAssetIdToAssetFileInfoMap.find(assetFileInfo.m_assetId);
                if (foundSecond != secondAssetIdToAssetFileInfoMap.end())
                {
                    // Even if the asset Id is present in both the AssetFileInfo List, it should match the file hash from the second AssetFileInfo list 
                    for (int idx = 0; idx < AzToolsFramework::AssetFileInfo::s_arraySize; idx++)
                    {
                        if (foundSecond->second.m_hash[idx] != assetFileInfo.m_hash[idx])
                        {
                            GTEST_FATAL_FAILURE_(AZStd::string::format("Invalid file hash.\n").c_str());
                            break;
                        }
                    }
                }
                else if (foundFirst != firstAssetIdToAssetFileInfoMap.end())
                {
                    // checking the file hash
                    for (int idx = 0; idx < AzToolsFramework::AssetFileInfo::s_arraySize; idx++)
                    {
                        if (foundFirst->second.m_hash[idx] != assetFileInfo.m_hash[idx])
                        {
                            GTEST_FATAL_FAILURE_(AZStd::string::format("Invalid file hash.\n").c_str());
                            break;
                        }
                    }
                }
                else
                {
                    GTEST_FATAL_FAILURE_(AZStd::string::format("Invalid file hash.\n").c_str());
                }
            }

            // Verifying that correct assetId are present in the assetFileInfo list 
            AZStd::unordered_set<AZ::Data::AssetId> expectedAssetIds{ m_assets[0], m_assets[1], m_assets[2], m_assets[3], m_assets[4], m_assets[5] };

            for (const AzToolsFramework::AssetFileInfo& assetFileInfo : assetFileInfoList.m_fileInfoList)
            {
                auto found = expectedAssetIds.find(assetFileInfo.m_assetId);
                if (found != expectedAssetIds.end())
                {
                    expectedAssetIds.erase(found);
                }
            }

            EXPECT_EQ(expectedAssetIds.size(), 0);
        }

        void AssetFileInfoValidation_IntersectionComparison_Valid()
        {
            // First AssetFileInfoList {0,1,2,3,4} , Second AssetFileInfoList {1,2*,3,4*,5} where * indicate that hash has changed for that asset
            AzToolsFramework::AssetFileInfoListComparison assetFileInfoListComparison;
            AzToolsFramework::AssetFileInfoListComparison::ComparisonData comparisonData(AzToolsFramework::AssetFileInfoListComparison::ComparisonType::Intersection, TempFiles[FileIndex::ResultAssetFileInfoList]);
            assetFileInfoListComparison.AddComparisonStep(comparisonData);
            ASSERT_TRUE(assetFileInfoListComparison.CompareAndSaveResults({ TempFiles[FileIndex::FirstAssetFileInfoList] }, { TempFiles[FileIndex::SecondAssetFileInfoList] }).IsSuccess()) << "Intersection operation failed.\n";

            // AssetFileInfo should contain {1,2*,3,4*}
            AzToolsFramework::AssetFileInfoList assetFileInfoList;
            ASSERT_TRUE(AZ::Utils::LoadObjectFromFileInPlace(TempFiles[FileIndex::ResultAssetFileInfoList], assetFileInfoList)) << "Unable to read the asset file info list.\n";

            EXPECT_EQ(assetFileInfoList.m_fileInfoList.size(), 4);

            // Verifying that the hash of the file are correct. They must be from the second AssetFileInfoList.
            AzToolsFramework::AssetFileInfoList secondAssetFileInfoList;

            ASSERT_TRUE(AZ::Utils::LoadObjectFromFileInPlace(TempFiles[FileIndex::SecondAssetFileInfoList], secondAssetFileInfoList)) << "Unable to read the asset file info list.\n";

            AZStd::unordered_map<AZ::Data::AssetId, AzToolsFramework::AssetFileInfo> assetIdToAssetFileInfoMap;

            for (const AzToolsFramework::AssetFileInfo& assetFileInfo : secondAssetFileInfoList.m_fileInfoList)
            {
                assetIdToAssetFileInfoMap[assetFileInfo.m_assetId] = AZStd::move(assetFileInfo);
            }

            for (const AzToolsFramework::AssetFileInfo& assetFileInfo : assetFileInfoList.m_fileInfoList)
            {
                auto found = assetIdToAssetFileInfoMap.find(assetFileInfo.m_assetId);
                if (found != assetIdToAssetFileInfoMap.end())
                {
                    // checking the file hash
                    for (int idx = 0; idx < AzToolsFramework::AssetFileInfo::s_arraySize; idx++)
                    {
                        if (found->second.m_hash[idx] != assetFileInfo.m_hash[idx])
                        {
                            GTEST_FATAL_FAILURE_(AZStd::string::format("Invalid file hash.\n").c_str());
                            break;
                        }
                    }
                }
            }

            // Verifying that correct assetId are present in the assetFileInfo list 
            AZStd::unordered_set<AZ::Data::AssetId> expectedAssetIds{ m_assets[1], m_assets[2], m_assets[3], m_assets[4] };

            for (const AzToolsFramework::AssetFileInfo& assetFileInfo : assetFileInfoList.m_fileInfoList)
            {
                auto found = expectedAssetIds.find(assetFileInfo.m_assetId);
                if (found != expectedAssetIds.end())
                {
                    expectedAssetIds.erase(found);
                }
            }

            EXPECT_EQ(expectedAssetIds.size(), 0);
        }

        void AssetFileInfoValidation_ComplementComparison_Valid()
        {
            using namespace AzToolsFramework;
            // First AssetFileInfoList {0,1,2,3,4} , Second AssetFileInfoList {1,2*,3,4*,5} where * indicate that hash has changed for that asset
            AssetFileInfoListComparison assetFileInfoListComparison;
            AzToolsFramework::AssetFileInfoListComparison::ComparisonData comparisonData(AssetFileInfoListComparison::ComparisonType::Complement, TempFiles[FileIndex::ResultAssetFileInfoList]);
            assetFileInfoListComparison.AddComparisonStep(comparisonData);
            ASSERT_TRUE(assetFileInfoListComparison.CompareAndSaveResults({ TempFiles[FileIndex::FirstAssetFileInfoList] }, { TempFiles[FileIndex::SecondAssetFileInfoList] }).IsSuccess()) << "Complement comparison failed.\n";

            // AssetFileInfo should contain {5}
            AssetFileInfoList assetFileInfoList;
            ASSERT_TRUE(AZ::Utils::LoadObjectFromFileInPlace(TempFiles[FileIndex::ResultAssetFileInfoList], assetFileInfoList)) << "Unable to read the asset file info list.\n";
            EXPECT_EQ(assetFileInfoList.m_fileInfoList.size(), 1);

            // Verifying that the hash of the file are correct. They must be from the second AssetFileInfoList.
            AssetFileInfoList secondAssetFileInfoList;
            ASSERT_TRUE(AZ::Utils::LoadObjectFromFileInPlace(TempFiles[FileIndex::SecondAssetFileInfoList], secondAssetFileInfoList)) << "Unable to read the asset file info list.\n";

            AZStd::unordered_map<AZ::Data::AssetId, AssetFileInfo> assetIdToAssetFileInfoMap;

            for (const AssetFileInfo& assetFileInfo : secondAssetFileInfoList.m_fileInfoList)
            {
                assetIdToAssetFileInfoMap[assetFileInfo.m_assetId] = AZStd::move(assetFileInfo);
            }

            for (const AssetFileInfo& assetFileInfo : assetFileInfoList.m_fileInfoList)
            {
                auto found = assetIdToAssetFileInfoMap.find(assetFileInfo.m_assetId);
                if (found != assetIdToAssetFileInfoMap.end())
                {
                    // checking the file hash
                    for (int idx = 0; idx < AssetFileInfo::s_arraySize; idx++)
                    {
                        if (found->second.m_hash[idx] != assetFileInfo.m_hash[idx])
                        {
                            GTEST_FATAL_FAILURE_(AZStd::string::format("Invalid file hash.\n").c_str());
                            break;
                        }
                    }
                }
            }

            // Verifying that correct assetId are present in the assetFileInfo list 
            AZStd::unordered_set<AZ::Data::AssetId> expectedAssetIds{ m_assets[5] };

            for (const AzToolsFramework::AssetFileInfo& assetFileInfo : assetFileInfoList.m_fileInfoList)
            {
                auto found = expectedAssetIds.find(assetFileInfo.m_assetId);
                if (found != expectedAssetIds.end())
                {
                    expectedAssetIds.erase(found);
                }
            }

            EXPECT_EQ(expectedAssetIds.size(), 0);
        }

        void AssetFileInfoValidation_FilePatternWildcardComparisonAll_Valid()
        {
            using namespace AzToolsFramework;
            // First AssetFileInfoList {0,1,2,3,4} , Second AssetFileInfoList {1,2*,3,4*,5} where * indicate that hash has changed for that asset
            AssetFileInfoListComparison assetFileInfoListComparison;
            AssetFileInfoListComparison::ComparisonData comparisonData(AssetFileInfoListComparison::ComparisonType::FilePattern, TempFiles[FileIndex::ResultAssetFileInfoList], "Asset*.txt", AssetFileInfoListComparison::FilePatternType::Wildcard);
            assetFileInfoListComparison.AddComparisonStep(comparisonData);
            ASSERT_TRUE(assetFileInfoListComparison.CompareAndSaveResults({ TempFiles[FileIndex::FirstAssetFileInfoList] }).IsSuccess()) << "File pattern match failed.\n";

            // AssetFileInfo should contain {0,1,2,3,4}
            AssetFileInfoList assetFileInfoList;
            ASSERT_TRUE(AZ::Utils::LoadObjectFromFileInPlace(TempFiles[FileIndex::ResultAssetFileInfoList], assetFileInfoList)) << "Unable to read the asset file info list.\n";

            EXPECT_EQ(assetFileInfoList.m_fileInfoList.size(), 5);

            // Verifying that correct assetId are present in the assetFileInfo list 
            AZStd::unordered_set<AZ::Data::AssetId> expectedAssetIds{ m_assets[0], m_assets[1], m_assets[2], m_assets[3], m_assets[4] };

            for (const AzToolsFramework::AssetFileInfo& assetFileInfo : assetFileInfoList.m_fileInfoList)
            {
                auto found = expectedAssetIds.find(assetFileInfo.m_assetId);
                if (found != expectedAssetIds.end())
                {
                    expectedAssetIds.erase(found);
                }
            }

            EXPECT_EQ(expectedAssetIds.size(), 0);
        }

        void AssetFileInfoValidation_FilePatternWildcardComparisonNone_Valid()
        {
            using namespace AzToolsFramework;
            // First AssetFileInfoList {0,1,2,3,4} , Second AssetFileInfoList {1,2*,3,4*,5} where * indicate that hash has changed for that asset
            AssetFileInfoListComparison assetFileInfoListComparison;
            AssetFileInfoListComparison::ComparisonData comparisonData(AssetFileInfoListComparison::ComparisonType::FilePattern, TempFiles[FileIndex::ResultAssetFileInfoList], "Foo*.txt", AssetFileInfoListComparison::FilePatternType::Wildcard);
            assetFileInfoListComparison.AddComparisonStep(comparisonData);
            ASSERT_TRUE(assetFileInfoListComparison.CompareAndSaveResults({ TempFiles[FileIndex::FirstAssetFileInfoList] }).IsSuccess()) << "File pattern match failed.\n";

            // AssetFileInfo should be empty
            AssetFileInfoList assetFileInfoList;
            ASSERT_TRUE(AZ::Utils::LoadObjectFromFileInPlace(TempFiles[FileIndex::ResultAssetFileInfoList], assetFileInfoList)) << "Unable to read the asset file info list.\n";

            EXPECT_EQ(assetFileInfoList.m_fileInfoList.size(), 0);
        }

        void AssetFileInfoValidation_FilePatternRegexComparisonPartial_Valid()
        {
            using namespace AzToolsFramework;
            // First AssetFileInfoList {0,1,2,3,4} , Second AssetFileInfoList {1,2*,3,4*,5} where * indicate that hash has changed for that asset
            AssetFileInfoListComparison assetFileInfoListComparison;
            AssetFileInfoListComparison::ComparisonData comparisonData(AssetFileInfoListComparison::ComparisonType::FilePattern, TempFiles[FileIndex::ResultAssetFileInfoList], "Asset[0-3].txt", AssetFileInfoListComparison::FilePatternType::Regex);
            assetFileInfoListComparison.AddComparisonStep(comparisonData);
            ASSERT_TRUE(assetFileInfoListComparison.CompareAndSaveResults({ TempFiles[FileIndex::FirstAssetFileInfoList] }).IsSuccess()) << "File pattern match failed.\n";

            // AssetFileInfo should be {0,1,2,3}
            AssetFileInfoList assetFileInfoList;
            ASSERT_TRUE(AZ::Utils::LoadObjectFromFileInPlace(TempFiles[FileIndex::ResultAssetFileInfoList], assetFileInfoList)) << "Unable to read the asset file info list.\n";

            EXPECT_EQ(assetFileInfoList.m_fileInfoList.size(), 4);

            AZStd::unordered_set<AZ::Data::AssetId> expectedAssetIds{ m_assets[0], m_assets[1], m_assets[2], m_assets[3]};

            for (const AssetFileInfo& assetFileInfo : assetFileInfoList.m_fileInfoList)
            {
                auto found = expectedAssetIds.find(assetFileInfo.m_assetId);
                if (found != expectedAssetIds.end())
                {
                    expectedAssetIds.erase(found);
                }
            }

            EXPECT_EQ(expectedAssetIds.size(), 0);
        }

        void AssetFileInfoValidation_DeltaFilePatternComparisonOperation_Valid()
        {
            using namespace AzToolsFramework;
            // First AssetFileInfoList {0,1,2,3,4} , Second AssetFileInfoList {1,2*,3,4*,5} where * indicate that hash has changed for that asset
            AssetFileInfoListComparison assetFileInfoListComparison;
            AzToolsFramework::AssetFileInfoListComparison::ComparisonData deltaComparisonData(AzToolsFramework::AssetFileInfoListComparison::ComparisonType::Delta, "$1");
            assetFileInfoListComparison.AddComparisonStep(deltaComparisonData);
            AssetFileInfoListComparison::ComparisonData filePatternComparisonData(AssetFileInfoListComparison::ComparisonType::FilePattern, TempFiles[FileIndex::ResultAssetFileInfoList], "Asset[0-3].txt", AssetFileInfoListComparison::FilePatternType::Regex);
            assetFileInfoListComparison.AddComparisonStep(filePatternComparisonData);
            ASSERT_TRUE(assetFileInfoListComparison.CompareAndSaveResults({ TempFiles[FileIndex::FirstAssetFileInfoList], "$1" }, { TempFiles[FileIndex::SecondAssetFileInfoList] }).IsSuccess()) << "Multiple Comparison Operation( Delta + FilePattern ) failed.\n";

            // Output of the Delta Operation should be {2*, 4*, 5}
            // Output of the FilePattern Operation should be {2*}
            AzToolsFramework::AssetFileInfoList assetFileInfoList;
            ASSERT_TRUE(AZ::Utils::LoadObjectFromFileInPlace(TempFiles[FileIndex::ResultAssetFileInfoList], assetFileInfoList)) << "Unable to read the asset file info list.\n";

            EXPECT_EQ(assetFileInfoList.m_fileInfoList.size(), 1);

            AZStd::unordered_map<AZ::Data::AssetId, AssetFileInfo> assetIdToAssetFileInfoMap;

            AzToolsFramework::AssetFileInfoList secondAssetFileInfoList;
            ASSERT_TRUE(AZ::Utils::LoadObjectFromFileInPlace(TempFiles[FileIndex::SecondAssetFileInfoList], secondAssetFileInfoList)) << "Unable to read the asset file info list.\n";

            for (const AzToolsFramework::AssetFileInfo& assetFileInfo : secondAssetFileInfoList.m_fileInfoList)
            {
                assetIdToAssetFileInfoMap[assetFileInfo.m_assetId] = AZStd::move(assetFileInfo);
            }

            for (const AzToolsFramework::AssetFileInfo& assetFileInfo : assetFileInfoList.m_fileInfoList)
            {
                auto found = assetIdToAssetFileInfoMap.find(assetFileInfo.m_assetId);
                if (found != assetIdToAssetFileInfoMap.end())
                {
                    // checking the file hash
                    for (int idx = 0; idx < AzToolsFramework::AssetFileInfo::s_arraySize; idx++)
                    {
                        if (found->second.m_hash[idx] != assetFileInfo.m_hash[idx])
                        {
                            GTEST_FATAL_FAILURE_(AZStd::string::format("Invalid file hash.\n").c_str());
                            break;
                        }
                    }
                }
            }

            // Verifying that correct assetId are present in the assetFileInfo list 
            AZStd::unordered_set<AZ::Data::AssetId> expectedAssetIds{ m_assets[2] };

            for (const AzToolsFramework::AssetFileInfo& assetFileInfo : assetFileInfoList.m_fileInfoList)
            {
                auto found = expectedAssetIds.find(assetFileInfo.m_assetId);
                if (found != expectedAssetIds.end())
                {
                    expectedAssetIds.erase(found);
                }
            }

            EXPECT_EQ(expectedAssetIds.size(), 0);

        }

        void AssetFileInfoValidation_FilePatternDeltaComparisonOperation_Valid()
        {
            using namespace AzToolsFramework;
            // First AssetFileInfoList {0,1,2,3,4} , Second AssetFileInfoList {1,2*,3,4*,5} where * indicate that hash has changed for that asset
            AssetFileInfoListComparison assetFileInfoListComparison;
            AssetFileInfoListComparison::ComparisonData filePatternComparisonData(AssetFileInfoListComparison::ComparisonType::FilePattern,"$1", "Asset[0-3].txt", AssetFileInfoListComparison::FilePatternType::Regex);
            assetFileInfoListComparison.AddComparisonStep(filePatternComparisonData);
            
            AzToolsFramework::AssetFileInfoListComparison::ComparisonData deltaComparisonData(AzToolsFramework::AssetFileInfoListComparison::ComparisonType::Delta, TempFiles[FileIndex::ResultAssetFileInfoList]);
            assetFileInfoListComparison.AddComparisonStep(deltaComparisonData);
            
            ASSERT_TRUE(assetFileInfoListComparison.CompareAndSaveResults({ TempFiles[FileIndex::FirstAssetFileInfoList], "$1" }, { TempFiles[FileIndex::SecondAssetFileInfoList] }).IsSuccess()) << "Multiple Comparison Operation( FilePattern + Delta ) failed.\n";
            // Output of the FilePattern Operation should be {0,1,2,3}
            // Output of the Delta Operation should be {2*,4*,5}
            AzToolsFramework::AssetFileInfoList assetFileInfoList;
            ASSERT_TRUE(AZ::Utils::LoadObjectFromFileInPlace(TempFiles[FileIndex::ResultAssetFileInfoList], assetFileInfoList)) << "Unable to read the asset file info list.\n";

            EXPECT_EQ(assetFileInfoList.m_fileInfoList.size(), 3);

            AZStd::unordered_map<AZ::Data::AssetId, AssetFileInfo> assetIdToAssetFileInfoMap;

            AzToolsFramework::AssetFileInfoList secondAssetFileInfoList;
            ASSERT_TRUE(AZ::Utils::LoadObjectFromFileInPlace(TempFiles[FileIndex::SecondAssetFileInfoList], secondAssetFileInfoList)) << "Unable to read the asset file info list.\n";

            for (const AzToolsFramework::AssetFileInfo& assetFileInfo : secondAssetFileInfoList.m_fileInfoList)
            {
                assetIdToAssetFileInfoMap[assetFileInfo.m_assetId] = AZStd::move(assetFileInfo);
            }

            for (const AzToolsFramework::AssetFileInfo& assetFileInfo : assetFileInfoList.m_fileInfoList)
            {
                auto found = assetIdToAssetFileInfoMap.find(assetFileInfo.m_assetId);
                if (found != assetIdToAssetFileInfoMap.end())
                {
                    // checking the file hash
                    for (int idx = 0; idx < AzToolsFramework::AssetFileInfo::s_arraySize; idx++)
                    {
                        if (found->second.m_hash[idx] != assetFileInfo.m_hash[idx])
                        {
                            GTEST_FATAL_FAILURE_(AZStd::string::format("Invalid file hash.\n").c_str());
                            break;
                        }
                    }
                }
            }

            // Verifying that correct assetId are present in the assetFileInfo list 
            AZStd::unordered_set<AZ::Data::AssetId> expectedAssetIds{ m_assets[2], m_assets[4], m_assets[5] };

            for (const AzToolsFramework::AssetFileInfo& assetFileInfo : assetFileInfoList.m_fileInfoList)
            {
                auto found = expectedAssetIds.find(assetFileInfo.m_assetId);
                if (found != expectedAssetIds.end())
                {
                    expectedAssetIds.erase(found);
                }
            }

            EXPECT_EQ(expectedAssetIds.size(), 0);

        }

        void AssetFileInfoValidation_DeltaUnionFilePatternComparisonOperation_Valid()
        {
            using namespace AzToolsFramework;
            // First AssetFileInfoList {0,1,2,3,4} , Second AssetFileInfoList {1,2*,3,4*,5} where * indicate that hash has changed for that asset
            AssetFileInfoListComparison assetFileInfoListComparison;
            AzToolsFramework::AssetFileInfoListComparison::ComparisonData deltaComparisonData(AzToolsFramework::AssetFileInfoListComparison::ComparisonType::Delta, "$1");
            assetFileInfoListComparison.AddComparisonStep(deltaComparisonData);
            AssetFileInfoListComparison::ComparisonData unionComparisonData(AssetFileInfoListComparison::ComparisonType::Union, "$2");
            assetFileInfoListComparison.AddComparisonStep(unionComparisonData);
            AssetFileInfoListComparison::ComparisonData filePatternComparisonData(AssetFileInfoListComparison::ComparisonType::FilePattern, TempFiles[FileIndex::ResultAssetFileInfoList], "Asset[4-5].txt", AssetFileInfoListComparison::FilePatternType::Regex);
            assetFileInfoListComparison.AddComparisonStep(filePatternComparisonData);

            ASSERT_TRUE(assetFileInfoListComparison.CompareAndSaveResults({ TempFiles[FileIndex::FirstAssetFileInfoList], TempFiles[FileIndex::FirstAssetFileInfoList], "$2" }, { TempFiles[FileIndex::SecondAssetFileInfoList], "$1" }).IsSuccess()) << "Multiple Comparison Operation( Delta + Union + FilePattern ) failed.\n";

            // Output of the Delta Operation should be {2*, 4*, 5}
            // Putput of the Union Operation should be {0, 1, 2*, 3, 4*, 5}
            // Output of the FilePattern Operation should be {4*, 5}
            AzToolsFramework::AssetFileInfoList assetFileInfoList;
            ASSERT_TRUE(AZ::Utils::LoadObjectFromFileInPlace(TempFiles[FileIndex::ResultAssetFileInfoList], assetFileInfoList)) << "Unable to read the asset file info list.\n";

            EXPECT_EQ(assetFileInfoList.m_fileInfoList.size(), 2);

            AZStd::unordered_map<AZ::Data::AssetId, AssetFileInfo> assetIdToAssetFileInfoMap;

            AzToolsFramework::AssetFileInfoList secondAssetFileInfoList;
            ASSERT_TRUE(AZ::Utils::LoadObjectFromFileInPlace(TempFiles[FileIndex::SecondAssetFileInfoList], secondAssetFileInfoList)) << "Unable to read the asset file info list.\n";

            for (const AzToolsFramework::AssetFileInfo& assetFileInfo : secondAssetFileInfoList.m_fileInfoList)
            {
                assetIdToAssetFileInfoMap[assetFileInfo.m_assetId] = AZStd::move(assetFileInfo);
            }

            for (const AzToolsFramework::AssetFileInfo& assetFileInfo : assetFileInfoList.m_fileInfoList)
            {
                auto found = assetIdToAssetFileInfoMap.find(assetFileInfo.m_assetId);
                if (found != assetIdToAssetFileInfoMap.end())
                {
                    // checking the file hash
                    for (int idx = 0; idx < AzToolsFramework::AssetFileInfo::s_arraySize; idx++)
                    {
                        if (found->second.m_hash[idx] != assetFileInfo.m_hash[idx])
                        {
                            GTEST_FATAL_FAILURE_(AZStd::string::format("Invalid file hash.\n").c_str());
                            break;
                        }
                    }
                }
            }

            // Verifying that correct assetId are present in the assetFileInfo list 
            AZStd::unordered_set<AZ::Data::AssetId> expectedAssetIds{ m_assets[4], m_assets[5] };

            for (const AzToolsFramework::AssetFileInfo& assetFileInfo : assetFileInfoList.m_fileInfoList)
            {
                auto found = expectedAssetIds.find(assetFileInfo.m_assetId);
                if (found != expectedAssetIds.end())
                {
                    expectedAssetIds.erase(found);
                }
            }

            EXPECT_EQ(expectedAssetIds.size(), 0);

        }

        AzToolsFramework::ToolsApplication* m_application;
        AzToolsFramework::PlatformAddressedAssetCatalog* m_pcCatalog;
        AZ::IO::FileIOBase* m_priorFileIO = nullptr;
        AZ::IO::FileIOBase* m_localFileIO = nullptr;
        AZ::IO::FileIOStream m_fileStreams[TotalAssets];
        AZ::Data::AssetId m_assets[TotalAssets];
        AZStd::string m_assetsPath[TotalAssets];
    };

    TEST_F(AssetFileInfoListComparisonTest, AssetFileInfoValidation_DeltaComparison_Valid)
    {
        AssetFileInfoValidation_DeltaComparison_Valid();
    }

    TEST_F(AssetFileInfoListComparisonTest, AssetFileInfoValidation_UnionComparison_Valid)
    {
        AssetFileInfoValidation_UnionComparison_Valid();
    }

    TEST_F(AssetFileInfoListComparisonTest, AssetFileInfoValidation_IntersectionComparison_Valid)
    {
        AssetFileInfoValidation_IntersectionComparison_Valid();
    }

    TEST_F(AssetFileInfoListComparisonTest, AssetFileInfoValidation_ComplementComparison_Valid)
    {
        AssetFileInfoValidation_ComplementComparison_Valid();
    }

    TEST_F(AssetFileInfoListComparisonTest, AssetFileInfoValidation_FilePatternWildcardComparisonAll_Valid)
    {
        AssetFileInfoValidation_FilePatternWildcardComparisonAll_Valid();
    }

    TEST_F(AssetFileInfoListComparisonTest, AssetFileInfoValidation_FilePatternWildcardComparisonNone_Valid)
    {
        AssetFileInfoValidation_FilePatternWildcardComparisonNone_Valid();
    }

    TEST_F(AssetFileInfoListComparisonTest, AssetFileInfoValidation_FilePatternRegexComparisonPartial_Valid)
    {
        AssetFileInfoValidation_FilePatternRegexComparisonPartial_Valid();
    }

    TEST_F(AssetFileInfoListComparisonTest, AssetFileInfoValidation_DeltaFilePatternComparisonOperation_Valid)
    {
        AssetFileInfoValidation_DeltaFilePatternComparisonOperation_Valid();
    }

    TEST_F(AssetFileInfoListComparisonTest, AssetFileInfoValidation_FilePatternDeltaComparisonOperation_Valid)
    {
        AssetFileInfoValidation_FilePatternDeltaComparisonOperation_Valid();
    }

    TEST_F(AssetFileInfoListComparisonTest, AssetFileInfoValidation_DeltaUnionFilePatternComparisonOperation_Valid)
    {
        AssetFileInfoValidation_DeltaUnionFilePatternComparisonOperation_Valid();
    }
}
