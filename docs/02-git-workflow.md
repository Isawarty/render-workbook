# Git 工作流

## 为什么不是「每个 task 一个分支」

一个直觉的方案是每道题一个分支、答案再一个分支。但它有两个问题：

1. **分支隔离是伪命题。** task N 的骨架必然包含 task N-1 的正确实现（否则代码跑不起来），
   所以切到 task N 的分支时你已经看到了上一题的答案。
2. 40 个 task 会变成 80 个分支。

所以本仓库用 **2 个长期分支 + tag**。

## 分支与 tag

```
course   我（生成方）维护的内容，线性历史，不要在上面提交
  tag  start/p01-t03    该 task 的骨架 = 上一 task 的参考实现 + 新的 TODO + 新测试
  tag  done/p01-t03     该 task 的参考实现

work     你的分支
  tag  mine/p01-t03     你自己实现的存档
```

## 一道题的四步

```bash
# 1. 领题。它已经包含 t01/t02 的官方基线。
git checkout -B work start/p01-t03

# 2. 填 TODO，然后判分
python rwb.py test p01-t03

# 3. 对答案（可以选择不看）
git diff done/p01-t03 -- projects/p01-triangle/src/steps/03_swapchain.cpp

# 4. 存档自己的版本，进下一题
git add -A && git commit -m "p01-t03: my impl"
git tag mine/p01-t03
git checkout -B work start/p01-t04
```

第 4 步的 `git checkout -B work start/p01-t04` 会**覆盖**你刚写的实现，
换成官方基线。这是刻意的设计，见下节。

## 为什么用官方基线

每道题结束后切回参考实现，代价是最终框架不是逐行「完全自己写的」，
回报是两条：

- **测试能真判分。** 基线确定，golden image 才有意义。
- **不会因为 t03 写歪导致 t12 崩。** 前面的架构偏差会在后面以极难定位的方式爆炸。
  CMU 15-462、Stanford CS248 这类课程的作业框架都是这个模式。

`mine/*` tag 完整保留了你每一题的实现。做作品集叙事时，
`git log --oneline --tags='mine/*'` 就是你的施工记录。

**例外**：P5（Render Graph）和 P6（Lua 嵌入）的架构设计部分不给强基线，
只给接口契约和测试。那两段是真正自由发挥的。

## 常用命令

```bash
# 看所有题目
git tag -l 'start/*'

# 我现在在哪一题
git describe --tags --match 'start/*' --abbrev=0

# 只看某一题改了哪些文件
git diff --stat start/p01-t03 done/p01-t03

# 对答案但只看某个函数
git diff done/p01-t07 -- projects/p01-triangle/src/steps/07_sync.cpp

# 完全放弃当前实现，重新领题
git checkout -B work start/p01-t07
```

## 推到 GitHub

```bash
git remote add origin git@github.com:<你的用户名>/render-workbook.git
git push -u origin course work
git push origin --tags
```

`course` 分支的 CI 必须全绿（它验证参考实现本身正确）。
`work` 分支的红叉是正常的 —— 那是你的进度条。

## 三台机器之间同步

```bash
# 换机器时
git push origin work --tags        # 走之前
git fetch --all --tags && git checkout work   # 到了之后
```

`build/` 和 `.authoring/` 都在 `.gitignore` 里，不会跟着跑。
到新机器上重新 `cmake --preset ...` 即可，依赖会按锁定的 SHA256 重新拉取。
