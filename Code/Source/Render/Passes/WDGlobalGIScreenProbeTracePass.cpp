/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Render/Passes/WDGlobalGIScreenProbeTracePass.h>
#include <Render/WDGlobalGIFeatureProcessor.h>

#include <AzCore/Math/MathUtils.h>
#include <Atom/RHI/FrameGraphInterface.h>
#include <Atom/RHI/FrameGraphAttachmentInterface.h>
#include <Atom/RHI/FrameGraphCompileContext.h>

namespace AZ
{
    namespace Render
    {
        RPI::Ptr<WDGlobalGIScreenProbeTracePass> WDGlobalGIScreenProbeTracePass::Create(const RPI::PassDescriptor& descriptor)
        {
            return aznew WDGlobalGIScreenProbeTracePass(descriptor);
        }

        WDGlobalGIScreenProbeTracePass::WDGlobalGIScreenProbeTracePass(const RPI::PassDescriptor& descriptor)
            : WDGlobalGIComputePass(descriptor, "Shaders/WDGlobalGI/WDGlobalGIScreenProbeTrace.azshader", /*requiresScreenProbes=*/true)
        {
        }

        void WDGlobalGIScreenProbeTracePass::SetupGIFrameGraph(RHI::FrameGraphInterface frameGraph)
        {
            WDGlobalGIFeatureProcessor* fp = GetFeatureProcessor();
            if (!fp)
            {
                return;
            }

            const uint32_t frameIndex = fp->GetClipmap().GetShaderConstants().m_frameIndex;
            const RHI::AttachmentId motionId = FindPipelineAttachment(AZ::Name("MotionVectorPass"), AZ::Name("MotionVectorOutput"));
            WDGlobalGIImportAttachment(frameGraph, motionId, RHI::ImageViewDescriptor(), RHI::ScopeAttachmentAccess::Read);
            WDGlobalGIImportAttachment(frameGraph, fp->GetClipmap().GetScreenProbeDataCurrent(frameIndex), WDGlobalGIClipmap::GetScreenProbeDataViewDescriptor(), RHI::ScopeAttachmentAccess::Read);
            WDGlobalGIImportAttachment(frameGraph, fp->GetClipmap().GetRadianceClipmap(), WDGlobalGIClipmap::GetRadianceViewDescriptor(), RHI::ScopeAttachmentAccess::Read);
            WDGlobalGIImportAttachment(frameGraph, fp->GetClipmap().GetSdfClipmap(), WDGlobalGIClipmap::GetSdfViewDescriptor(), RHI::ScopeAttachmentAccess::Read);
            WDGlobalGIImportAttachment(frameGraph, fp->GetClipmap().GetVoxelNormalClipmap(), WDGlobalGIClipmap::GetVoxelNormalViewDescriptor(), RHI::ScopeAttachmentAccess::Read);
            WDGlobalGIImportAttachment(frameGraph, fp->GetClipmap().GetAnisoRadianceClipmap(), WDGlobalGIClipmap::GetAnisoRadianceViewDescriptor(), RHI::ScopeAttachmentAccess::Read);
            WDGlobalGIImportAttachment(frameGraph, fp->GetClipmap().GetIrradianceClipmap(), WDGlobalGIClipmap::GetIrradianceViewDescriptor(), RHI::ScopeAttachmentAccess::Read);
            // Ping-pong: write the "current" atlas, read the "previous" one (reprojected) for history.
            WDGlobalGIImportAttachment(frameGraph, fp->GetClipmap().GetScreenProbeAtlasCurrent(frameIndex), WDGlobalGIClipmap::GetScreenProbeAtlasViewDescriptor(), RHI::ScopeAttachmentAccess::ReadWrite);
            WDGlobalGIImportAttachment(frameGraph, fp->GetClipmap().GetScreenProbeAtlasPrev(frameIndex), WDGlobalGIClipmap::GetScreenProbeAtlasViewDescriptor(), RHI::ScopeAttachmentAccess::Read);
            WDGlobalGIImportAttachment(frameGraph, fp->GetClipmap().GetScreenProbeDataPrev(frameIndex), WDGlobalGIClipmap::GetScreenProbeDataViewDescriptor(), RHI::ScopeAttachmentAccess::Read);
        }

