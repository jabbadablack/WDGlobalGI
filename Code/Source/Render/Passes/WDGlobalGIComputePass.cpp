/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Render/Passes/WDGlobalGIComputePass.h>
#include <Render/WDGlobalGIFeatureProcessor.h>

#include <Atom/RHI/FrameGraphAttachmentInterface.h>
#include <Atom/RHI/FrameGraphInterface.h>
#include <Atom/RHI/FrameGraphCompileContext.h>
#include <Atom/RHI/FrameGraphExecuteContext.h>
#include <Atom/RPI.Public/Pass/PassFilter.h>
#include <Atom/RPI.Public/Pass/PassSystemInterface.h>
#include <Atom/RPI.Public/RenderPipeline.h>
#include <Atom/RPI.Public/RPIUtils.h>
#include <Atom/RPI.Public/Scene.h>

namespace AZ
{
    namespace Render
    {
        void WDGlobalGIImportAttachment(
            RHI::FrameGraphInterface frameGraph,
            const Data::Instance<RPI::AttachmentImage>& image,
            const RHI::ImageViewDescriptor& viewDesc,
            RHI::ScopeAttachmentAccess access,
            RHI::ScopeAttachmentStage stage)
        {
            if (!image)
            {
                return;
            }
            const RHI::AttachmentId id = image->GetAttachmentId();
            if (!frameGraph.GetAttachmentDatabase().IsAttachmentValid(id))
            {
                frameGraph.GetAttachmentDatabase().ImportImage(id, image->GetRHIImage());
            }
            RHI::ImageScopeAttachmentDescriptor desc;
            desc.m_attachmentId = id;
            desc.m_imageViewDescriptor = viewDesc;
            desc.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Load;
            frameGraph.UseShaderAttachment(desc, access, stage);
        }

        void WDGlobalGIImportAttachment(
            RHI::FrameGraphInterface frameGraph,
            const RHI::AttachmentId& attachmentId,
            const RHI::ImageViewDescriptor& viewDesc,
            RHI::ScopeAttachmentAccess access,
            RHI::ScopeAttachmentStage stage)
        {
            if (attachmentId.IsEmpty() || !frameGraph.GetAttachmentDatabase().IsAttachmentValid(attachmentId))
            {
                return;
            }
            RHI::ImageScopeAttachmentDescriptor desc;
            desc.m_attachmentId = attachmentId;
            desc.m_imageViewDescriptor = viewDesc;
            desc.m_loadStoreAction.m_loadAction = RHI::AttachmentLoadAction::Load;
            frameGraph.UseShaderAttachment(desc, access, stage);
        }

        RHI::AttachmentId WDGlobalGIComputePass::FindPipelineAttachment(const AZ::Name& passName, const AZ::Name& slotName) const
        {
            if (!m_pipeline)
            {
                return RHI::AttachmentId{};
            }
            RPI::PassFilter filter = RPI::PassFilter::CreateWithPassName(passName, m_pipeline);
            RPI::Pass* pass = RPI::PassSystemInterface::Get()->FindFirstPass(filter);
            if (!pass)
            {
                return RHI::AttachmentId{};
            }
            RPI::PassAttachmentBinding* binding = pass->FindAttachmentBinding(slotName);
            if (!binding || !binding->GetAttachment())
            {
                return RHI::AttachmentId{};
            }
            return binding->GetAttachment()->GetAttachmentId();
        }

        WDGlobalGIComputePass::WDGlobalGIComputePass(
            const RPI::PassDescriptor& descriptor, const char* shaderFilePath, bool requiresScreenProbes)
            : RPI::RenderPass(descriptor)
            , m_requiresScreenProbes(requiresScreenProbes)
        {
            LoadShader(shaderFilePath);
        }

