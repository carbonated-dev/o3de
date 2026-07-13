/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <Atom/Feature/PostProcess/RadialBlur/RadialBlurSettingsInterface.h>
#include <PostProcess/PostProcessBase.h>

namespace AZ
{
    namespace Render
    {
        class PostProcessSettings;

        //! Stores and blends the runtime settings for the radial blur post-processing effect.
        class RadialBlurSettings final
            : public RadialBlurSettingsInterface
            , public PostProcessBase
        {
            friend class PostProcessSettings;
            friend class PostProcessFeatureProcessor;

        public:
            AZ_RTTI(
                AZ::Render::RadialBlurSettings,
                "{995E5E32-A37D-4F4C-ACD7-9DF78315C17C}",
                AZ::Render::RadialBlurSettingsInterface,
                AZ::Render::PostProcessBase);
            AZ_CLASS_ALLOCATOR(RadialBlurSettings, SystemAllocator, 0);

            //! Creates radial blur settings owned by the post-process feature processor.
            RadialBlurSettings(PostProcessFeatureProcessor* featureProcessor);
            //! Destroy radial blur settings.
            ~RadialBlurSettings() = default;

            void OnConfigChanged() override;
            //! Blend this settings object into the target settings using the supplied layer alpha.
            void ApplySettingsTo(RadialBlurSettings* target, float alpha) const;

            bool GetEnabled() const override;
            void SetEnabled(bool val) override;
            bool GetEnabledOverride() const override;
            void SetEnabledOverride(bool val) override;

            AZ::Vector2 GetCenter() const override;
            void SetCenter(AZ::Vector2 val) override;
            float GetCenterOverride() const override;
            void SetCenterOverride(float val) override;

            float GetAmount() const override;
            void SetAmount(float val) override;
            float GetAmountOverride() const override;
            void SetAmountOverride(float val) override;

            float GetInnerRadius() const override;
            void SetInnerRadius(float val) override;
            float GetInnerRadiusOverride() const override;
            void SetInnerRadiusOverride(float val) override;

            uint32_t GetSampleCount() const override;
            void SetSampleCount(uint32_t val) override;
            bool GetSampleCountOverride() const override;
            void SetSampleCountOverride(bool val) override;

        private:
            // Generate members...
#include <Atom/Feature/ParamMacros/StartParamMembers.inl>
#include <Atom/Feature/PostProcess/RadialBlur/RadialBlurParams.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>

            void Simulate(float deltaTime);

            PostProcessSettings* m_parentSettings = nullptr;
            float m_deltaTime = 0.0f;
        };
    } // namespace Render
} // namespace AZ