        void WDGlobalGIScreenProbeTracePass::BindGIResources(const RHI::FrameGraphCompileContext& context, RPI::ShaderResourceGroup* srg)
        {
            WDGlobalGIFeatureProcessor* fp = GetFeatureProcessor();
            if (!fp)
            {
                return;
            }

            const uint32_t frameIndex = fp->GetClipmap().GetShaderConstants().m_frameIndex;
            WDGlobalGISetImage(srg, "m_depth", GetInputImageView(context, AZ::Name("Depth")));
            const RHI::AttachmentId motionId = FindPipelineAttachment(AZ::Name("MotionVectorPass"), AZ::Name("MotionVectorOutput"));
            const RHI::ImageView* motionView = motionId.IsEmpty() ? nullptr : context.GetImageView(motionId);
            WDGlobalGISetImage(srg, "m_motionVector", motionView);
            const uint32_t hasMotionVector = motionView ? 1u : 0u;
            WDGlobalGISetConstant(srg, "m_hasMotionVector", &hasMotionVector, sizeof(hasMotionVector));
            WDGlobalGISetImage(srg, "m_screenProbeData", fp->GetClipmap().GetScreenProbeDataCurrent(frameIndex));
            const auto& radiance = fp->GetClipmap().GetRadianceClipmap();
            const auto& sdf = fp->GetClipmap().GetSdfClipmap();
            const auto& voxelNormal = fp->GetClipmap().GetVoxelNormalClipmap();
            const auto& anisoRadiance = fp->GetClipmap().GetAnisoRadianceClipmap();
            const auto& irradiance = fp->GetClipmap().GetIrradianceClipmap();
            const auto& atlasCurrent = fp->GetClipmap().GetScreenProbeAtlasCurrent(frameIndex);
            const auto& atlasPrev = fp->GetClipmap().GetScreenProbeAtlasPrev(frameIndex);
            WDGlobalGISetImage(srg, "m_radianceClipmap", radiance ? radiance->GetImageView() : nullptr);
            WDGlobalGISetImage(srg, "m_sdfClipmap", sdf ? sdf->GetImageView() : nullptr);
            WDGlobalGISetImage(srg, "m_voxelNormalClipmap", voxelNormal ? voxelNormal->GetImageView() : nullptr);
            WDGlobalGISetImage(srg, "m_anisoRadianceClipmap", anisoRadiance ? anisoRadiance->GetImageView() : nullptr);
            WDGlobalGISetImage(srg, "m_irradianceClipmap", irradiance ? irradiance->GetImageView() : nullptr);
            WDGlobalGISetImage(srg, "m_screenProbeAtlas", atlasCurrent ? atlasCurrent->GetImageView() : nullptr);
            WDGlobalGISetImage(srg, "m_screenProbeHistory", atlasPrev ? atlasPrev->GetImageView() : nullptr);
            WDGlobalGISetImage(srg, "m_screenProbeDataHistory", fp->GetClipmap().GetScreenProbeDataPrev(frameIndex));

            // Screen resolution -> probe grid; the tile size grows if the frame needs more probes than fit.
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
            const float temporal = fp->GetScreenProbeTemporal();
            WDGlobalGISetConstant(srg, "m_screenProbeTemporal", &temporal, sizeof(temporal));
        }

        void WDGlobalGIScreenProbeTracePass::GetDispatchThreadCount(uint32_t& x, uint32_t& y, uint32_t& z) const
        {
            x = AZ::GetMax(m_tilesX * WDGlobalGILimits::ScreenProbeOctaRes, 1u);
            y = AZ::GetMax(m_tilesY * WDGlobalGILimits::ScreenProbeOctaRes, 1u);
            z = 1;
        }
    } // namespace Render
} // namespace AZ