        void WDGlobalGIComputePass::LoadShader(const char* shaderFilePath)
        {
            m_shader = RPI::LoadCriticalShader(shaderFilePath);
            if (m_shader == nullptr)
            {
                AZ_Error("WDGlobalGIComputePass", false, "Failed to load GI compute shader '%s'", shaderFilePath);
                return;
            }

            RHI::PipelineStateDescriptorForDispatch pipelineStateDescriptor;
            const auto& shaderVariant = m_shader->GetVariant(RPI::ShaderAsset::RootShaderVariantStableId);
            shaderVariant.ConfigurePipelineState(pipelineStateDescriptor);
            m_pipelineState = m_shader->AcquirePipelineState(pipelineStateDescriptor);

            m_srgLayout = m_shader->FindShaderResourceGroupLayout(RPI::SrgBindingSlot::Pass);
            if (m_srgLayout)
            {
                m_passSrg = RPI::ShaderResourceGroup::Create(
                    m_shader->GetAsset(), m_shader->GetSupervariantIndex(), m_srgLayout->GetName());
            }

            const auto outcome = RPI::GetComputeShaderNumThreads(m_shader->GetAsset(), m_dispatchArgs);
            if (!outcome.IsSuccess())
            {
                AZ_Error("WDGlobalGIComputePass", false, "Shader '%s' has invalid numthreads:\n%s",
                    shaderFilePath, outcome.GetError().c_str());
            }
        }

        WDGlobalGIFeatureProcessor* WDGlobalGIComputePass::GetFeatureProcessor() const
        {
            RPI::Scene* scene = m_pipeline ? m_pipeline->GetScene() : nullptr;
            return scene ? scene->GetFeatureProcessor<WDGlobalGIFeatureProcessor>() : nullptr;
        }

        const RHI::ImageView* WDGlobalGIComputePass::GetInputImageView(
            const RHI::FrameGraphCompileContext& context, const AZ::Name& slotName)
        {
            RPI::PassAttachmentBinding* binding = FindAttachmentBinding(slotName);
            if (!binding || !binding->GetAttachment())
            {
                return nullptr;
            }
            return context.GetImageView(binding->GetAttachment()->GetAttachmentId());
        }

        bool WDGlobalGIComputePass::IsEnabled() const
        {
            if (!RenderPass::IsEnabled())
            {
                return false;
            }

            // Never run (and therefore never declare attachments) if the shader failed to load.
            if (!m_passSrg || !m_pipelineState)
            {
                return false;
            }

            WDGlobalGIFeatureProcessor* fp = GetFeatureProcessor();
            if (!fp || !fp->ShouldRender())
            {
                return false;
            }
            return !m_requiresScreenProbes || fp->GetUseScreenProbes();
        }

        void WDGlobalGIComputePass::SetupFrameGraphDependencies(RHI::FrameGraphInterface frameGraph)
        {
            // Declare scope attachments for the connected pipeline slots (G-buffer, lighting target).
            RenderPass::SetupFrameGraphDependencies(frameGraph);
            frameGraph.SetEstimatedItemCount(1);

            // Let the subclass import and declare the clipmap attachments it touches.
            SetupGIFrameGraph(frameGraph);
        }

        void WDGlobalGIComputePass::CompileResources(const RHI::FrameGraphCompileContext& context)
        {
            WDGlobalGIFeatureProcessor* fp = GetFeatureProcessor();
            if (!fp || !m_passSrg)
            {
                return;
            }

            // Shared addressing / lighting / tuning constants.
            fp->GetClipmap().FillSharedConstants(m_passSrg.get());

            // Pass-specific resource views.
            BindGIResources(context, m_passSrg.get());

            if (!m_passSrg->IsQueuedForCompile())
            {
                m_passSrg->Compile();
            }

            // Build the dispatch item.
            uint32_t threadsX = 1, threadsY = 1, threadsZ = 1;
            GetDispatchThreadCount(threadsX, threadsY, threadsZ);

            RHI::DispatchDirect arguments = m_dispatchArgs;
            arguments.m_totalNumberOfThreadsX = threadsX;
            arguments.m_totalNumberOfThreadsY = threadsY;
            arguments.m_totalNumberOfThreadsZ = threadsZ;

            m_dispatchItem.SetPipelineState(m_pipelineState);
            m_dispatchItem.SetArguments(arguments);
        }

        void WDGlobalGIComputePass::BuildCommandListInternal(const RHI::FrameGraphExecuteContext& context)
        {
            if (!m_passSrg)
            {
                return;
            }

            const uint32_t deviceIndex = context.GetDeviceIndex();
            RHI::CommandList* commandList = context.GetCommandList();

            commandList->SetShaderResourceGroupForDispatch(
                *m_passSrg->GetRHIShaderResourceGroup()->GetDeviceShaderResourceGroup(deviceIndex));
            commandList->Submit(m_dispatchItem.GetDeviceDispatchItem(deviceIndex), 0);
        }
    } // namespace Render
} // namespace AZ
