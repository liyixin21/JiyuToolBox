<template>
    <div class="waline-wrapper">
      <Waline
        :serverURL="serverURL"
        :path="currentPath"
        :dark="isDark"
        :emoji="emoji"
      />
    </div>
  </template>
  
  <script setup>
  import { Waline } from '@waline/client/component';
  import '@waline/client/dist/waline.css';
  import { computed } from 'vue';
  import { useRoute, useData } from 'vitepress';
  
  // Waline 服务端地址（必须修改！）
  const serverURL = 'https://waline.liyixin.vip';
  
  // 动态路径（自动根据路由更新）
  const route = useRoute();
  const currentPath = computed(() => route.path);
  
  // 暗黑模式适配
  const { isDark } = useData();
  
  // 表情包配置（可选）
  const emoji = [
    'https://cdn.jsdelivr.net/gh/walinejs/emojis@1.0.0/alus',
    'https://cdn.jsdelivr.net/gh/walinejs/emojis@1.0.0/qq',
    'https://cdn.jsdelivr.net/gh/walinejs/emojis@1.0.0/tieba',
    'https://cdn.jsdelivr.net/gh/walinejs/emojis@1.0.0/tw-emoji',
  ];
  </script>
  
  <style>
  /* 限制评论框最大宽度，避免在大屏幕上过宽 */
  .waline-wrapper {
  max-width: 800px !important; /* 自定义宽度 */
  margin: 10px -8px;
  padding: 0rem 0rem;
  }

  </style>