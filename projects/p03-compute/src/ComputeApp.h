#pragma once

// P3 —— Compute Shader 专项。
//
// ## 和 P1/P2 的两个结构性差别
//
// 1. **没有窗口。** Context 用 headless 模式：不建 window、不建 surface、
//    不要 swapchain 扩展。P3 的输出全部是 buffer，靠回读判分，不出图。
// 2. **没有 initUpTo 级联。** P2 里 t05 的 sampler 依赖 t04 的 image，所以要按阶段
//    逐层初始化；compute 三题的资源互不相干，每题一个独立入口方法，
//    自己申请、自己释放。这也让「t03 没写完不会影响 t01 判分」成为结构性事实，
//    而不是靠小心翼翼维护出来的。
//
// ## 谁写哪一部分
//
// 直接给你的（`ComputeApp.cpp`）：Context 创建、createBuffer / destroyBuffer /
// uploadToBuffer。前者是 P1 的成果，后两者是 P2-t01 的成果 —— 再写一遍不会让你
// 多懂 compute 任何事。
//
// 你写的（`src/steps/0N_*.cpp` 和被挖空的 `shaders/*.comp`）：
// descriptor pool / set layout / set、compute pipeline、dispatch、
// 以及 dispatch 之间和 dispatch 与回读之间的 **barrier**。
//
// barrier 是本项目真正的考点。P3 全程开着 sync validation；但 validation layer
// 对 descriptor 背后的 shader 资源 hazard 并非完备 oracle，所以 L2 回读仍是最终兜底。

#include "rwb/rhi/Context.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// VMA 的句柄。和 P2 一样在这里前向声明，免得把 vk_mem_alloc.h 拖进每个 TU。
VK_DEFINE_HANDLE(VmaAllocation)

namespace p03 {

struct AppConfig {
    std::string title            = "render-workbook p03";
    bool        enableValidation = true;
};

// 一块显存。和 P2 的 Buffer 同构 —— 你在 P2-t01 写过它。
struct Buffer {
    VkBuffer      handle     = VK_NULL_HANDLE;
    VmaAllocation allocation = nullptr;
    VkDeviceSize  size       = 0;
    void*         mapped     = nullptr;

    bool valid() const { return handle != VK_NULL_HANDLE; }
};

// 一条 compute 管线连同它的 layout。
//
// 三样东西的寿命是绑在一起的，所以放一个结构体里：
//   setLayout  描述「shader 期待几个 storage buffer」
//   layout     = setLayout + push constant range
//   pipeline   = layout + 一个 compute stage
struct ComputePipeline {
    VkDescriptorSetLayout setLayout    = VK_NULL_HANDLE;
    VkPipelineLayout      layout       = VK_NULL_HANDLE;
    VkPipeline            pipeline     = VK_NULL_HANDLE;
    std::uint32_t         bindingCount = 0;

    bool valid() const { return pipeline != VK_NULL_HANDLE; }
};

struct BarrierSnapshot {
    VkPipelineStageFlags srcStage  = 0;
    VkPipelineStageFlags dstStage  = 0;
    VkAccessFlags        srcAccess = 0;
    VkAccessFlags        dstAccess = 0;
};

struct QueueSyncSnapshot {
    std::uint32_t computeFamily = VK_QUEUE_FAMILY_IGNORED;
    std::uint32_t graphicsFamily = VK_QUEUE_FAMILY_IGNORED;
    bool usedOwnershipTransfer = false;
    bool semaphoreWaitedByGraphics = false;
};

// t02 的两条归约路径。
enum class ReducePath {
    Subgroup,   // 用 subgroupAdd()，要求设备支持 subgroup 算术运算
    Shared,     // 共享内存里的树形归约，任何设备都能跑
};
const char* toString(ReducePath path);

// .spv 文件的完整路径（构建期由 cmake/CompileShaders.cmake 产出）。
std::string shaderPath(const std::string& name);

// 所有 compute shader 的 local_size_x。host 侧算 dispatch 组数时必须和它一致 ——
// 对不上就是「最后不足一组的那些元素没人算」或者「越界写」。
inline constexpr std::uint32_t kWorkgroupSize = 256;

// t03 的分块大小。一个 workgroup 扫一块。
// 两级 scan 因此最多处理 kScanBlockSize * kScanBlockSize 个元素。
inline constexpr std::uint32_t kScanBlockSize = kWorkgroupSize;
inline constexpr std::uint32_t kScanMaxElements = kScanBlockSize * kScanBlockSize;

class ComputeApp {
public:
    explicit ComputeApp(AppConfig config = {});
    ~ComputeApp();

