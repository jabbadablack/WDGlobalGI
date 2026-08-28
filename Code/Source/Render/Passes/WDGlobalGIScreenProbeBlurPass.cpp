/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Render/Passes/WDGlobalGIScreenProbeBlurPass.h>
#include <Render/WDGlobalGIFeatureProcessor.h>

#include <AzCore/Math/MathUtils.h>
#include <Atom/RHI/FrameGraphInterface.h>
#include <Atom/RHI/FrameGraphAttachmentInterface.h>
#include <Atom/RHI/FrameGraphCompileContext.h>

namespace AZ
{
    namespace Render
    {
        RPI::Ptr<WDGlobalGIScreenProbeBlurPass> WDGlobalGIScreenProbeBlurPass::Create(const RPI::PassDescriptor& descriptor)
        {
            return aznew WDGlobalGIScreenProbeBlurPass(descriptor);
        }

        WDGlobalGIScreenProbeBlurPass::WDGlobalGIScreenProbeBlurPass(const RPI::PassDescriptor& descriptor)
            : WDGlobalGIComputePass(descriptor, "Shaders/WDGlobalGI/WDGlobalGIScreenProbeBlur.azshader", /*requiresScreenProbes=*/true)
        {
        }

        void WDGlobalGIScreenProbeBlurPass::SetupGIFrameGraph(RHI::FrameGraphInterface frameGraph)
        {
            WDGlobalGIFeatureProcessor* fp = GetFeatureProcessor();
            if (!fp)
            {
                return;
            }

            const uint32_t frameIndex = fp->GetClipmap().GetShaderConstants().m_frameIndex;
            WDGlobalGIImportAttachment(frameGraph, fp->GetClipmap().GetScreenProbeDataCurrent(frameIndex), WDGlobalGIClipmap::GetScreenProbeDataViewDescriptor(), RHI::ScopeAttachmentAccess::Read);
            WDGlobalGIImportAttachment(frameGraph, fp->GetClipmap().GetScreenProbeAtlasCurrent(frameIndex), WDGlobalGIClipmap::GetScreenProbeAtlasViewDescriptor(), RHI::ScopeAttachmentAccess::Read);
            WDGlobalGIImportAttachment(frameGraph, fp->GetClipmap().GetScreenProbeRadianceFiltered(), WDGlobalGIClipmap::GetScreenProbeAtlasViewDescriptor(), RHI::ScopeAttachmentAccess::ReadWrite);
        }

        void WDGlobalGIScreenProbeBlurPass::BindGIResources(const RHI::FrameGraphCompileContext& context, RPI::ShaderResourceGroup* srg)
        {
            WDGlobalGIFeatureProcessor* fp = GetFeatureProcessor();
            if (!fp)
            {
                return;
            }

            const uint32_t frameIndex = fp->GetClipmap().GetShaderConstants().m_frameIndex;
            WDGlobalGISetImage(srg, "m_screenProbeData", fp->GetClipmap().GetScreenProbeDataCurrent(frameIndex));
            WDGlobalGISetImage(srg, "m_screenProbeAtlas", fp->GetClipmap().GetScreenProbeAtlasCurrent(frameIndex));
            WDGlobalGISetImage(srg, "m_screenProbeRadianceFiltered", fp->GetClipmap().GetScreenProbeRadianceFiltered());

            // Screen resolution -> probe grid; ComputeScreenProbeGrid keeps every pass in agreement.
            uint32_t screenW = 1, screenH = 1;
            if (RPI::PassAttachmentBinding* binding = FindAttachmentBinding(AZ::Name("Depth")); binding && binding->GetAttachment())
            {
                const RHI::ImageDescriptor& imageDesc = binding->GetAttachment()->m_descriptor.m_image;
                screenW = imageDesc.m_size.m_width;
                screenH = imageDesc.m_size.m_height;
            }

            const WDGlobalGIClipmap::ScreenProbeGrid grid = WDGlobalGIClipmap::FillScreenProbeInfo(srg, screenW, screenH);
            m_tilesX = grid.m_tilesX;
            m_tilesY = grid.TotalRows();
            const float blur = fp->GetConfiguration().m_screenProbeBlur;
            WDGlobalGISetConstant(srg, "m_screenProbeBlur", &blur, sizeof(blur));
        }

        void WDGlobalGIScreenProbeBlurPass::GetDispatchThreadCount(uint32_t& x, uint32_t& y, uint32_t& z) const
        {
            x = AZ::GetMax(m_tilesX * WDGlobalGILimits::ScreenProbeOctaRes, 1u);
            y = AZ::GetMax(m_tilesY * WDGlobalGILimits::ScreenProbeOctaRes, 1u);
            z = 1;
        }
    } // namespace Render
} // namespace AZ
