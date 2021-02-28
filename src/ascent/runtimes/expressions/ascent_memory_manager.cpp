#include "ascent_memory_manager.hpp"
#include <ascent_logging.hpp>
#include <ascent_config.h>

#include <umpire/Umpire.hpp>
#include <umpire/util/MemoryResourceTraits.hpp>

#include <conduit.hpp>

namespace ascent
{


int AllocationManager::m_umpire_device_allocator_id = -1;
int AllocationManager::m_umpire_host_allocator_id = -1;
int AllocationManager::m_conduit_host_allocator_id = -1;
bool AllocationManager::m_external_device_allocator = false;

int
AllocationManager::umpire_device_allocator_id()
{
  if(m_umpire_device_allocator_id == -1)
  {
    auto &rm = umpire::ResourceManager::getInstance ();
    auto allocator = rm.getAllocator("DEVICE");
    // we can use the umpire profiling to find a good default size
    auto pooled_allocator = rm.makeAllocator<umpire::strategy::DynamicPool>(
                            "GPU_POOL",
                            allocator,
                            1ul * // 1GB default size
                            1024ul * 1024ul * 1024ul + 1);
    m_umpire_device_allocator_id = pooled_allocator.getId();
  }
  return m_umpire_device_allocator_id;
}

int
AllocationManager::umpire_host_allocator_id()
{
  if(m_umpire_host_allocator_id == -1)
  {
    auto &rm = umpire::ResourceManager::getInstance ();
    auto allocator = rm.getAllocator("HOST");
    // we can use the umpire profiling to find a good default size
    auto pooled_allocator = rm.makeAllocator<umpire::strategy::DynamicPool>(
                            "HOST_POOL",
                            allocator,
                            1ul * // 1GB default size
                            1024ul * 1024ul * 1024ul + 1);
    m_umpire_host_allocator_id = pooled_allocator.getId();
  }
  return m_umpire_host_allocator_id;
}

bool
AllocationManager::umpire_device_allocator_id(int id)
{

  if(m_external_device_allocator && m_umpire_device_allocator_id != id)
  {
    // with the current implementation, i cant control switching allocators in the middle
    // This would cause a mismatch between memory allocated with one 'allocator' then that
    // memory being deallocated with another. Something to think about
    ASCENT_ERROR("Setting the device allocator id in the middle of the run to something new is BAD\n");
  }

  auto &rm = umpire::ResourceManager::getInstance ();
  bool valid_id = true;

  umpire::Allocator allocator;
  try
  {
    allocator = rm.getAllocator (id);
  }
  catch(...)
  {
    valid_id = false;
  }

  if(!valid_id)
  {
    return false;
  }

  auto resource = allocator.getAllocationStrategy()->getTraits().resource;

  bool can_use = false;
  bool need_device = false;

#ifdef ASCENT_USE_CUDA
  need_device = true;
#endif

  bool is_device = resource == umpire::MemoryResourceTraits::resource_type::DEVICE;
  bool is_host = resource == umpire::MemoryResourceTraits::resource_type::HOST;

  if(is_device && need_device)
  {
    can_use = true;
  }
  else if(is_host && !need_device)
  {
    can_use = true;
  }
  if(!can_use)
  {
    return false;
  }

  if(id != m_umpire_device_allocator_id)
  {
    // Matt: i don't think anyone will s
    // if this is not the same, we have to get rid
    // of all currently allocated deviec resources.
    // Data will be preserved by a synch to host
    //release_device_res();
    m_umpire_device_allocator_id = id;
  }
  m_external_device_allocator = true;
  return true;
}

int
AllocationManager::conduit_host_allocator_id()
{
  if(m_conduit_host_allocator_id == -1)
  {
    m_conduit_host_allocator_id
      = conduit::utils::register_mem_handler(HostAllocator::alloc,
                                             HostAllocator::free,
                                             HostAllocator::copy,
                                             HostAllocator::memset);

  }
  return m_conduit_host_allocator_id;
}

size_t HostAllocator::m_total_bytes_alloced = 0;
size_t HostAllocator::m_alloc_count = 0;
size_t HostAllocator::m_free_count = 0;

void *
HostAllocator::alloc(size_t items, size_t item_size)
{
  std::cout<<"Bananas allocate\n";
  m_total_bytes_alloced += items * item_size;
  m_alloc_count++;
  auto &rm = umpire::ResourceManager::getInstance ();
  const int allocator_id = AllocationManager::umpire_host_allocator_id();
  umpire::Allocator host_allocator = rm.getAllocator (allocator_id);
  return host_allocator.allocate (items * item_size);
}

void
HostAllocator::free(void *data_ptr)
{
  std::cout<<"free bananas\n";
  m_free_count++;

  auto &rm = umpire::ResourceManager::getInstance ();
  const int allocator_id = AllocationManager::umpire_host_allocator_id();
  umpire::Allocator host_allocator = rm.getAllocator (allocator_id);
  host_allocator.deallocate (data_ptr);
}

void
HostAllocator::memset(void * ptr, int value, size_t num )
{
  std::cout<<"set bananas\n";
  memset(ptr,value,num);
}

void
HostAllocator::copy(void * destination, const void * source, size_t num)
{
  std::cout<<"copy bananas\n";
  memcpy(destination,source,num);
}

// ----------------------- Memory Access -----------------------------
const conduit::float32 *
MemoryAccessor::float32_ptr_const(const conduit::Node &values)
{
  if(!values.dtype().is_float32())
  {
    ASCENT_ERROR("Accessor: Data type is not float32");
  }
  const conduit::float32 *ptr =  values.as_float32_ptr();
  return ptr;
}

const conduit::float64 *
MemoryAccessor::float64_ptr_const(const conduit::Node &values)
{
  if(!values.dtype().is_float64())
  {
    ASCENT_ERROR("Accessor: Data type is not float64");
  }
  const conduit::float64 *ptr =  values.as_float64_ptr();
  return ptr;
}

} // namespace ascent
