# P1-t01 — Instance / Validation Layer / Debug Messenger

## 目标

让程序和 Vulkan 建立连接，并且把 validation layer 的消息接到本仓库的判分系统上。

`VkInstance` 代表「这个进程和 Vulkan 的连接」。OpenGL 里没有对应物 —— OpenGL 的
context 由窗口系统隐式创建，你从来不用管。Vulkan 把它变成了你的第一个显式对象。

## 要实现的函数

`src/steps/01_instance.cpp` 里的四个：

| 函数 | 职责 |
|---|---|
| `validationLayerSupported()` | 查 `VK_LAYER_KHRONOS_validation` 在不在 |
| `requiredInstanceExtensions()` | 窗口系统扩展 + debug utils |
| `createInstance()` | 建 `m_instance` |
| `setupDebugMessenger()` | 建 `m_debugMessenger`，回调接到 `ValidationLog` |

## 验收标准

```bash
python rwb.py test p01-t01
```

测试检查三件事：

1. `m_instance != VK_NULL_HANDLE`
2. `m_debugMessenger != VK_NULL_HANDLE`
3. **整个生命周期（含销毁）validation 零 error 零 warning**

第 3 条依赖你把回调接对：回调里必须**原样转交**

```cpp
rwb::ValidationLog::instance().record(severity, types, data->pMessage);
```

注意 `types` 也要传，不要自己先按 severity 过滤。

**为什么由框架决定什么算失败**：debug messenger 这条通道上不只有 validation layer
的消息，Vulkan loader 自己也会发。比如 CI 上用 `VK_LOADER_DRIVERS_SELECT`
只保留 lavapipe 时，loader 会为每个被过滤掉的 ICD 发一条 WARNING：

```
Driver "radeon_icd.json" ignored because not selected by env var ...
```

这类消息是 `GENERAL` 类型，和你的代码对错毫无关系。本仓库的判分规则是：

| 消息类型 | 是否判失败 |
|---|---|
| `VALIDATION` | **是**（error 和 warning 都算） |
| `PERFORMANCE` | 否，单独计数 —— 不同驱动的性能建议差异很大，拿它判分会让结果取决于跑在谁家 GPU 上 |
| `GENERAL` | 否，只记录 |

这套分类逻辑本身有测试：`ctest -L framework`。

不接这根线，测试会「假绿」—— validation 报了错也没人知道。

## 阅读材料

按这个顺序读，不要一上来就读 spec：

1. **vulkan-tutorial.com** → Drawing a triangle → Setup → *Instance* 与 *Validation layers*
   两节。这是最快的入门路径。
2. **vkguide.dev** → Chapter 0 → *Vulkan Init Code*。它用 vk-bootstrap 简化了这一段，
   看它是为了理解「哪些是必要的、哪些是样板」。
3. **Vulkan Spec** §4.2 *Devices* 之前的 §4.1 *Instances*。只在你对某个字段有疑问时查。
4. macOS 用户额外读：**MoltenVK Runtime User Guide** 里关于
   `VK_KHR_portability_enumeration` 的部分。

## 常见坑

- **macOS 直接返回 `VK_ERROR_INCOMPATIBLE_DRIVER`**
  MoltenVK 是 portability 实现，必须同时启用 `VK_KHR_portability_enumeration` 扩展
  和 `VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR` 标志。
  写成「有就加」而不是「macOS 才加」，这样同一份代码三平台通用。

- **debug messenger 收不到 instance 创建/销毁期间的消息**
  正式的 messenger 要在 instance 之后才能建，所以它天然覆盖不到 `vkCreateInstance`
  自身。解法是再填一个 `VkDebugUtilsMessengerCreateInfoEXT` 挂到
  `VkInstanceCreateInfo::pNext` 上。

- **回调返回 `VK_TRUE`**
  那会让触发消息的那个 Vulkan 调用直接中止，是给 layer 开发者调试用的。
  应用层恒返回 `VK_FALSE`。

- **`vkCreateDebugUtilsMessengerEXT` 是空指针**
  它是扩展函数，不在 loader 的核心导出里。本仓库用 volk，
  `volkLoadInstance()` 已经在框架里替你调过了（见 `TriangleApp::initUpTo`）。

## 做完之后

```bash
git diff done/p01-t01 -- projects/p01-triangle/src/steps/01_instance.cpp
```

参考实现里的注释解释了每个选择的理由。看之前先自己跑通测试。
