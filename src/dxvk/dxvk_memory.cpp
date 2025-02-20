#include "dxvk_device.h"
#include "dxvk_memory.h"

namespace dxvk {
  
  DxvkMemory::DxvkMemory() { }
  DxvkMemory::DxvkMemory(
          DxvkMemoryAllocator*  alloc,
          DxvkMemoryChunk*      chunk,
          DxvkMemoryType*       type,
          VkDeviceMemory        memory,
          VkDeviceSize          offset,
          VkDeviceSize          length,
          void*                 mapPtr)
  : m_alloc   (alloc),
    m_chunk   (chunk),
    m_type    (type),
    m_memory  (memory),
    m_offset  (offset),
    m_length  (length),
    m_mapPtr  (mapPtr) { }
  
  
  DxvkMemory::DxvkMemory(DxvkMemory&& other)
  : m_alloc   (std::exchange(other.m_alloc,  nullptr)),
    m_chunk   (std::exchange(other.m_chunk,  nullptr)),
    m_type    (std::exchange(other.m_type,   nullptr)),
    m_memory  (std::exchange(other.m_memory, VkDeviceMemory(VK_NULL_HANDLE))),
    m_offset  (std::exchange(other.m_offset, 0)),
    m_length  (std::exchange(other.m_length, 0)),
    m_mapPtr  (std::exchange(other.m_mapPtr, nullptr)) { }
  
  
  DxvkMemory& DxvkMemory::operator = (DxvkMemory&& other) {
    this->free();
    m_alloc   = std::exchange(other.m_alloc,  nullptr);
    m_chunk   = std::exchange(other.m_chunk,  nullptr);
    m_type    = std::exchange(other.m_type,   nullptr);
    m_memory  = std::exchange(other.m_memory, VkDeviceMemory(VK_NULL_HANDLE));
    m_offset  = std::exchange(other.m_offset, 0);
    m_length  = std::exchange(other.m_length, 0);
    m_mapPtr  = std::exchange(other.m_mapPtr, nullptr);
    return *this;
  }
  
  
  DxvkMemory::~DxvkMemory() {
    this->free();
  }
  
  
  void DxvkMemory::free() {
    if (m_alloc != nullptr)
      m_alloc->free(*this);
  }
  

