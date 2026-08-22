// mbedtls_psram_alloc.cpp — velké TLS buffery do PSRAM (ne interní DRAM)
// Arduino prebuild má CONFIG_MBEDTLS_INTERNAL_MEM_ALLOC — bez wrapu padá
// SSL - Memory allocation failed při RGB + NimBLE (~28 KB free heap).
#include <stddef.h>
#include <esp_heap_caps.h>

extern "C" {

void* __real_esp_mbedtls_mem_calloc(size_t n, size_t size);
void __real_esp_mbedtls_mem_free(void* ptr);

void* __wrap_esp_mbedtls_mem_calloc(size_t n, size_t size) {
  if (n == 0 || size == 0) {
    return nullptr;
  }
  const size_t bytes = n * size;
  // TLS buffery do PSRAM — interní DRAM nechat pro esp-sha / WiFi DMA
  if (bytes >= 512) {
    void* p = heap_caps_calloc(n, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (p) {
      return p;
    }
  }
  void* p = heap_caps_calloc(n, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (p) {
    return p;
  }
  return __real_esp_mbedtls_mem_calloc(n, size);
}

void __wrap_esp_mbedtls_mem_free(void* ptr) {
  if (!ptr) {
    return;
  }
  heap_caps_free(ptr);
}

}  // extern "C"
