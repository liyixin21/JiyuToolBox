<template>
  <div class="waline-wrapper">
    <div id="waline"></div>
  </div>
</template>

<script setup>
import { onMounted, watch, nextTick } from 'vue';
import { useRoute, useData } from 'vitepress';

const route = useRoute();
const { isDark } = useData();

let walineInstance = null;

// 主站 CDN 优先，失败时回退到备用 CDN（两者均为 Waline v3 同版本产物）
const WALINE_CDN = 'https://cdn.jsdelivr.net/npm/@waline/client@v3/dist/waline.js';
const WALINE_CDN_FALLBACK = 'https://unpkg.com/@waline/client@v3/dist/waline.js';

const loadWalineClient = async () => {
  try {
    return await import(/* @vite-ignore */ WALINE_CDN);
  } catch (err) {
    console.warn('[waline] 主 CDN 加载失败，回退备用 CDN', err);
    return await import(/* @vite-ignore */ WALINE_CDN_FALLBACK);
  }
};

const initWaline = async () => {
  // 销毁旧实例
  if (walineInstance) {
    walineInstance.destroy?.();
    document.querySelector('#waline') && (document.querySelector('#waline').innerHTML = '');
  }

  await nextTick();

  const { init } = await loadWalineClient();
  walineInstance = init({
    el: '#waline',
    serverURL: 'https://waline.liyixin.vip',
    lang: 'zh-CN',
    path: route.path,
    dark: isDark.value,
    emoji: [
      'https://cdn.jsdelivr.net/gh/walinejs/emojis@1.0.0/alus',
      'https://cdn.jsdelivr.net/gh/walinejs/emojis@1.0.0/qq',
      'https://cdn.jsdelivr.net/gh/walinejs/emojis@1.0.0/tieba',
      'https://cdn.jsdelivr.net/gh/walinejs/emojis@1.0.0/tw-emoji',
    ],
  });
};

onMounted(() => {
  initWaline();
});

// 路由变化时重新初始化
watch(() => route.path, () => {
  initWaline();
});

// 暗黑模式切换
watch(isDark, () => {
  initWaline();
});
</script>

<style>
.waline-wrapper {
  max-width: 800px !important;
  margin: 10px -8px;
  padding: 0rem 0rem;
}
</style>
