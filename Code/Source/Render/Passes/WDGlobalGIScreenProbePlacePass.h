/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include <Render/Passes/WDGlobalGIComputePass.h>

namespace AZ
{
    namespace Render
    {
        //! Screen-probe placement. One thread per screen tile: picks the pixel the tile's probe sits
        //! on (re-anchoring onto last frame's world point where it can) and publishes the probe's
        //! depth, normal and in-tile offset for the trace/convert/blur/integrate passes to share.
        class WDGlobalGIScreenProbePlacePass final
            : public WDGlobalGIComputePass
        {
        public:
            AZ_RPI_PASS(WDGlobalGIScreenProbePlacePass);
            AZ_RTTI(AZ::Render::WDGlobalGIScreenProbePlacePass, "{5C6D7E8F-9A0B-41C2-D3E4-F50617283A4B}", WDGlobalGIComputePass);
            AZ_CLASS_ALLOCATOR(WDGlobalGIScreenProbePlacePass, SystemAllocator);

            static RPI::Ptr<WDGlobalGIScreenProbePlacePass> Create(const RPI::PassDescriptor& descriptor);

        private:
            explicit WDGlobalGIScreenProbePlacePass(const RPI::PassDescriptor& descriptor);

            void SetupGIFrameGraph(RHI::FrameGraphInterface frameGraph) override;
            void BindGIResources(const RHI::FrameGraphCompileContext& context, RPI::ShaderResourceGroup* srg) override;
            void GetDispatchThreadCount(uint32_t& x, uint32_t& y, uint32_t& z) const override;

            uint32_t m_tilesX = 1;
            uint32_t m_tilesY = 1;
        };
    } // namespace Render
} // namespace AZ
