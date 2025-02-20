#include "../d3d11/d3d11_options.h"

#include "dxbc_options.h"

namespace dxvk {
  
  DxbcOptions::DxbcOptions() {

  }


  DxbcOptions::DxbcOptions(const Rc<DxvkDevice>& device, const D3D11Options& options) {
    const Rc<DxvkAdapter> adapter = device->adapter();

    const DxvkDeviceFeatures& devFeatures = device->features();
    const DxvkDeviceInfo& devInfo = adapter->devicePropertiesExt();

    const VkShaderStageFlags allShaderStages = device->getShaderPipelineStages();
    
    useDepthClipWorkaround
      = !devFeatures.extDepthClipEnable.depthClipEnable;
    useStorageImageReadWithoutFormat
      = devFeatures.core.features.shaderStorageImageReadWithoutFormat;
    useSubgroupOpsForAtomicCounters
      = (devInfo.coreSubgroup.supportedStages     & allShaderStages) == allShaderStages
     && (devInfo.coreSubgroup.supportedOperations & VK_SUBGROUP_FEATURE_BALLOT_BIT);
    useSubgroupOpsForEarlyDiscard
      = (devInfo.coreSubgroup.subgroupSize >= 4)
     && (devInfo.coreSubgroup.supportedStages     & VK_SHADER_STAGE_FRAGMENT_BIT)
     && (devInfo.coreSubgroup.supportedOperations & VK_SUBGROUP_FEATURE_BALLOT_BIT);
    useSdivForBufferIndex
      = adapter->matchesDriver(DxvkGpuVendor::Nvidia, VK_DRIVER_ID_NVIDIA_PROPRIETARY_KHR, 0, 0);
    
    switch (device->config().useRawSsbo) {
      case Tristate::Auto:  minSsboAlignment = devInfo.core.properties.limits.minStorageBufferOffsetAlignment; break;
      case Tristate::True:  minSsboAlignment =  4u; break;
      case Tristate::False: minSsboAlignment = ~0u; break;
    }
    
    strictDivision           = options.strictDivision;
    zeroInitWorkgroupMemory  = options.zeroInitWorkgroupMemory;

    if (DxvkGpuVendor(devInfo.core.properties.vendorID) != DxvkGpuVendor::Amd)
      constantBufferRangeCheck = options.constantBufferRangeCheck;
    
    // Disable early discard on RADV due to GPU hangs
    // Disable early discard on Nvidia because it may hurt performance
    if (adapter->matchesDriver(DxvkGpuVendor::Amd,    VK_DRIVER_ID_MESA_RADV_KHR,          0, 0)
     || adapter->matchesDriver(DxvkGpuVendor::Nvidia, VK_DRIVER_ID_NVIDIA_PROPRIETARY_KHR, 0, 0))
      useSubgroupOpsForEarlyDiscard = false;
    
    // Disable atomic counters on older RADV versions
    if (adapter->matchesDriver(DxvkGpuVendor::Amd, VK_DRIVER_ID_MESA_RADV_KHR, 0, VK_MAKE_VERSION(19, 1, 0)))
      useSubgroupOpsForAtomicCounters = false;
    
    // Apply shader-related options
    applyTristate(useSubgroupOpsForEarlyDiscard, device->config().useEarlyDiscard);
  }
  
}