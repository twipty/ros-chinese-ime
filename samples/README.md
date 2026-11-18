# samples — 接入示例

这里的代码不是产品的一部分，是**证明"接入只需要加一个文件夹"的样品**。
每个样品都自己实现 `imfw::Scheme` / `imfw::Engine`，通过 `imfw::scheme_factory()`
挂进框架，不修改 `framework/` 一行。

| 目录 | 是什么 | 怎么跑 |
|---|---|---|
| `toy-scheme/` | 玩具方案：字母表 `a-z`，每个键出两个候选（原文 / 原文+`!`），零数据、纯 ASCII | `cmake -DROSPINYIN_SCHEME=toy` 编出玩具版 `rospinyin.ime`；`ctest -R toy-scheme` 跑宿主机契约测试 |

`toy-scheme` 是 X2-a0 的注入缝（`imfw::SchemeRegistry`）的第一个消费者。
判据（PLAN-MODULARIZATION G4）：接入它时 `framework/` 零 diff；哪天要改框架才能接，
就应该改框架（补耦合点），而不是改样品。

X3-a 会在这里补 `README` 的接入五步与常见坑，并把玩具方案打包成独立的示例目标。
