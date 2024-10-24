/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#if defined(CARBONATED)
#include <AzCore/PlatformDef.h>
#endif

#include <Atom_RHI_Metal_Platform.h>

@interface RHIMetalViewController : NativeViewControllerType {}
- (BOOL)prefersStatusBarHidden;
#if defined(CARBONATED) && defined(AZ_PLATFORM_IOS)
- (UIRectEdge)preferredScreenEdgesDeferringSystemGestures;
#endif
- (void)windowWillClose:(NSNotification *)notification;
- (void)windowDidResize:(NSNotification *)notification;
@end    


