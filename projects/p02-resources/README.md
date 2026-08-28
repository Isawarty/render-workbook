# P2 — 资源与场景

**预估工时 45–60 小时。**

P1 的三角形顶点是硬编码在 shader 里的，因为「把数据搬到 GPU」这件事本身就够一整节课。
P2 把真正的数据管线建起来：显存分配、staging 中转、descriptor、纹理、深度、模型加载、多重采样。

做完 P2，你手上就是一个能加载 glTF、带纹理和深度、开着抗锯齿的渲染器 ——
从这里到 P4 的延迟渲染只差光照模型和多附件。

## 8 个 task

| task | 内容 | 任务书 |
|---|---|---|
| t01 | VMA / vertex buffer / index buffer / staging 上传 | [docs/t01-buffers.md](docs/t01-buffers.md) |
| t02 | UBO / descriptor set layout / pool / set | [docs/t02-uniforms.md](docs/t02-uniforms.md) |
| t03 | push constant，与 UBO 对比 | [docs/t03-pushconstants.md](docs/t03-pushconstants.md) |
| t04 | **image / staging 上传 / layout transition（最难）** | [docs/t04-texture.md](docs/t04-texture.md) |
| t05 | image view / sampler / mipmap 生成 | [docs/t05-sampler.md](docs/t05-sampler.md) |
| t06 | 深度缓冲 / 深度测试 | [docs/t06-depth.md](docs/t06-depth.md) |
| t07 | glTF 加载 / 多 mesh 绘制 | [docs/t07-model.md](docs/t07-model.md) |
| t08 | 多重采样 | [docs/t08-msaa.md](docs/t08-msaa.md) |

t04 是这一节的分水岭。buffer 只有「一段字节」，image 多了 tiling 和 layout 两层，
而 layout transition 是 Vulkan 里最容易写错、也最容易被 validation 抓住的一环。

## 新东西：`engine/rhi/`

从 P2 起，instance / device / swapchain / 同步与呈现不再是挖空题 ——
它们变成了 `engine/rhi/` 里的框架代码。

**这不是黑盒，是你在 P1 亲手写过那份代码的清理版。** 开工前值得花二十分钟：

```bash
git diff mine/p01-t02 -- projects/p01-triangle/src/steps/02_device.cpp
# 然后对着读 engine/src/rhi/Context.cpp
```

相对你写的那份，实质改动只有三处，都在 `engine/include/rwb/rhi/Context.h` 顶部注释里写明了：
多找一个专用 compute 队列（P3 要用）、设备特性变成显式开关、接管 VMA allocator 的创建。

## 代码组织

```
src/
  ResourceApp.h        类声明。贯穿 8 个 task，不用改
  ResourceApp.cpp      框架：场景数据、render pass、pipeline、每帧录制
  main.cpp             入口。框架提供
  steps/
    01_buffers.cpp     <- t01 在这里
    02_uniforms.cpp    <- t02
    ...                   一个 task 一个文件
shaders/
  scene_basic.*        t01 用：无 UBO、无 push、无纹理
  scene_ubo.*          t02 用：加相机矩阵
  scene_push.*         t03/t04 用：加每物体的 model 矩阵
  scene_tex.*          t05 起用：加纹理采样
assets/
  shapes.gltf          t07 加载的模型（三个 mesh，各自覆盖一组解析分支，顶点数据内嵌）
  make_shapes_gltf.py  上面那个文件的生成脚本
tests/
  test_p02.cpp         判分逻辑。可以读，能帮你理解验收标准
```

`initUpTo(Stage)` 把初始化切成 8 段，每个 task 的测试只驱动到自己那一段。
render pass 和 pipeline 会随阶段变形（t06 加深度、t08 加多重采样），
所以 `ResourceApp.cpp` 里有不少 `if (stage >= ...)` —— 读的时候只看你当前那一档就行。

两个函数在骨架里是**直接给你的**：`destroyBuffer` 和 `destroyImage`。
它们是 `noexcept`，而清理路径在任何阶段都会调它们；挖空的话你在做完对应的题之前，
每次程序退出收到的都会是一个 abort 弹窗而不是可读的报错。P1 的析构函数上已经栽过一次。

## 单个 task 的工作流

```bash
# 1. 领题（自带前面所有 task 的官方基线）
git checkout -B work start/p02-t04

# 2. 填 src/steps/04_texture.cpp 里的 TODO

# 3. 判分
cmake --build build/win-msvc
python rwb.py test p02-t04

# 4. 对答案（可以选择不看）
git diff done/p02-t04 -- projects/p02-resources/src/steps/04_texture.cpp

# 5. 存档你自己的实现，然后进下一题
git commit -am "p02-t04: my impl" && git tag mine/p02-t04
git checkout -B work start/p02-t05
```

完整说明见 [docs/02-git-workflow.md](../../docs/02-git-workflow.md)。

## 直接运行

```bash
# 参数是阶段号 1-8，不给就跑到最后一阶段
./build/win-msvc/projects/p02-resources/p02_resources 6
```

没实现的函数会让它以退出码 10 退出，并告诉你卡在哪个函数。那不是 bug，是进度提示。

## 判分说明

- **L1 validation** — 全程零 error 零 warning。t04 的 layout transition 和 t05 的
  mip 链几乎全靠这一层判分：画面看起来对、但 barrier 写错，只有 validation 抓得住。
- **结构断言** — 该建的对象建出来了、数量关系对、声明的 push constant 没超 128 字节、
  glTF 的索引落在自己那段顶点范围内。
- **L3 golden** — 截图与基准图比对。基准图由 CI 的 lavapipe 生成；
  本地没有基准时该条 SKIP 而不是 FAIL。

关于 L3 有一件事要说清楚，免得你以为测试放水：**t05 之后的画面在真实 GPU 上
是不可能和基准图逐像素一致的**。各向异性过滤的实现是厂商私有的，
NVIDIA 的 16x 各向异性和 lavapipe 的「压根没有」画出来的地面差得极远。
所以这几题在真实 GPU 上会退化成「结构比对」——
把图切成 16×16 的块比平均色，构图错了照样抓得住，逐像素的过滤差异则被平均掉。
严格的逐像素判分仍然在 CI 的 lavapipe 上做。

t08 更特殊：它刻意**不建自己的基准图**。它的场景和 t07 完全相同，区别只有多重采样，
所以测试是「结构上必须和 t07 的基准一致，同时逐像素必须和单采样版本不同」。
后者是在验 resolve 真的生效了 —— 完全一致说明 `pResolveAttachments` 没接对，
画面其实还是单采样那张。

## 完成标志

```bash
python rwb.py test p02     # 8 个 task 全绿
```

下一站是 [P3 — Compute Shader 专项](../../docs/00-roadmap.md)，
全套里判分最硬的一段：每一题都是 GPU 结果回读后与 CPU 参考实现逐元素比对，
100% 确定性，不受光栅化和驱动差异影响。
