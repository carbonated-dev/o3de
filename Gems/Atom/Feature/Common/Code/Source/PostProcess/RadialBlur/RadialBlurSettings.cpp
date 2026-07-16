/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <PostProcess/RadialBlur/RadialBlurSettings.h>

#include <PostProcess/PostProcessFeatureProcessor.h>
#include <PostProcess/PostProcessSettings.h>
#include <AzCore/Math/MathUtils.h>

namespace AZ
{
    namespace Render
    {
        namespace
        {
            float ClampUnit(float value)
            {
                return AZ::GetClamp(value, 0.0f, 1.0f);
            }

            float ClampOverride(float value)
            {
                return ClampUnit(value);
            }

            AZ::Vector2 ClampUnitVector(AZ::Vector2 value)
            {
                return AZ::Vector2(ClampUnit(value.GetX()), ClampUnit(value.GetY()));
            }
        } // namespace

        RadialBlurSettings::RadialBlurSettings(PostProcessFeatureProcessor* featureProcessor)
            : PostProcessBase(featureProcessor)
        {
        }

        void RadialBlurSettings::OnConfigChanged()
        {
            m_parentSettings->OnConfigChanged();
        }

        void RadialBlurSettings::ApplySettingsTo(RadialBlurSettings* target, float alpha) const
        {
            AZ_Assert(target != nullptr, "RadialBlurSettings::ApplySettingsTo called with nullptr as argument.");

#define OVERRIDE_TARGET target
#define OVERRIDE_ALPHA alpha
#include <Atom/Feature/ParamMacros/StartOverrideBlend.inl>
#include <Atom/Feature/PostProcess/RadialBlur/RadialBlurParams.inl>
#include <Atom/Feature/ParamMacros/EndParams.inl>
#undef OVERRIDE_TARGET
#undef OVERRIDE_ALPHA
        }

        void RadialBlurSettings::Simulate(float deltaTime)
        {
            m_deltaTime = deltaTime;
        }

        bool RadialBlurSettings::GetEnabled() const
        {
            return m_enabled;
        }

        void RadialBlurSettings::SetEnabled(bool val)
        {
            m_enabled = val;
        }

        bool RadialBlurSettings::GetEnabledOverride() const
        {
            return m_enabledOverride;
        }

        void RadialBlurSettings::SetEnabledOverride(bool val)
        {
            m_enabledOverride = val;
        }

        AZ::Vector2 RadialBlurSettings::GetCenter() const
        {
            return m_center;
        }

        void RadialBlurSettings::SetCenter(AZ::Vector2 val)
        {
            m_center = ClampUnitVector(val);
        }

        float RadialBlurSettings::GetCenterOverride() const
        {
            return m_centerOverride;
        }

        void RadialBlurSettings::SetCenterOverride(float val)
        {
            m_centerOverride = ClampOverride(val);
        }

        float RadialBlurSettings::GetAmount() const
        {
            return m_amount;
        }

        void RadialBlurSettings::SetAmount(float val)
        {
            m_amount = ClampUnit(val);
        }

        float RadialBlurSettings::GetAmountOverride() const
        {
            return m_amountOverride;
        }

        void RadialBlurSettings::SetAmountOverride(float val)
        {
            m_amountOverride = ClampOverride(val);
        }

        float RadialBlurSettings::GetInnerRadius() const
        {
            return m_innerRadius;
        }

        void RadialBlurSettings::SetInnerRadius(float val)
        {
            m_innerRadius = ClampUnit(val);
        }

        float RadialBlurSettings::GetInnerRadiusOverride() const
        {
            return m_innerRadiusOverride;
        }

        void RadialBlurSettings::SetInnerRadiusOverride(float val)
        {
            m_innerRadiusOverride = ClampOverride(val);
        }

        uint32_t RadialBlurSettings::GetSampleCount() const
        {
            return m_sampleCount;
        }

        void RadialBlurSettings::SetSampleCount(uint32_t val)
        {
            m_sampleCount = AZ::GetClamp(val, RadialBlur::MinSampleCount, RadialBlur::MaxSampleCount);
        }

        bool RadialBlurSettings::GetSampleCountOverride() const
        {
            return m_sampleCountOverride;
        }

        void RadialBlurSettings::SetSampleCountOverride(bool val)
        {
            m_sampleCountOverride = val;
        }
    } // namespace Render
} // namespace AZ
