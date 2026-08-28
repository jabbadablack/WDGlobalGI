/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Render/Passes/WDGlobalGIScreenProbePlacePass.h>
#include <Render/WDGlobalGIFeatureProcessor.h>

#include <AzCore/Math/MathUtils.h>
#include <Atom/RHI/FrameGraphInterface.h>
#include <Atom/RHI/FrameGraphAttachmentInterface.h>
#include <Atom/RHI/FrameGraphCompileContext.h>

namespace AZ
{
    namespace Render
    {
        RPI::Ptr<WDGlobalGIScreenProbePlacePass> WDGlobalGIScreenProbePlacePass::Create(const RPI::PassDescriptor& descriptor)
        {
            return aznew WDGlobalGIScreenProbePlacePass(descriptor);
        }

        WDGlobalGIScreenProbePlacePass::WDGlobalGIScreenProbePlacePass(const RPI::PassDescriptor& descriptor)
            : WDGlobalGIComputePass(descriptor, "Shaders/WDGlobalGI/WDGlobalGIScreenProbePlace.azshader", /*requiresScreenProbes=*/true)
        {
        }

        void WDGlobalGIScreenProbePlacePass::SetupGIFrameGraph(RHI::FrameGraphInterface frameGraph)
        {
            WDGlobalGIFeatureProcessor* fp = GetFeatureProcessor();
            if (!fp)
            {
                return;
            }

            const uint32_t frameIndex = fp->GetClipmap().GetShaderConstants().m_frameIndex;
            WDGlobalGIImportAttachment(frameGraph, fp->GetClipmap().GetScreenProbeDataCurrent(frameIndex),
                WDGlobalGIClipmap::GetScreenProbeDataViewDescriptor(), RHI::ScopeAttachmentAccess::ReadWrite);
            WDGlobalGIImportAttachment(frameGraph, fp->GetClipmap().GetScreenProbeDataPrev(frameIndex),
                WDGlobalGIClipmap::GetScreenProbeDataViewDescriptor(), RHI::ScopeAttachmentAccess::Read);
        }

        void WDGlobalGIScreenProbePlacePass::BindGIResources(const RHI::FrameGraphCompileContext& context, RPI::ShaderResourceGroup* srg)
        {
            WDGlobalGIFeatureProcessor* fp = GetFeatureProcessor();
            if (!fp)
            {
                return;
            }

            const uint32_t frameIndex = fp->GetClipmap().GetShaderConstants().m_frameIndex;
            WDGlobalGISetImage(srg, "m_depth", GetInputImageView(context, AZ::Name("Depth")));
            WDGlobalGISetImage(srg, "m_normal", GetInputImageView(context, AZ::Name("Normal")));
            WDGlobalGISetImage(srg, "m_screenProbeData", fp->GetClipmap().GetScreenProbeDataCurrent(frameIndex));
            WDGlobalGISetImage(srg, "m_screenProbeDataHistory", fp->GetClipmap().GetScreenProbeDataPrev(frameIndex));

            uint32_t screenW = 1, screenH = 1;
            if (RPI::PassAttachmentBinding* binding = FindAttachmentBinding(AZ::Name("Depth")); binding && binding->GetAttachment())
            {
                const RHI::ImageDescriptor& imageDesc = binding->GetAttachment()->m_descriptor.m_image;
                screenW = imageDesc.m_size.m_width;
                screenH = imageDesc.m_size.m_height;
            }

            const WDGlobalGIClipmap::ScreenProbeGrid grid = WDGlobalGIClipmap::FillScreenProbeInfo(srg, screenW, screenH);
            m_tilesX = grid.m_tilesX;
            m_tilesY = grid.m_tilesY;
        }

        void WDGlobalGIScreenProbePlacePass::GetDispatchThreadCount(uint32_t& x, uint32_t& y, uint32_t& z) const
        {
            x = AZ::GetMax(m_tilesX, 1u);
            y = AZ::GetMax(m_tilesY, 1u);
            z = 1;
        }
    } // namespace Render
} // namespace AZ