    ComputeApp(const ComputeApp&)            = delete;
    ComputeApp& operator=(const ComputeApp&) = delete;

    const rwb::rhi::Context& ctx() const { return *m_ctx; }
    std::string              deviceName() const;

    // ===== 框架：显存与上传（P2-t01 你写过，这里直接给你）=====================
    Buffer createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                        bool hostVisible, bool persistentlyMapped);
    void   destroyBuffer(Buffer& buffer) noexcept;
    // 经 staging 把 CPU 数据搬进 device-local buffer。
    void   uploadToBuffer(const void* data, VkDeviceSize size, const Buffer& dst);

    // ===== t01 —— src/steps/01_saxpy.cpp ====================================
    // descriptor pool。第一次用到时惰性创建，销毁在 cleanup() 里（noexcept，不挖空）。
    VkDescriptorPool descriptorPool();

    // 建一条 compute 管线。
    //   spvName             .spv 文件名，如 "saxpy.comp.spv"
    //   storageBindingCount shader 里声明了几个 storage buffer（binding 0..n-1）
    //   pushConstantSize    push constant 块的字节数；0 表示不用
    ComputePipeline createComputePipeline(const std::string& spvName,
                                          std::uint32_t      storageBindingCount,
                                          std::uint32_t      pushConstantSize);
    void            destroyComputePipeline(ComputePipeline& pipe) noexcept;

    // 从池里分一个 descriptor set，并把 buffers 依次写进 binding 0..n-1。
    VkDescriptorSet allocateStorageSet(const ComputePipeline&            pipe,
                                       const std::vector<const Buffer*>& buffers);

    // y := a * x + y，在 GPU 上算。x 和 y 必须等长。
    std::vector<float> runSaxpy(float a, const std::vector<float>& x,
                                const std::vector<float>& y);

    // ===== t02 —— src/steps/02_reduce.cpp ===================================
    // 纯函数：给定设备的 subgroup 能力，决定走哪条路。
    //
    // 故意不让它读设备 —— 这样测试可以拿编造的能力值表驱动地考它，
    // 而不必真的找一台没有 subgroup 算术的机器。
    static ReducePath choosePath(VkSubgroupFeatureFlags supportedOperations,
                                 VkShaderStageFlags     supportedStages,
                                 std::uint32_t          subgroupSize);
    // 用本机真实能力调上面那个。
    ReducePath chooseReducePath() const;

    // data 的所有元素求和。path 指定走哪条实现。
    float runReduce(const std::vector<float>& data, ReducePath path);

    // ===== t03 —— src/steps/03_scan.cpp =====================================
    // exclusive prefix sum：out[0] = 0, out[i] = data[0] + ... + data[i-1]。
    // 元素个数上限 kScanMaxElements。
    std::vector<std::uint32_t> runScan(const std::vector<std::uint32_t>& data);

    // ===== t04 —— src/steps/04_bitonic.cpp =================================
    // 升序 bitonic sort。输入不要求是 2 的幂；host 侧补 UINT_MAX 后再裁掉 padding。
    std::vector<std::uint32_t> runBitonicSort(const std::vector<std::uint32_t>& data);

    // ===== t05 —— src/steps/05_postprocess.cpp ==============================
    // 输入是 width*height 个线性 RGBA float 像素。compute 做 3x3 Gaussian + Reinhard
    // tonemap，graphics fragment pass 再逐像素消费结果，最终回读仍为 RGBA float。
    std::vector<float> runPostprocess(const std::vector<float>& rgba,
                                      std::uint32_t width, std::uint32_t height);
    const BarrierSnapshot& lastComputeToGraphicsBarrier() const { return m_lastBarrier; }

