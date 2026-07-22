<script setup>
import DefaultTheme from 'vitepress/theme'
import Waline from '../components/waline.vue';  // 注意大小写
    
const { Layout } = DefaultTheme
</script>

<template>
  <Layout>
    <template #doc-after>
      <Waline />
    </template>
  </Layout>
</template>