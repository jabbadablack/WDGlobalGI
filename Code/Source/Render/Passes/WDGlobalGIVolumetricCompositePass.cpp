/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Render/Passes/WDGlobalGIVolumetricCompositePass.h>
#include <Render/WDGlobalGIFeatureProcessor.h>

#include <Atom/RHI/FrameGraphInterface.h>
#include <Atom/RHI/FrameGraphAttachmentInterface.h>
#include <Atom/RHI/FrameGraphCompileContext.h>
#include <Atom/RPI.Public/RenderPipeline.h>
#include <Atom/RPI.Public/Scene.h>
#include <Atom/RPI.Public/Shader/ShaderResourceGroup.h>

namespace AZ
{
    namespace Render
    {
        RPI::Ptr<WDGlobalGIVolumetricCompositePass> WDGlobalGIVolumetricCompositePass::Create(const RPI::PassDescriptor& descriptor)
        {
            return aznew WDGlobalGIVolumetricCompositePass(descriptor);
        }

        WDGlobalGIVolumetricCompositePass::WDGlobalGIVolumetricCompositePass(const RPI::PassDescriptor& descriptor)
            : RPI::FullscreenTrianglePass(descriptor)
        {
        }

        WDGlobalGIFeatureProcessor* WDGlobalGIVolumetricCompositePass::GetFeatureProcessor() const
        {
            RPI::Scene* scene = m_pipeline ? m_pipeline->GetScene() : nullptr;
            return scene ? scene->GetFeatureProcessor<WDGlobalGIFeatureProcessor>() : nullptr;
        }

        bool WDGlobalGIVolumetricCompositePass::IsEnabled() const
        {
            if (!FullscreenTrianglePass::IsEnabled())
            {
                return false;
            }
            WDGlobalGIFeatureProcessor* fp = GetFeatureProcessor();
            return fp && fp->ShouldRender() && fp->GetUseVolumetric();
        }

        void WDGlobalGIVolumetricCompositePass::SetupFrameGraphDependencies(RHI::FrameGraphInterface frameGraph)
        {
            FullscreenTrianglePass::SetupFrameGraphDependencies(frameGraph);

            WDGlobalGIFeatureProcessor* fp = GetFeatureProcessor();
            if (!fp)
            {
                return;
            }

            const auto& froxel = fp->GetClipmap().GetVolumetricFroxel();
            if (!froxel)
            {
                return;
            }

            const RHI::AttachmentId id = froxel->GetAttachmentId();
            if (!frameGraph.GetAttachmentDatabase().IsAttachmentValid(id))
            {
                frameGraph.GetAttachmentDatabase().ImportImage(id, froxel->GetRHIImage());
            }
            RHI::ImageScopeAttachmentDescriptor desc;
            desc.m_attachmentId = id;
            desc.m_imageViewDescriptor = WDGlobalGIClipmap::GetVolumetricFroxelViewDescriptor();
            desc.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Load;
            frameGraph.UseShaderAttachment(desc, RHI::ScopeAttachmentAccess::Read, RHI::ScopeAttachmentStage::FragmentShader);
        }

        void WDGlobalGIVolumetricCompositePass::CompileResources(const RHI::FrameGraphCompileContext& context)
        {
            WDGlobalGIFeatureProcessor* fp = GetFeatureProcessor();
            Data::Instance<RPI::ShaderResourceGroup> srg = GetShaderResourceGroup();

            if (fp && srg)
            {
                fp->GetClipmap().FillSharedConstants(srg.get());

                WDGlobalGISetImage(srg.get(), "m_volumetricFroxel", fp->GetClipmap().GetVolumetricFroxel());

                const float maxDist = fp->GetConfiguration().m_volumetricMaxDistance;
                WDGlobalGISetConstant(srg.get(), "m_volumetricMaxDistance", &maxDist, sizeof(maxDist));
            }

            FullscreenTrianglePass::CompileResources(context);
        }
    } // namespace Render
} // namespace AZ
