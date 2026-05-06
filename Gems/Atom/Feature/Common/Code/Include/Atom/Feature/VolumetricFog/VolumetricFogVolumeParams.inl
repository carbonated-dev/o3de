/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// Macros below are of the form:
// PARAM(NAME, MEMBER_NAME, DEFAULT_VALUE, ...)

AZ_GFX_TEXTURE_ASSET_PARAM(
    NoiseTexture,
    m_noiseTexture,
    Data::Asset<AZ::RPI::StreamingImageAsset>(Data::AssetId(
        "{5AB3C089-DCE0-54EA-83AA-6118ED561449}", AZ::RPI::StreamingImageAsset::GetImageAssetSubId()), // "textures/volumetricfog/fognoise.dds.streamingimage"
        azrtti_typeid<AZ::RPI::StreamingImageAsset>())) // optional 3D tiling noise texture; when absent, noise sampling is skipped
