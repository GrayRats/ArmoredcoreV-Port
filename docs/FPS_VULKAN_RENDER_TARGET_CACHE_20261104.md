# Render-target pipelines in the persistent Vulkan cache — 2026-11-04

The persistent `VkPipelineCache` now also reaches the two lazy graphics-pipeline
creation paths for render-target ownership transfers: the base pipeline and its
masked-sample derivatives. Successful creations notify the existing cache writer
so its periodic snapshot can include these entries. The render-target cache only
borrows the handle and callback; `VulkanPipelineCache::Shutdown` clears both
before destroying the cache owner. With no usable cache handle, creation falls
back to `VK_NULL_HANDLE` as before.

This follow-up is included in the separately packaged
[ACV-Linux-release-2.1](../ACV-Linux-release-2.1/start.sh). The final build
recompiled243 native TUs without stale PCH and linked both runtime/GPU libraries;
the package verifier checked189 payload files, shell syntax and shared-library
resolution. No v2.1 runtime/game launch, GPU image check or performance test was
run. No FPS or stutter improvement is claimed; `ACV-Linux-release-2.0` remains
unchanged.

## Changed code

- `VulkanPipelineCache::Initialize` passes its device cache to the render-target
  cache after successful initialization.
- Each successful ownership-transfer graphics pipeline creation calls the same
  `PersistentPipelineCache::Created` notification used for guest pipelines.
- `VulkanPipelineCache::Shutdown` stops pipeline workers, clears the borrowed
  render-target references, then saves and destroys the cache.
- The feature remains opt-in through `ACV_VULKAN_PIPELINE_CACHE`; a missing or
  unavailable cache retains the uncached creation path.

The cache is created with default flags, so the externally-synchronized cache
mode is not requested. Cache ownership remains with the device's pipeline-cache
owner, and the render-target cache has no destruction responsibility for it.
The local Khronos Vulkan-Docs snapshot is pinned at
`090f1b190d60ced4a1d198fd3747d071cc271b1c`; `chapters/pipelines.adoc` covers the
graphics-pipeline cache parameter and its externally-synchronized flag rule.

## Build evidence and next acceptance

The v2.1 build records the seven-path aggregate and additive
[render-target cache patch](tools/native-rexglue/patches/vulkan-render-target-driver-cache.patch)
in [build.json](tmp/acv-vulkan-release-2.1-build/build.json); the exact
before/after SDK hashes remain in
[`patch-manifest.json`](logs/render-target-cache-20261104/patch-manifest.json).

The focused v2.1 GPU/image and matched cold/warm hitch checks remain pending.
The compiled package is a candidate and has not been accepted as a playable or
performance-improved release.
