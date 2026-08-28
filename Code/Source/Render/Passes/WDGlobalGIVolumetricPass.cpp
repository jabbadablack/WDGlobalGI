/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#include <Render/Passes/WDGlobalGIVolumetricPass.h>
#include <Render/WDGlobalGIFeatureProcessor.h>

#include <Atom/RHI/FrameGraphInterface.h>
#include <Atom/RHI/FrameGraphAttachmentInterface.h>
#include <Atom/RHI/FrameGraphCompileContext.h>

namespace AZ
{
    namespace Render
    {
        RPI::Ptr<WDGlobalGIVolumetricPass> WDGlobalGIVolumetricPass::Create(const RPI::PassDescriptor& descriptor)
        {
            return aznew WDGlobalGIVolumetricPass(descriptor);
        }

        WDGlobalGIVolumetricPass::WDGlobalGIVolumetricPass(const RPI::PassDescriptor& descriptor)
            : WDGlobalGIComputePass(descriptor, "Shaders/WDGlobalGI/WDGlobalGIVolumetric.azshader")
        {
        }

        bool WDGlobalGIVolumetricPass::IsEnabled() const
        {
            if (!WDGlobalGIComputePass::IsEnabled())
            {
                return false;
            }
            WDGlobalGIFeatureProcessor* fp = GetFeatureProcessor();
            return fp && fp->GetUseVolumetric();
        }

        void WDGlobalGIVolumetricPass::SetupGIFrameGraph(RHI::FrameGraphInterface frameGraph)
        {
            WDGlobalGIFeatureProcessor* fp = GetFeatureProcessor();
            if (!fp)
            {
                return;
            }

            const auto& clipmap = fp->GetClipmap();
            WDGlobalGIImportAttachment(frameGraph, clipmap.GetIrradianceClipmap(), WDGlobalGIClipmap::GetIrradianceViewDescriptor(), RHI::ScopeAttachmentAccess::Read);
            WDGlobalGIImportAttachment(frameGraph, clipmap.GetVolumetricFroxel(), WDGlobalGIClipmap::GetVolumetricFroxelViewDescriptor(), RHI::ScopeAttachmentAccess::ReadWrite);
        }

        void WDGlobalGIVolumetricPass::BindGIResources([[maybe_unused]] const RHI::FrameGraphCompileContext& context, RPI::ShaderResourceGroup* srg)
        {
            WDGlobalGIFeatureProcessor* fp = GetFeatureProcessor();
            if (!fp)
            {
                return;
            }

            const auto& clipmap = fp->GetClipmap();
            WDGlobalGISetImage(srg, "m_irradianceClipmap", clipmap.GetIrradianceClipmap());
            WDGlobalGISetImage(srg, "m_volumetricFroxel", clipmap.GetVolumetricFroxel());

            const WDGlobalGIConfiguration& config = fp->GetConfiguration();
            WDGlobalGISetConstant(srg, "m_volumetricDensity", &config.m_volumetricDensity, sizeof(float));
            WDGlobalGISetConstant(srg, "m_volumetricIntensity", &config.m_volumetricIntensity, sizeof(float));
            WDGlobalGISetConstant(srg, "m_volumetricMaxDistance", &config.m_volumetricMaxDistance, sizeof(float));
        }

        void WDGlobalGIVolumetricPass::GetDispatchThreadCount(uint32_t& x, uint32_t& y, uint32_t& z) const
        {
            x = WDGlobalGILimits::VolumetricFroxelX;
            y = WDGlobalGILimits::VolumetricFroxelY;
            z = 1;
        }
    } // namespace Render
} // namespace AZ
