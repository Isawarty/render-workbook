---
name: course-review
description: render-workbook 课程内容的发布前审查。新增或改动 task 后、打 tag 前跑一遍。P0–P2 踩过的坑都编码在这里，专治「测试全绿但其实没判到」和「学生领不到题」。
---

# 课程内容发布前审查

用在 `render-workbook` 上：任何新 task、新 project、或改动已有判分逻辑之后，**打 tag 之前**走一遍。

规范正文在 `docs/04-course-authoring.md`，本文件是它的可执行版 —— 只列**实际炸过**的东西，
每一条后面都有一次真实事故。别跳。

---

## A. 判分是真的吗

判分出问题的默认形态不是「误报」，是**假绿**。以下每条都是假绿事故的复现方式。

- [ ] **人工注错，逐条验证断言真的抓得住它声称抓的东西。**
      写完断言就以为它有效，是本仓库最贵的一类错误。做法：把参考实现里对应的那一行
      改错 → 重新编译 → 跑测试 → 确认失败信息**指向正确的概念**。
      ```
      p02-t07 实测：
        忽略 byteStride        -> Pyramid 局部 y 范围 [-0.5, 0.95]，期望 [-0.2, 0.6]  抓到
        u32 索引按 u16 读       -> 有顶点没被任何索引引用                              抓到
        不认 matrix 形式 node   -> 平移列对不上                                        抓到
        四元数分量顺序写反      -> 101 条断言全过                                      漏了
      ```
      最后一条就是这么发现的。补了一条查旋转角的 CPU 断言才抓住。

- [ ] **解析层的语义不变量不能交给 golden。**
      真实 GPU 上 `filteringSensitive` 的场景走**结构比对**（16×16 块比平均色，容忍 2%）。
      实测四元数分量序写反只让 **2.00%** 的块超阈 —— 正好卡在容忍线上判为通过。
      严格逐像素只在 CI 的 lavapipe 上跑，学生在本机做题时**根本不跑**。
      所以：分量顺序、componentType、stride、矩阵主序、单位换算这类东西，
      一律写成与设备无关的 CPU 断言。golden 只负责「整体读成一团乱麻」。

- [ ] **资产真的触发了任务书声称要支持的每一条分支吗？**
      不触发 = 参考实现里那条路径从没被执行过 + 学生写错也全绿。
      两条出路，选一条，不许含糊：**补资产**，或**明确降级成扩展练习**。
      「要求了、也承认测不到、但你还是得写对」是最坏的第三种，不许有。
      > P2 的 `shapes.gltf` 为此从 2 个 mesh 改成 3 个，覆盖
      > 交错 byteStride / u8 / u16 / u32 / `matrix` 形式 node / 带 rotation 的 TRS。

- [ ] **借用别的用例基准图的测试，必须走 `compareToGoldenReadOnly`。**
      否则 `RWB_UPDATE_GOLDEN=1` 那一跑会按用例顺序把被借的那张覆盖成借用方的画面。
      这个 bug 只在重新生成基准图时发作。现有两处：t04 借 t03、t08 借 t07。

- [ ] **必做测试没有静默 SKIP。**
      基准图缺失时 SKIP 是对的（「基准没建立」≠「你写错了」），但那是**临时状态**：
      基准图入库前不算发布完成。

- [ ] **验证工具自己不会静默通过。**
      `verify-grading.py` 早先不检查 `git checkout` 的返回码，工作区一有未提交改动
      就整轮跑在同一个 commit 上，报「全部通过」。
      任何验证脚本，每一步失败都必须 `exit`，不许 `|| true`。

---

## B. 骨架能领题吗

- [ ] **`start/*` 能编译。** 编译不过 = 学生连题都领不到。这是全仓库最硬的不变量。

- [ ] **骨架状态下是干净失败，不是进程消失。**
      跑一遍，确认看到的是 `尚未实现: pXX-tNN Class::method`，而不是弹窗 / 静默退出 /
      判分表里一片 `?`。两次事故：
      1. `noexcept` 函数里抛异常 = `std::terminate()`。析构、`cleanup()`、
         `destroyBuffer`/`destroyImage` 都是 —— **这类函数必须直接给学生，不能挖**。
      2. VMA 的显存泄漏检查默认接 `assert()`，在关掉弹窗的测试进程里是**静默 abort**。
         t05 的 `generateMipmaps` 还是空的时候抛异常 → 跳过 `destroyBuffer(staging)` →
         VMA 断言把整个测试进程打死在 t04。**每个学生做到 t04 都会撞上。**
         已改成 `VMA_ASSERT_LEAK` → 打 error 继续。

- [ ] **参考实现异常安全。** 挖空的函数会抛异常，它上游的资源必须还能释放。
      `immediateSubmit` 的 lambda 里调用了未实现的函数时尤其要注意：
      ```cpp
      try { m_ctx->immediateSubmit([&](VkCommandBuffer cmd){ ... }); }
      catch (...) { destroyBuffer(staging); throw; }
      destroyBuffer(staging);
      ```

