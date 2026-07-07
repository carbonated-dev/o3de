/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
->ClassElement(Edit::ClassElements::Group, "Density Control")
    ->Attribute(Edit::Attributes::AutoExpand, true)

->DataElement(
    AZ::Edit::UIHandlers::Color,
    &CONFIG_COMPONENT_NAME::m_fogAlbedo,
    "Albedo",
    "Single-scattering albedo: the fraction of light that scatters per channel. "
    "White = fully scattering (typical mist/haze). "
    "Dark = mostly absorbing (soot/smoke). "
    "Together with Fog Density, this fully controls how the fog looks.")
->Attribute(AZ::Edit::Attributes::ChangeNotify, Edit::PropertyRefreshLevels::ValuesOnly)

->DataElement(
    AZ::Edit::UIHandlers::Slider,
    &CONFIG_COMPONENT_NAME::m_fogDensity,
    "Density",
    "Total extinction coefficient (sigma_t). Controls how quickly light is attenuated through the fog. "
    "Higher values produce thicker, more opaque fog.")
    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
    ->Attribute(AZ::Edit::Attributes::Max, 1.0f)
    ->Attribute(AZ::Edit::Attributes::SoftMin, 0.0f)
    ->Attribute(AZ::Edit::Attributes::SoftMax, 1.0f)
    ->Attribute(AZ::Edit::Attributes::ChangeNotify, Edit::PropertyRefreshLevels::ValuesOnly)

->DataElement(
    AZ::Edit::UIHandlers::Slider,
    &CONFIG_COMPONENT_NAME::m_fogBaseHeight,
    "Base height",
    "The height at which the fog layer starts")
    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
    ->Attribute(AZ::Edit::Attributes::Max, 500.0f)
    ->Attribute(AZ::Edit::Attributes::SoftMin, 0.0f)
    ->Attribute(AZ::Edit::Attributes::SoftMax, 50.0f)
    ->Attribute(AZ::Edit::Attributes::ChangeNotify, Edit::PropertyRefreshLevels::ValuesOnly)

->DataElement(
    AZ::Edit::UIHandlers::Slider,
    &CONFIG_COMPONENT_NAME::m_fogHeightFalloff,
    "Falloff",
    "How quickly fog thins above the base height")
    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
    ->Attribute(AZ::Edit::Attributes::Max, 1.0f)
    ->Attribute(AZ::Edit::Attributes::SoftMin, 0.0f)
    ->Attribute(AZ::Edit::Attributes::SoftMax, 1.0f)
    ->Attribute(AZ::Edit::Attributes::ChangeNotify, Edit::PropertyRefreshLevels::ValuesOnly)

    // Fog turbulence properties
->ClassElement(Edit::ClassElements::Group, "Turbulence")
    ->Attribute(Edit::Attributes::AutoExpand, true)
->DataElement(AZ::Edit::UIHandlers::Default, &CONFIG_COMPONENT_NAME::m_noiseTexture,
    "Noise Texture", "The noise texture used for creating the fog turbulence")
    ->Attribute(AZ::Edit::Attributes::ChangeNotify, Edit::PropertyRefreshLevels::ValuesOnly)

->DataElement(
    AZ::Edit::UIHandlers::Slider,
    &CONFIG_COMPONENT_NAME::m_noiseScale,
    "Noise Scale",
    "UV tiling scale (smaller = larger blobs)")
    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
    ->Attribute(AZ::Edit::Attributes::Max, 1.0f)
    ->Attribute(AZ::Edit::Attributes::SoftMin, 0.0f)
    ->Attribute(AZ::Edit::Attributes::SoftMax, 1.0f)
    ->Attribute(AZ::Edit::Attributes::ChangeNotify, Edit::PropertyRefreshLevels::ValuesOnly)

->DataElement(
    AZ::Edit::UIHandlers::Vector3,
    &CONFIG_COMPONENT_NAME::m_noiseWindVelocity,
    "Noise Wind Velocity",
    "Wind velocity (direction and magnitude)")
    ->Attribute(AZ::Edit::Attributes::ChangeNotify, Edit::PropertyRefreshLevels::ValuesOnly)

->ClassElement(Edit::ClassElements::Group, "Lighting")
    ->Attribute(Edit::Attributes::AutoExpand, true)

->DataElement(
    AZ::Edit::UIHandlers::Slider,
    &CONFIG_COMPONENT_NAME::m_phaseG,
    "Anisotropy",
    "Phase function asymmetry parameter g for Henyey-Greenstein")
    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
    ->Attribute(AZ::Edit::Attributes::Max, 1.0f)
    ->Attribute(AZ::Edit::Attributes::SoftMin, 0.0f)
    ->Attribute(AZ::Edit::Attributes::SoftMax, 1.0f)
    ->Attribute(AZ::Edit::Attributes::ChangeNotify, Edit::PropertyRefreshLevels::ValuesOnly)

->DataElement(
    AZ::Edit::UIHandlers::Slider,
    &CONFIG_COMPONENT_NAME::m_ambientIntensity,
    "Ambient Intensity",
    "Scale applied to the IBL diffuse ambient contribution sampled from the scene's environment cubemap")
    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
    ->Attribute(AZ::Edit::Attributes::Max, 1.0f)
    ->Attribute(AZ::Edit::Attributes::SoftMin, 0.0f)
    ->Attribute(AZ::Edit::Attributes::SoftMax, 1.0f)
    ->Attribute(AZ::Edit::Attributes::ChangeNotify, Edit::PropertyRefreshLevels::ValuesOnly)

->DataElement(
    AZ::Edit::UIHandlers::Color,
    &CONFIG_COMPONENT_NAME::m_emissiveColor,
    "Emissive Color",
    "Self-illumination color. The fog emits this color proportionally to its density.")
    ->Attribute(AZ::Edit::Attributes::ChangeNotify, Edit::PropertyRefreshLevels::ValuesOnly)

->DataElement(
    AZ::Edit::UIHandlers::Slider,
    &CONFIG_COMPONENT_NAME::m_emissiveIntensity,
    "Emissive Intensity",
    "Scale applied to the emissive color. Values above 1 produce HDR emission.")
    ->Attribute(AZ::Edit::Attributes::Min, 0.0f)
    ->Attribute(AZ::Edit::Attributes::Max, 100.0f)
    ->Attribute(AZ::Edit::Attributes::SoftMin, 0.0f)
    ->Attribute(AZ::Edit::Attributes::SoftMax, 10.0f)
    ->Attribute(AZ::Edit::Attributes::ChangeNotify, Edit::PropertyRefreshLevels::ValuesOnly)
