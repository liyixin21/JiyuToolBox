<!--
  Twikoo 评论组件
  ------------------------------------------------------------------
  后端：自托管 https://twikoo.liyixin.vip（Twikoo 2.0.17）
  参考：https://github.com/twikoojs/twikoo/tree/main/docs/.vitepress/theme

  与 Waline 的差异：
  1. Twikoo 2.x 没有 destroy()，切换路由时需手动清空容器再 init
  2. Twikoo 的暗色选择器是 .night / .darkmode / [data-theme=dark]，
     不认 VitePress 的 html.dark，故在容器上同步 data-theme
-->
<template>
  <div class="twikoo-wrapper">
    <div id="twikoo" :data-theme="isDark ? 'dark' : 'light'" />
  </div>
</template>

<script setup>
import { onMounted, watch, nextTick } from 'vue'
import { useRoute, useData } from 'vitepress'

const route = useRoute()
const { isDark } = useData()

// 自托管实例；换后端只改这一行
const ENV_ID = 'https://twikoo.liyixin.vip'

// 锁定 2.0.17，避免上游非兼容更新影响现网；国内优先 npmmirror
const CDN_PRIMARY = 'https://registry.npmmirror.com/twikoo/2.0.17/files/dist/twikoo.min.js'
const CDN_FALLBACK = 'https://cdn.jsdelivr.net/npm/twikoo@2.0.17/dist/twikoo.min.js'

let loaded = false
let loading = null

const injectScript = (src) =>
  new Promise((resolve, reject) => {
    const el = document.createElement('script')
    el.src = src
    el.async = true
    el.onload = resolve
    el.onerror = () => reject(new Error(`加载失败: ${src}`))
    document.head.appendChild(el)
  })

const loadTwikoo = () => {
  if (loaded) return Promise.resolve()
  if (loading) return loading
  loading = injectScript(CDN_PRIMARY)
    .catch((err) => {
      console.warn('[twikoo] 主 CDN 加载失败，回退 jsdelivr', err)
      return injectScript(CDN_FALLBACK)
    })
    .then(() => {
      loaded = true
    })
    .catch((err) => {
      loading = null
      throw err
    })
  return loading
}

const initTwikoo = async () => {
  await nextTick()
  const el = document.getElementById('twikoo')
  if (!el) return

  // 换页时先清空，否则 Twikoo 会在旧节点上重复挂载
  el.innerHTML = ''

  try {
    await loadTwikoo()
    window.twikoo.init({
      envId: ENV_ID,
      el: '#twikoo',
      path: route.path,
      lang: 'zh-CN',
    })
  } catch (err) {
    console.error('[twikoo] 初始化失败', err)
    el.innerHTML = '<p class="twikoo-error">评论加载失败，请检查网络后刷新重试。</p>'
  }
}

onMounted(initTwikoo)

// 路由切换：评论需要重新拉取对应页面的数据
watch(() => route.path, () => {
  initTwikoo()
})

// 暗色切换：Twikoo 靠容器上的 data-theme 生效
watch(isDark, (dark) => {
  const el = document.getElementById('twikoo')
  if (el) el.setAttribute('data-theme', dark ? 'dark' : 'light')
})
</script>

<style>
/* 与正文 .vp-doc 左右边界严格对齐：
   不再使用负边距，否则评论块会比正文宽出 16px 并左移 8px */
.twikoo-wrapper {
  width: 100%;
  max-width: 100% !important;
  margin: 24px 0 0;
}

/* Twikoo 输入行左侧的 .tk-avatar 占 56px，导致输入框比正文缩进。
   桌面端隐藏它并让内容列占满整行，使输入区与正文左右边缘齐平。
   注意 Twikoo 用的是自己的 .tk-* 类名，不是 Element 的 .el-* */
@media (min-width: 768px) {
  .twikoo .tk-submit .tk-avatar {
    display: none;
  }

  .twikoo .tk-submit .tk-row > .tk-col {
    flex: 1 1 auto;
    width: 100%;
  }

  /* 操作行（表情/预览/发送）原为避让头像留了 56px 左外边距，一并去掉 */
  .twikoo .tk-submit .tk-row.actions {
    margin-left: 0;
  }
}

.twikoo-error {
  padding: 16px;
  border-radius: 8px;
  background-color: var(--vp-c-bg-soft);
  color: var(--vp-c-text-2);
  font-size: 14px;
  text-align: center;
}

/* 让 Twikoo 的输入框、按钮配色跟随 VitePress 主题变量。
   注意：Twikoo 2.x 用的是自己的 tk-* 类名，不是 Element 的 el-* */
.twikoo .tk-input__inner,
.twikoo .tk-textarea__inner {
  background-color: var(--vp-c-bg-soft);
  border-color: var(--vp-c-divider);
  color: var(--vp-c-text-1);
}

.twikoo .tk-input-group__prepend {
  background-color: var(--vp-c-bg-soft);
  border-color: var(--vp-c-divider);
  color: var(--vp-c-text-2);
}

.twikoo .tk-comment,
.twikoo .tk-submit {
  border-color: var(--vp-c-divider);
}

.twikoo .tk-content {
  color: var(--vp-c-text-2);
}
</style>