- [ ] **判分单调性。** 在 `done/tN` 上前 N 题绿、后面全红。
      ```
      python .authoring/verify-grading.py p01
      python .authoring/verify-grading.py p02
      ```
      要求是完美对角线，没有 `?`，没有意外的 SKIP。

---

## C. 文档说的和代码做的一致吗

- [ ] 任务书里的命令和真实 CTest 名字一致，且用统一入口 `python rwb.py test pXX-tNN`。
- [ ] 验收标准逐条对得上测试里的断言（数量、编号、含义）。
      改了资产就去查 `draws().size()` 这类硬编码数字 —— P2 从 4 改成 5 时漏过一轮。
- [ ] 任务书没有「测试抓不到但你照样得写对」的表述（见 A 第 3 条）。
- [ ] **交付状态在所有地方都更新了**：顶层英文 `README.md` 的项目表、
      `docs/00-roadmap.md` 的项目序列表。这两处曾经不一致过一整轮
      —— README 写 Shipped，roadmap 还写「大纲」。

---

## D. 历史与 tag

- [ ] **新增的仓库级文件必须存在于每一个 tag。**
      tag 指向整棵树的快照，不是改动。只在最新提交加的文件，
      `git checkout start/p01-t03` 之后就不存在了。
      `rwb.py` 就这么翻过车：文档写的第一条命令在 tag 上找不到文件。
      ```bash
      for t in $(git tag | grep '^start/'); do
        git cat-file -e "$t:rwb.py" 2>/dev/null || echo "缺失: $t"
      done
      ```

- [ ] **注入用 `filter-branch`，不要重建提交链。**
      `.md` / `.py` / `.png` 不参与编译，破坏不了「`start/*` 必须能编译」，
      重编 17 次买不到任何东西。工具在 `.authoring/inject/`。
      ```bash
      FILTER_BRANCH_SQUELCH_WARNING=1 git filter-branch -f --prune-empty \
        --tag-name-filter cat --index-filter "sh \"$RWB_INJECT_DIR/apply.sh\"" -- --all
      ```
      `--tag-name-filter cat` 不能少，否则 35 个 tag 全部留在旧历史上变成孤儿。
      改代码文件才需要重建链。

- [ ] **描述「你手上这棵树」的文档要分版。**
      `README.md` 和 `docs/00-roadmap.md` 会写「`engine/rhi/` 在这儿」「P2 已交付」。
      整条历史都塞最新版的话，`start/p01-t01` 会让人去找一个还不存在的目录。
      判据用「树里有没有 `projects/pNN-xxx/CMakeLists.txt`」。

- [ ] **基准图必须折进每个 tag。** 只在 `course` 顶端的话，
      学生 checkout 中间的 tag 时 L3 会 SKIP。流程：
      CI `workflow_dispatch` + `update_golden`（分支 `course`）→ 下载 artifact
      `golden-baselines` → 放进 `tests/golden/` → 注入历史。
      **基准图只能由 lavapipe 生成**，本机 NVIDIA 出的图不作数。

---

## E. 工具链（都真的浪费过时间）

- `.authoring/build.bat test` **只跑 ctest，不重新编译**。改了 `.cpp` 要先 `build`。
  因此误判过一次「断言没生效」。
- 直接跑 `pXX_tests.exe` 时，CTest 注入的 `RWB_GOLDEN_DIR` / `RWB_OUTPUT_DIR` **没有**，
  落盘的 `.actual.png` 会跑到别处。因此误判过一次「两张图完全相同」。
  ```bash
  export RWB_GOLDEN_DIR="D:/render-workbook/tests/golden" RWB_OUTPUT_DIR=/tmp/rwbout
  ```
- Bash heredoc 写大文件会 `unexpected EOF`，也会吃掉一层反斜杠。写 `.cpp` / 生成器脚本
  一律用 Write 工具或 Read+Edit。
- 文档做三方合并前先确认行尾：工作区是 CRLF、`git show` 出来是 LF，
  直接 `git merge-file` 会逐行冲突。用 `git show <ref>:<path>` 拿规范化版本。
- `.bat` 必须纯 ASCII + CRLF。Git Bash 会把 `/nologo` 当路径转换，MSVC 命令写进 `.bat` 跑。
- CI 的骨架检查**共用**构建目录：tag 有 35 个，每个都清空的话 glslang 要重编几十次。

---

## F. 发布

```bash
git status --porcelain                      # 必须为空
python .authoring/verify-grading.py pXX     # 完美对角线
cmd //c ".authoring\build.bat test"         # 全量绿
git push -f origin course work && git push -f origin --tags
```

`filter-branch` 之后本地的 `origin/*` 也被改写了，`git status` 会骗你说「已是最新」。照推。
`refs/original/` 是回滚路径，**确认远端没问题之前不要删**。
