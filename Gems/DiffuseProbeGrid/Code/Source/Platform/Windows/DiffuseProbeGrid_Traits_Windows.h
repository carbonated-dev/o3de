/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */
#pragma once

#if defined(CARBONATED)
//supress Diffuse GI as workaround until diffuse issue is fixed
#define AZ_TRAIT_DIFFUSE_GI_PASSES_SUPPORTED 0
#else
#define AZ_TRAIT_DIFFUSE_GI_PASSES_SUPPORTED 1
#endif