  DxvkMemoryChunk::DxvkMemoryChunk(
          DxvkMemoryAllocator*  alloc,
          DxvkMemoryType*       type,
          DxvkDeviceMemory      memory)
  : m_alloc(alloc), m_type(type), m_memory(memory) {
    // Mark the entire chunk as free
    m_freeList.push_back(FreeSlice { 0, memory.memSize });
  }
  
  
  DxvkMemoryChunk::~DxvkMemoryChunk() {
    // This call is technically not thread-safe, but it
    // doesn't need to be since we don't free chunks
    m_alloc->freeDeviceMemory(m_type, m_memory);
  }
  
  
  DxvkMemory DxvkMemoryChunk::alloc(
          VkMemoryPropertyFlags flags,
          VkDeviceSize          size,
          VkDeviceSize          align,
          float                 priority) {
    // Property flags must be compatible. This could
    // be refined a bit in the future if necessary.
    if (m_memory.memFlags != flags
     || m_memory.priority != priority)
      return DxvkMemory();
    
    // If the chunk is full, return
    if (m_freeList.size() == 0)
      return DxvkMemory();
    
    // Select the slice to allocate from in a worst-fit
    // manner. This may help keep fragmentation low.
    auto bestSlice = m_freeList.begin();
    
    for (auto slice = m_freeList.begin(); slice != m_freeList.end(); slice++) {
      if (slice->length == size) {
        bestSlice = slice;
        break;
      } else if (slice->length > bestSlice->length) {
        bestSlice = slice;
      }
    }
    
    // We need to align the allocation to the requested alignment
    const VkDeviceSize sliceStart = bestSlice->offset;
    const VkDeviceSize sliceEnd   = bestSlice->offset + bestSlice->length;
    
    const VkDeviceSize allocStart = dxvk::align(sliceStart,        align);
    const VkDeviceSize allocEnd   = dxvk::align(allocStart + size, align);
    
    if (allocEnd > sliceEnd)
      return DxvkMemory();
    
    // We can use this slice, but we'll have to add
    // the unused parts of it back to the free list.
    m_freeList.erase(bestSlice);
    
    if (allocStart != sliceStart)
      m_freeList.push_back({ sliceStart, allocStart - sliceStart });
    
    if (allocEnd != sliceEnd)
      m_freeList.push_back({ allocEnd, sliceEnd - allocEnd });
    
    // Create the memory object with the aligned slice
    return DxvkMemory(m_alloc, this, m_type,
      m_memory.memHandle, allocStart, allocEnd - allocStart,
      reinterpret_cast<char*>(m_memory.memPointer) + allocStart);
  }
  
  
  void DxvkMemoryChunk::free(
          VkDeviceSize  offset,
          VkDeviceSize  length) {
    // Remove adjacent entries from the free list and then add
    // a new slice that covers all those entries. Without doing
    // so, the slice could not be reused for larger allocations.
    auto curr = m_freeList.begin();
    
    while (curr != m_freeList.end()) {
      if (curr->offset == offset + length) {
        length += curr->length;
        curr = m_freeList.erase(curr);
      } else if (curr->offset + curr->length == offset) {
        offset -= curr->length;
        length += curr->length;
        curr = m_freeList.erase(curr);
      } else {
        curr++;
      }
    }
    
    m_freeList.push_back({ offset, length });
  }
  
  
  DxvkMemoryAllocator::DxvkMemoryAllocator(const DxvkDevice* device)
  : m_vkd             (device->vkd()),
    m_device          (device),
    m_devProps        (device->adapter()->deviceProperties()),
    m_memProps        (device->adapter()->memoryProperties()) {
    for (uint32_t i = 0; i < m_memProps.memoryHeapCount; i++) {
      VkDeviceSize heapSize = m_memProps.memoryHeaps[i].size;
      
      m_memHeaps[i].properties = m_memProps.memoryHeaps[i];
      m_memHeaps[i].chunkSize  = pickChunkSize(heapSize);
      m_memHeaps[i].stats      = DxvkMemoryStats { 0, 0 };
    }
    
    for (uint32_t i = 0; i < m_memProps.memoryTypeCount; i++) {
      m_memTypes[i].heap       = &m_memHeaps[m_memProps.memoryTypes[i].heapIndex];
      m_memTypes[i].heapId     = m_memProps.memoryTypes[i].heapIndex;
      m_memTypes[i].memType    = m_memProps.memoryTypes[i];
      m_memTypes[i].memTypeId  = i;
    }
  }
  
  
  DxvkMemoryAllocator::~DxvkMemoryAllocator() {
    
  }
  
  
  DxvkMemory DxvkMemoryAllocator::alloc(
    const VkMemoryRequirements*             req,
    const VkMemoryDedicatedRequirements&    dedAllocReq,
    const VkMemoryDedicatedAllocateInfoKHR& dedAllocInfo,
          VkMemoryPropertyFlags             flags,
          float                             priority) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Try to allocate from a memory type which supports the given flags exactly
    auto dedAllocPtr = dedAllocReq.prefersDedicatedAllocation ? &dedAllocInfo : nullptr;
    DxvkMemory result = this->tryAlloc(req, dedAllocPtr, flags, priority);

    // If the first attempt failed, try ignoring the dedicated allocation
    if (!result && dedAllocPtr && !dedAllocReq.requiresDedicatedAllocation) {
      result = this->tryAlloc(req, nullptr, flags, priority);
      dedAllocPtr = nullptr;
    }

