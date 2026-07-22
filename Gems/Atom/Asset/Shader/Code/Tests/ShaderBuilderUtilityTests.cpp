/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <AzTest/AzTest.h>
#include <AzCore/UnitTest/TestTypes.h>

#include "Common/ShaderBuilderTestFixture.h"

#include <ShaderBuilderUtility.h>

namespace UnitTest
{
    using namespace AZ;

    // The main purpose of this class is to test ShaderBuilderUtility functions
    class ShaderBuilderUtilityTests : public ShaderBuilderTestFixture
    {
    }; // class ShaderBuilderUtilityTests

    void ExpectHasIncludeFile(const AZStd::vector<AZStd::string> fileList, bool shouldExist, const char* filePath)
    {
        //We must normalize because internally AZ::ShaderBuilder::ShaderBuilderUtility::IncludedFilesParser
        // always returns normalized paths.
        AZStd::string filePathNormalized(filePath);
        AzFramework::StringFunc::Path::Normalize(filePathNormalized);
        auto it = AZStd::find(fileList.begin(), fileList.end(), filePathNormalized);
        if (shouldExist)
        {
            EXPECT_TRUE(it != fileList.end()) << "Could not find path '" << filePath << "' in the include list.";
        }
        else
        {
            EXPECT_TRUE(it == fileList.end()) << "Path '" << filePath << "' should not be in the include list.";
        }
    }

    TEST_F(ShaderBuilderUtilityTests, IncludedFilesParser_ParseStringAndGetIncludedFiles)
    {
        AZStd::string haystack(
            R"(
                Some content to parse
                #include <valid_file1.azsli>
                // #include <valid_file2.azsli>
                blah # include "valid_file3.azsli"
                bar include <a\dire-ctory\invalid-file4.azsli>
                foo #   include "a/directory/valid-file5.azsli"
                # include <a\dire-ctory\valid-file6.azsli>
                #includ "a\dire-ctory\invalid-file7.azsli"
                #include <..\Relative\Path\To\File.azsi>
                #include <C:\Absolute\Path\To\File.azsi>
            )"
        );

        AZ::ShaderBuilder::ShaderBuilderUtility::IncludedFilesParser includedFilesParser;
        auto fileList = includedFilesParser.ParseStringAndGetIncludedFiles(haystack);
        EXPECT_EQ(fileList.size(), 7);

