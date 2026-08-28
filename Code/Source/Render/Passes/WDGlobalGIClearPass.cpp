/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Render/Passes/WDGlobalGIClearPass.h>
#include <Render/WDGlobalGIFeatureProcessor.h>

#include <Atom/RHI/FrameGraphInterface.h>
#include <Atom/RHI/FrameGraphAttachmentInterface.h>
#include <Atom/RHI/FrameGraphCompileContext.h>

namespace AZ
{
    namespace Render
    {
        RPI::Ptr<WDGlobalGIClearPass> WDGlobalGIClearPass::Create(const RPI::PassDescriptor& descriptor)
        {
            return aznew WDGlobalGIClearPass(descriptor);
        }

        WDGlobalGIClearPass::WDGlobalGIClearPass(const RPI::PassDescriptor& descriptor)
            : WDGlobalGIComputePass(descriptor, "Shaders/WDGlobalGI/WDGlobalGIClear.azshader")
        {
        }

        void WDGlobalGIClearPass::SetupGIFrameGraph(RHI::FrameGraphInterface frameGraph)
        {
            WDGlobalGIFeatureProcessor* fp = GetFeatureProcessor();
            if (!fp)
            {
                return;
            }

            const auto& clipmap = fp->GetClipmap();
            WDGlobalGIImportAttachment(frameGraph, clipmap.GetRadianceClipmap(), WDGlobalGIClipmap::GetRadianceViewDescriptor(), RHI::ScopeAttachmentAccess::ReadWrite);
            WDGlobalGIImportAttachment(frameGraph, clipmap.GetIrradianceClipmap(), WDGlobalGIClipmap::GetIrradianceViewDescriptor(), RHI::ScopeAttachmentAccess::ReadWrite);
            WDGlobalGIImportAttachment(frameGraph, clipmap.GetSdfClipmap(), WDGlobalGIClipmap::GetSdfViewDescriptor(), RHI::ScopeAttachmentAccess::ReadWrite);
            WDGlobalGIImportAttachment(frameGraph, clipmap.GetVoxelNormalClipmap(), WDGlobalGIClipmap::GetVoxelNormalViewDescriptor(), RHI::ScopeAttachmentAccess::ReadWrite);
            WDGlobalGIImportAttachment(frameGraph, clipmap.GetAlbedoClipmap(), WDGlobalGIClipmap::GetAlbedoViewDescriptor(), RHI::ScopeAttachmentAccess::ReadWrite);
            WDGlobalGIImportAttachment(frameGraph, clipmap.GetAnisoRadianceClipmap(), WDGlobalGIClipmap::GetAnisoRadianceViewDescriptor(), RHI::ScopeAttachmentAccess::ReadWrite);
        }

        void WDGlobalGIClearPass::BindGIResources([[maybe_unused]] const RHI::FrameGraphCompileContext& context, RPI::ShaderResourceGroup* srg)
        {
            WDGlobalGIFeatureProcessor* fp = GetFeatureProcessor();
            if (!fp)
            {
                return;
            }

            const auto& clipmap = fp->GetClipmap();
            WDGlobalGISetImage(srg, "m_radianceClipmap", clipmap.GetRadianceClipmap());
            WDGlobalGISetImage(srg, "m_irradianceClipmap", clipmap.GetIrradianceClipmap());
            WDGlobalGISetImage(srg, "m_sdfClipmap", clipmap.GetSdfClipmap());
            WDGlobalGISetImage(srg, "m_voxelNormalClipmap", clipmap.GetVoxelNormalClipmap());
            WDGlobalGISetImage(srg, "m_albedoClipmap", clipmap.GetAlbedoClipmap());
            WDGlobalGISetImage(srg, "m_anisoRadianceClipmap", clipmap.GetAnisoRadianceClipmap());
        }

        void WDGlobalGIClearPass::GetDispatchThreadCount(uint32_t& x, uint32_t& y, uint32_t& z) const
        {
            uint32_t cascadeCount = 1;
            if (WDGlobalGIFeatureProcessor* fp = GetFeatureProcessor())
            {
                cascadeCount = fp->GetClipmap().GetShaderConstants().m_cascadeCount;
            }
            x = WDGlobalGILimits::GridResolutionX;
            y = WDGlobalGILimits::GridResolutionY;
            z = WDGlobalGILimits::GridResolutionZ * cascadeCount;
        }
    } // namespace Render
} // namespace AZ
