/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Render/Passes/WDGlobalGIScreenProbeIntegratePass.h>
#include <Render/Passes/WDGlobalGIComputePass.h>
#include <Render/WDGlobalGIFeatureProcessor.h>

#include <AzCore/Math/MathUtils.h>
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
        RPI::Ptr<WDGlobalGIScreenProbeIntegratePass> WDGlobalGIScreenProbeIntegratePass::Create(const RPI::PassDescriptor& descriptor)
        {
            return aznew WDGlobalGIScreenProbeIntegratePass(descriptor);
        }

        WDGlobalGIScreenProbeIntegratePass::WDGlobalGIScreenProbeIntegratePass(const RPI::PassDescriptor& descriptor)
            : RPI::FullscreenTrianglePass(descriptor)
        {
        }

        WDGlobalGIFeatureProcessor* WDGlobalGIScreenProbeIntegratePass::GetFeatureProcessor() const
        {
            RPI::Scene* scene = m_pipeline ? m_pipeline->GetScene() : nullptr;
            return scene ? scene->GetFeatureProcessor<WDGlobalGIFeatureProcessor>() : nullptr;
        }

        bool WDGlobalGIScreenProbeIntegratePass::IsEnabled() const
        {
            if (!FullscreenTrianglePass::IsEnabled())
            {
                return false;
            }
            WDGlobalGIFeatureProcessor* fp = GetFeatureProcessor();
            return fp && fp->ShouldRender() && fp->GetUseScreenProbes();
        }

        void WDGlobalGIScreenProbeIntegratePass::SetupFrameGraphDependencies(RHI::FrameGraphInterface frameGraph)
        {
            FullscreenTrianglePass::SetupFrameGraphDependencies(frameGraph);

            WDGlobalGIFeatureProcessor* fp = GetFeatureProcessor();
            if (!fp)
            {
                return;
            }

            const uint32_t frameIndex = fp->GetClipmap().GetShaderConstants().m_frameIndex;
            WDGlobalGIImportAttachment(frameGraph, fp->GetClipmap().GetScreenProbeDataCurrent(frameIndex), WDGlobalGIClipmap::GetScreenProbeDataViewDescriptor(), RHI::ScopeAttachmentAccess::Read, RHI::ScopeAttachmentStage::FragmentShader);
            WDGlobalGIImportAttachment(frameGraph, fp->GetClipmap().GetScreenProbeSHBlurred(), WDGlobalGIClipmap::GetScreenProbeShViewDescriptor(), RHI::ScopeAttachmentAccess::Read, RHI::ScopeAttachmentStage::FragmentShader);
            WDGlobalGIImportAttachment(frameGraph, fp->GetClipmap().GetScreenProbeAtlasCurrent(frameIndex), WDGlobalGIClipmap::GetScreenProbeAtlasViewDescriptor(), RHI::ScopeAttachmentAccess::Read, RHI::ScopeAttachmentStage::FragmentShader);
            WDGlobalGIImportAttachment(frameGraph, fp->GetClipmap().GetScreenProbeDepthAtlas(), WDGlobalGIClipmap::GetScreenProbeAtlasViewDescriptor(), RHI::ScopeAttachmentAccess::Read, RHI::ScopeAttachmentStage::FragmentShader);
        }

        void WDGlobalGIScreenProbeIntegratePass::CompileResources(const RHI::FrameGraphCompileContext& context)
        {
            WDGlobalGIFeatureProcessor* fp = GetFeatureProcessor();
            Data::Instance<RPI::ShaderResourceGroup> srg = GetShaderResourceGroup();

            if (fp && srg)
            {
                fp->GetClipmap().FillSharedConstants(srg.get());


                const uint32_t frameIndex = fp->GetClipmap().GetShaderConstants().m_frameIndex;
                WDGlobalGISetImage(srg.get(), "m_screenProbeData", fp->GetClipmap().GetScreenProbeDataCurrent(frameIndex));
                WDGlobalGISetImage(srg.get(), "m_screenProbeSH", fp->GetClipmap().GetScreenProbeSHBlurred()); // denoised SH
                WDGlobalGISetImage(srg.get(), "m_screenProbeAtlas", fp->GetClipmap().GetScreenProbeAtlasCurrent(frameIndex));
                WDGlobalGISetImage(srg.get(), "m_screenProbeDepthAtlas", fp->GetClipmap().GetScreenProbeDepthAtlas());

                const float specular = fp->GetScreenProbeSpecular();
                WDGlobalGISetConstant(srg.get(), "m_screenProbeSpecular", &specular, sizeof(specular));

                const float specularRoughness = fp->GetConfiguration().m_specularRoughness;
                WDGlobalGISetConstant(srg.get(), "m_specularRoughness", &specularRoughness, sizeof(specularRoughness));

                // Screen resolution -> probe grid; ComputeScreenProbeGrid keeps every pass in agreement.
                uint32_t screenW = 1, screenH = 1;
                if (RPI::PassAttachmentBinding* binding = FindAttachmentBinding(AZ::Name("Depth")); binding && binding->GetAttachment())
                {
                    const RHI::ImageDescriptor& imageDesc = binding->GetAttachment()->m_descriptor.m_image;
                    screenW = imageDesc.m_size.m_width;
                    screenH = imageDesc.m_size.m_height;
                }
                WDGlobalGIClipmap::FillScreenProbeInfo(srg.get(), screenW, screenH);
            }

            FullscreenTrianglePass::CompileResources(context);
        }
    } // namespace Render
} // namespace AZ
