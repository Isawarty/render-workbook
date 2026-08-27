# P1-t02 — Surface / Physical Device / Queue Family / Logical Device

## 目标

从机器上的若干块 GPU 里挑一块，声明你要用哪些队列，拿到 `VkDevice`。

Vulkan 把「设备」拆成两层，这是它和 OpenGL 最根本的差别之一：

- `VkPhysicalDevice` — 只读句柄，代表一块物理 GPU，用来**查询能力**
- `VkDevice` — 你显式声明了要用哪些队列、开哪些扩展和特性之后，得到的**逻辑连接**

没声明就使用的东西，行为未定义。这条规则贯穿整个 Vulkan。

## 要实现的函数

`src/steps/02_device.cpp`：`createSurface` / `querySwapchainSupport` / `findQueueFamilies` / `deviceExtensionsSupported` /
`isDeviceSuitable` / `pickPhysicalDevice` / `createLogicalDevice`

## 验收标准

```bash
python rwb.py test p01-t02
```

0. `m_surface` 非空
1. `m_physicalDevice` 和 `m_device` 都非空
2. `m_queueFamilies.complete()`（graphics 与 present 都找到了）
3. validation 零报错

## 阅读材料

1. **vulkan-tutorial.com** → Setup → *Physical devices and queue families*、
   *Logical device and queues*，以及 Presentation → *Window surface* 三节。
   注意 vulkan-tutorial 也是在「Window surface」那一章里回头重构了设备选择代码 ——
   原因和本课把 surface 放进 t02 是同一个。
2. **Vulkan Spec** §5.1 *Physical Devices*、§5.3 *Queues*。
   重点看 `VkQueueFlagBits` 的语义 —— 尤其是「支持 graphics 的队列族一定支持 transfer」
   这条隐含规则。
3. 想知道真实引擎怎么选设备，可以看 **vk-bootstrap** 的 `PhysicalDeviceSelector`
   源码，它把工业界的判据都列出来了。

## 为什么 surface 在这一题

「这个设备能不能把画面交给这个窗口」是选物理设备的判据之一。没有 surface，
`vkGetPhysicalDeviceSurfaceSupportKHR` 就无从调用，你也就无法判断 present 支持。

同理，`querySwapchainSupport()` 也在这一题：判断「这个设备对这个窗口有没有
可用的格式和呈现模式」同样是选设备的判据。

所以顺序必须是：surface → 查询 surface 能力 → 挑设备 → 建逻辑设备。
把这两样放到 swapchain 那一题去做，依赖顺序就是错的 ——
本课程在构建时用一个脚本逐个 tag 验证「前 N 题绿、后面红」，
正是这个脚本抓出了原本的错误顺序。

## 常见坑

- **无脑取第 0 个物理设备**
  跑一下 `p00_setup`，看看你这台机器的枚举顺序。很多笔记本上核显排在独显前面，
  取第 0 个会让你在集显上跑一整门课。要打分挑选。

- **假设 graphics 队列一定能 present**
  绝大多数硬件上确实是同一个族，但规范不保证。必须用
  `vkGetPhysicalDeviceSurfaceSupportKHR` 单独查。

- **两个族相同时提交了两份 `VkDeviceQueueCreateInfo`**
  同一个 `queueFamilyIndex` 出现两次是非法的，validation 会报错。用 `std::set` 去重。

- **忘了 `pQueuePriorities`**
  即使只有一个队列，这个字段也是必填的，传 `nullptr` 是非法的。

- **macOS: 漏了 `VK_KHR_portability_subset`**
  规范规定：只要设备**上报**了这个扩展，你就**必须**启用它。不启用是未定义行为。
  同样写成「有就加」。

- **还在设置 `enabledLayerCount` / `ppEnabledLayerNames`**
  device 级 layer 在现代 Vulkan 里已废弃。instance 上开的 layer 会自动覆盖 device 调用。

## 做完之后

```bash
git diff done/p01-t02 -- projects/p01-triangle/src/steps/02_device.cpp
```