    // ===== t06 —— src/steps/06_particles.cpp ================================
    // RGBA/vec4 粒子位置由 compute 更新，再由 graphics vertex stage 消费并写入
    // 回读 buffer。专用 compute queue 存在时必须转移 queue-family ownership。
    std::vector<float> runParticles(const std::vector<float>& positions,
                                    float deltaX, float deltaY);
    const QueueSyncSnapshot& lastQueueSync() const { return m_lastQueueSync; }

    // ===== t07 —— src/steps/07_indirect.cpp =================================
    // 第一条 compute shader 在 GPU 上生成 VkDispatchIndirectCommand，第二条通过
    // vkCmdDispatchIndirect 执行缩放。返回缩放后的 float buffer。
    std::vector<float> runIndirectScale(const std::vector<float>& data, float factor);
    const VkDispatchIndirectCommand& lastIndirectCommand() const { return m_lastIndirect; }

private:
    void cleanup() noexcept;

    AppConfig                          m_config;
    std::unique_ptr<rwb::rhi::Context> m_ctx;
    VkDescriptorPool                   m_descriptorPool = VK_NULL_HANDLE;
    BarrierSnapshot                    m_lastBarrier{};
    QueueSyncSnapshot                  m_lastQueueSync{};
    VkDispatchIndirectCommand          m_lastIndirect{};
};

// 让「中途抛异常」不至于变成显存泄漏。
//
// 为什么需要它：VMA 在 allocator 销毁时会报「还有 N 块没释放」，
// 而那条错误会记进 ValidationLog，把**下一题**的 L1 判成红 ——
// P2 踩过这个坑，症状是「我明明没动 t04，t04 却红了」。
// 挖空的函数随时可能抛 NotImplemented，所以 P3 里每一块临时 buffer 都用它兜着。
class ScopedBuffer {
public:
    ScopedBuffer(ComputeApp& app, Buffer buffer) : m_app(&app), m_buffer(buffer) {}
    ~ScopedBuffer() noexcept {
        if (m_app) m_app->destroyBuffer(m_buffer);
    }

    ScopedBuffer(ScopedBuffer&& other) noexcept : m_app(other.m_app), m_buffer(other.m_buffer) {
        other.m_app = nullptr;
    }
    ScopedBuffer& operator=(ScopedBuffer&&)      = delete;
    ScopedBuffer(const ScopedBuffer&)            = delete;
    ScopedBuffer& operator=(const ScopedBuffer&) = delete;

    Buffer&       get() { return m_buffer; }
    const Buffer& get() const { return m_buffer; }
    VkBuffer      handle() const { return m_buffer.handle; }

private:
    ComputeApp* m_app = nullptr;
    Buffer      m_buffer{};
};

// 同理，管线也要兜住。VkPipeline 漏了不会触发 VMA 的显存泄漏报告，
// 但 vkDestroyDevice 时 validation layer 会报「还有对象活着」——
// 一样会把 L1 判成红，而且报的是设备销毁那一行，看不出是哪条管线漏的。
class ScopedPipeline {
public:
    ScopedPipeline(ComputeApp& app, ComputePipeline pipe) : m_app(&app), m_pipe(pipe) {}
    ~ScopedPipeline() noexcept {
        if (m_app) m_app->destroyComputePipeline(m_pipe);
    }

    ScopedPipeline(ScopedPipeline&& other) noexcept : m_app(other.m_app), m_pipe(other.m_pipe) {
        other.m_app = nullptr;
    }
    ScopedPipeline& operator=(ScopedPipeline&&)      = delete;
    ScopedPipeline(const ScopedPipeline&)            = delete;
    ScopedPipeline& operator=(const ScopedPipeline&) = delete;

    ComputePipeline&       get() { return m_pipe; }
    const ComputePipeline& get() const { return m_pipe; }

private:
    ComputeApp*     m_app = nullptr;
    ComputePipeline m_pipe{};
};

} // namespace p03