        ExpectHasIncludeFile(fileList, true, "valid_file1.azsli");
        ExpectHasIncludeFile(fileList, true, "valid_file2.azsli");
        ExpectHasIncludeFile(fileList, true, "valid_file3.azsli");
        ExpectHasIncludeFile(fileList, false, "a\\dire-ctory\\invalid-file4.azsli");
        ExpectHasIncludeFile(fileList, true, "a\\directory\\valid-file5.azsli");
        ExpectHasIncludeFile(fileList, true, "a\\dire-ctory\\valid-file6.azsli");
        ExpectHasIncludeFile(fileList, false, "a\\dire-ctory\\invalid-file7.azsli");
        ExpectHasIncludeFile(fileList, true, "C:\\Absolute\\Path\\To\\File.azsi");
        ExpectHasIncludeFile(fileList, true, "..\\Relative\\Path\\To\\File.azsi");
    }

    TEST_F(ShaderBuilderUtilityTests, IncludedFilesParser_HandleMaterialPipelineMacro)
    {
        // This is a temporary solution to support material pipeline where the include path is specified in a #define and
        // later included like #include MATERIAL_TYPE_AZSLI_FILE_PATH

        AZStd::string haystack(
            R"(
                #define MATERIAL_TYPE_AZSLI_FILE_PATH "D:\o3de\Gems\Atom\TestData\TestData\Materials\Types\MaterialPipelineTest_Animated.azsli" 
                #include "D:\o3de\Gems\Atom\Feature\Common\Assets\Materials\Pipelines\LowEndPipeline\ForwardPass_BaseLighting.azsli" 
            )"
        );

        AZ::ShaderBuilder::ShaderBuilderUtility::IncludedFilesParser includedFilesParser;
        auto fileList = includedFilesParser.ParseStringAndGetIncludedFiles(haystack);
        EXPECT_EQ(fileList.size(), 2);

        ExpectHasIncludeFile(fileList, true, R"(D:\o3de\Gems\Atom\TestData\TestData\Materials\Types\MaterialPipelineTest_Animated.azsli)");
        ExpectHasIncludeFile(fileList, true, R"(D:\o3de\Gems\Atom\Feature\Common\Assets\Materials\Pipelines\LowEndPipeline\ForwardPass_BaseLighting.azsli)");
    }

    TEST_F(ShaderBuilderUtilityTests, GetSupervariantList_NoDeclaredSupervariants_AddsDefaultAndNoSpecialization)
    {
        RPI::ShaderSourceData shaderSourceData;
        RHI::ShaderBuildArguments buildArguments;
        buildArguments.m_azslcArguments = { "--sc-options" };

        const auto supervariants =
            ShaderBuilder::ShaderBuilderUtility::GetSupervariantListFromShaderSourceData(shaderSourceData, &buildArguments);

        ASSERT_EQ(supervariants.size(), 2);
        EXPECT_TRUE(supervariants[0].m_name.IsEmpty());
        EXPECT_EQ(supervariants[1].m_name, AZ::Name(RPI::NoSpecializationSupervariantName));
    }

    TEST_F(ShaderBuilderUtilityTests, GetSupervariantList_AddsNoSpecializationCompanionForEverySupervariant)
    {
        RPI::ShaderSourceData shaderSourceData;
        RPI::ShaderSourceData::SupervariantInfo customSupervariant;
        customSupervariant.m_name = "NoMS";
        customSupervariant.m_definitions.push_back("NO_MS_DEFINITION");
        customSupervariant.m_addBuildArguments.m_azslcArguments = { "--sc-options", "--no-ms-option" };
        customSupervariant.m_removeBuildArguments.m_azslcArguments = { "--no-ms-removal" };
        shaderSourceData.m_supervariants.push_back(customSupervariant);

        RPI::ShaderSourceData::SupervariantInfo defaultSupervariant;
        defaultSupervariant.m_definitions.push_back("DEFAULT_DEFINITION");
        defaultSupervariant.m_addBuildArguments.m_azslcArguments = { "--sc-options", "--default-option" };
        defaultSupervariant.m_removeBuildArguments.m_azslcArguments = { "--default-removal" };
        shaderSourceData.m_supervariants.push_back(defaultSupervariant);

        RHI::ShaderBuildArguments buildArguments;
        const auto supervariants =
            ShaderBuilder::ShaderBuilderUtility::GetSupervariantListFromShaderSourceData(shaderSourceData, &buildArguments);

        ASSERT_EQ(supervariants.size(), 4);
        EXPECT_TRUE(supervariants[0].m_name.IsEmpty());
        EXPECT_EQ(supervariants[1].m_name, AZ::Name("NoMS"));
        EXPECT_EQ(supervariants[2].m_name, AZ::Name(RPI::NoSpecializationSupervariantName));
        EXPECT_EQ(supervariants[3].m_name, AZ::Name("NoMSNoSpecialization"));

        EXPECT_EQ(supervariants[2].m_definitions, defaultSupervariant.m_definitions);
        EXPECT_TRUE(RHI::ShaderBuildArguments::HasArgument(
            supervariants[2].m_addBuildArguments.m_azslcArguments, "--default-option"));
        EXPECT_FALSE(RHI::ShaderBuildArguments::HasArgument(
            supervariants[2].m_addBuildArguments.m_azslcArguments, "--sc-options"));
        EXPECT_TRUE(RHI::ShaderBuildArguments::HasArgument(
            supervariants[2].m_removeBuildArguments.m_azslcArguments, "--default-removal"));
        EXPECT_TRUE(RHI::ShaderBuildArguments::HasArgument(
            supervariants[2].m_removeBuildArguments.m_azslcArguments, "--sc-options"));

        EXPECT_EQ(supervariants[3].m_definitions, customSupervariant.m_definitions);
        EXPECT_TRUE(RHI::ShaderBuildArguments::HasArgument(
            supervariants[3].m_addBuildArguments.m_azslcArguments, "--no-ms-option"));
        EXPECT_FALSE(RHI::ShaderBuildArguments::HasArgument(
            supervariants[3].m_addBuildArguments.m_azslcArguments, "--sc-options"));
        EXPECT_TRUE(RHI::ShaderBuildArguments::HasArgument(
            supervariants[3].m_removeBuildArguments.m_azslcArguments, "--no-ms-removal"));
        EXPECT_TRUE(RHI::ShaderBuildArguments::HasArgument(
            supervariants[3].m_removeBuildArguments.m_azslcArguments, "--sc-options"));
    }

    TEST_F(ShaderBuilderUtilityTests, GetSupervariantList_UsesPassedFullBuildArguments)
    {
        RPI::ShaderSourceData shaderSourceData;
        RPI::ShaderSourceData::SupervariantInfo noMsSupervariant;
        noMsSupervariant.m_name = "NoMS";
        noMsSupervariant.m_removeBuildArguments.m_azslcArguments = { "--sc-options" };
        shaderSourceData.m_supervariants.push_back(noMsSupervariant);

        // This is the complete argument set already assembled from the global, platform, RHI API, and shader scopes.
        RHI::ShaderBuildArguments buildArguments;
        buildArguments.m_preprocessorArguments =
            { "--global-cpp", "--platform-cpp", "--api-cpp", "--shader-cpp" };
        buildArguments.m_azslcArguments =
            { "--global-azslc", "--platform-azslc", "--api-azslc", "--shader-azslc", "--sc-options" };

        const auto supervariants =
            ShaderBuilder::ShaderBuilderUtility::GetSupervariantListFromShaderSourceData(shaderSourceData, &buildArguments);

        ASSERT_EQ(supervariants.size(), 3);
        EXPECT_TRUE(supervariants[0].m_name.IsEmpty());
        EXPECT_EQ(supervariants[1].m_name, AZ::Name("NoMS"));
        EXPECT_EQ(supervariants[2].m_name, AZ::Name(RPI::NoSpecializationSupervariantName));
    }

    TEST_F(ShaderBuilderUtilityTests, GetSupervariantList_WithoutBuildArgumentsReturnsOnlyBaseSupervariants)
    {
        RPI::ShaderSourceData shaderSourceData;
        RPI::ShaderSourceData::SupervariantInfo defaultSupervariant;
        defaultSupervariant.m_addBuildArguments.m_azslcArguments = { "--sc-options" };
        shaderSourceData.m_supervariants.push_back(defaultSupervariant);

        const auto supervariants =
            ShaderBuilder::ShaderBuilderUtility::GetSupervariantListFromShaderSourceData(shaderSourceData);

        ASSERT_EQ(supervariants.size(), 1);
        EXPECT_TRUE(supervariants[0].m_name.IsEmpty());
    }

    TEST_F(ShaderBuilderUtilityTests, GetSupervariantList_DeclaredNoSpecializationSuffix_IsRejected)
    {
        RPI::ShaderSourceData shaderSourceData;
        RPI::ShaderSourceData::SupervariantInfo reservedSupervariant;
        reservedSupervariant.m_name = "NoMSNoSpecialization";
        shaderSourceData.m_supervariants.push_back(reservedSupervariant);

        AZ_TEST_START_TRACE_SUPPRESSION;
        const auto supervariants =
            ShaderBuilder::ShaderBuilderUtility::GetSupervariantListFromShaderSourceData(shaderSourceData);
        AZ_TEST_STOP_TRACE_SUPPRESSION(1);

        EXPECT_TRUE(supervariants.empty());
    }

} //namespace UnitTest

//AZ_UNIT_TEST_HOOK(DEFAULT_UNIT_TEST_ENV);