    // If that still didn't work, probe slower memory types as well
    VkMemoryPropertyFlags optFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
                                   | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    
    if (!result && (flags & optFlags))
      result = this->tryAlloc(req, dedAllocPtr, flags & ~optFlags, priority);
    
    if (!result) {
      Logger::err(str::format(
        "DxvkMemoryAllocator: Memory allocation failed",
        "\n  Size:      ", req->size,
        "\n  Alignment: ", req->alignment,
        "\n  Mem flags: ", "0x", std::hex, flags,
        "\n  Mem types: ", "0x", std::hex, req->memoryTypeBits));

      for (uint32_t i = 0; i < m_memProps.memoryHeapCount; i++) {
        Logger::err(str::format("Heap ", i, ": ",
          (m_memHeaps[i].stats.memoryAllocated >> 20), " MB allocated, ",
          (m_memHeaps[i].stats.memoryUsed      >> 20), " MB used, ",
          (m_memHeaps[i].properties.size       >> 20), " MB available"));
      }

      throw DxvkError("DxvkMemoryAllocator: Memory allocation failed");
    }
    
    return result;
  }
  
  
  DxvkMemoryStats DxvkMemoryAllocator::getMemoryStats() {
    std::lock_guard<std::mutex> lock(m_mutex);

    DxvkMemoryStats totalStats;
    
    for (size_t i = 0; i < m_memProps.memoryHeapCount; i++) {
      totalStats.memoryAllocated += m_memHeaps[i].stats.memoryAllocated;
      totalStats.memoryUsed      += m_memHeaps[i].stats.memoryUsed;
    }
      
    return totalStats;
  }
  
  
  DxvkMemory DxvkMemoryAllocator::tryAlloc(
    const VkMemoryRequirements*             req,
    const VkMemoryDedicatedAllocateInfoKHR* dedAllocInfo,
          VkMemoryPropertyFlags             flags,
          float                             priority) {
    DxvkMemory result;

    for (uint32_t i = 0; i < m_memProps.memoryTypeCount && !result; i++) {
      const bool supported = (req->memoryTypeBits & (1u << i)) != 0;
      const bool adequate  = (m_memTypes[i].memType.propertyFlags & flags) == flags;
      
      if (supported && adequate) {
        result = this->tryAllocFromType(&m_memTypes[i],
          flags, req->size, req->alignment, priority, dedAllocInfo);
      }
    }
    
    return result;
  }
  
  
  DxvkMemory DxvkMemoryAllocator::tryAllocFromType(
          DxvkMemoryType*                   type,
          VkMemoryPropertyFlags             flags,
          VkDeviceSize                      size,
          VkDeviceSize                      align,
          float                             priority,
    const VkMemoryDedicatedAllocateInfoKHR* dedAllocInfo) {
    // Prevent unnecessary external host memory fragmentation
    bool isDeviceLocal = (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0;

    if (!isDeviceLocal)
      priority = 0.0f;

    DxvkMemory memory;

    if ((size >= type->heap->chunkSize / 4) || dedAllocInfo) {
      DxvkDeviceMemory devMem = this->tryAllocDeviceMemory(
        type, flags, size, priority, dedAllocInfo);

      if (devMem.memHandle != VK_NULL_HANDLE)
        memory = DxvkMemory(this, nullptr, type, devMem.memHandle, 0, size, devMem.memPointer);
    } else {
      for (uint32_t i = 0; i < type->chunks.size() && !memory; i++)
        memory = type->chunks[i]->alloc(flags, size, align, priority);
      
      if (!memory) {
        DxvkDeviceMemory devMem = tryAllocDeviceMemory(
          type, flags, type->heap->chunkSize, priority, nullptr);

        if (devMem.memHandle == VK_NULL_HANDLE)
          return DxvkMemory();
        
        Rc<DxvkMemoryChunk> chunk = new DxvkMemoryChunk(this, type, devMem);
        memory = chunk->alloc(flags, size, align, priority);

        type->chunks.push_back(std::move(chunk));
      }
    }

    if (memory)
      type->heap->stats.memoryUsed += memory.m_length;

    return memory;
  }
  
  
  DxvkDeviceMemory DxvkMemoryAllocator::tryAllocDeviceMemory(
          DxvkMemoryType*                   type,
          VkMemoryPropertyFlags             flags,
          VkDeviceSize                      size,
          float                             priority,
    const VkMemoryDedicatedAllocateInfoKHR* dedAllocInfo) {
    bool useMemoryPriority = (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
                          && (m_device->features().extMemoryPriority.memoryPriority);
    
    DxvkDeviceMemory result;
    result.memSize  = size;
    result.memFlags = flags;
    result.priority = priority;

    VkMemoryPriorityAllocateInfoEXT prio;
    prio.sType            = VK_STRUCTURE_TYPE_MEMORY_PRIORITY_ALLOCATE_INFO_EXT;
    prio.pNext            = dedAllocInfo;
    prio.priority         = priority;

    VkMemoryAllocateInfo info;
    info.sType            = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    info.pNext            = useMemoryPriority ? &prio : prio.pNext;
    info.allocationSize   = size;
    info.memoryTypeIndex  = type->memTypeId;

    if (m_vkd->vkAllocateMemory(m_vkd->device(), &info, nullptr, &result.memHandle) != VK_SUCCESS)
      return DxvkDeviceMemory();
    
    if (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
      VkResult status = m_vkd->vkMapMemory(m_vkd->device(), result.memHandle, 0, VK_WHOLE_SIZE, 0, &result.memPointer);

      if (status != VK_SUCCESS) {
        Logger::err(str::format("DxvkMemoryAllocator: Mapping memory failed with ", status));
        return DxvkDeviceMemory();
      }
    }

    type->heap->stats.memoryAllocated += size;
    m_device->adapter()->notifyHeapMemoryAlloc(type->heapId, size);
    return result;
  }


  void DxvkMemoryAllocator::free(
    const DxvkMemory&           memory) {
    std::lock_guard<std::mutex> lock(m_mutex);
    memory.m_type->heap->stats.memoryUsed -= memory.m_length;

    if (memory.m_chunk != nullptr) {
      this->freeChunkMemory(
        memory.m_type,
        memory.m_chunk,
        memory.m_offset,
        memory.m_length);
    } else {
      DxvkDeviceMemory devMem;
      devMem.memHandle  = memory.m_memory;
      devMem.memPointer = nullptr;
      devMem.memSize    = memory.m_length;
      this->freeDeviceMemory(memory.m_type, devMem);
    }
  }

  
  void DxvkMemoryAllocator::freeChunkMemory(
          DxvkMemoryType*       type,
          DxvkMemoryChunk*      chunk,
          VkDeviceSize          offset,
          VkDeviceSize          length) {
    chunk->free(offset, length);
  }
  

  void DxvkMemoryAllocator::freeDeviceMemory(
          DxvkMemoryType*       type,
          DxvkDeviceMemory      memory) {
    m_vkd->vkFreeMemory(m_vkd->device(), memory.memHandle, nullptr);
    type->heap->stats.memoryAllocated -= memory.memSize;
    m_device->adapter()->notifyHeapMemoryFree(type->heapId, memory.memSize);
  }


  VkDeviceSize DxvkMemoryAllocator::pickChunkSize(VkDeviceSize heapSize) const {
    // Pick a reasonable chunk size depending on the memory
    // heap size. Small chunk sizes can reduce fragmentation
    // and are therefore preferred for small memory heaps.
    constexpr VkDeviceSize MaxChunkSize  = 64 * 1024 * 1024;
    constexpr VkDeviceSize MinChunkCount = 16;

    return std::min(heapSize / MinChunkCount, MaxChunkSize);
  }
  
}