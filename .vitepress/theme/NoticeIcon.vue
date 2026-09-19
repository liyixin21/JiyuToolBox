<script setup>
/**
 * 渲染 notice-icons.js 中的线性图标；描边统一取 currentColor，跟随主题深浅色。
 * 支持 path / rect / circle / line 等任意 SVG 形状，由图标定义里的 tag 决定。
 */
import { computed } from 'vue'
import { resolveNoticeIcon } from './notice-icons'

const props = defineProps({
  name: { type: String, default: 'board' },
  size: { type: [Number, String], default: 22 },
  strokeWidth: { type: [Number, String], default: 1.8 },
})

const icon = computed(() => resolveNoticeIcon(props.name))
</script>

<template>
  <svg
    class="VPNoticeIcon"
    :viewBox="icon.viewBox"
    :width="size"
    :height="size"
    fill="none"
    stroke="currentColor"
    :stroke-width="strokeWidth"
    stroke-linecap="round"
    stroke-linejoin="round"
    aria-hidden="true"
  >
    <component
      :is="shape.tag"
      v-for="(shape, index) in icon.shapes"
      :key="index"
      v-bind="shape.attrs"
    />
  </svg>
</template>

<style scoped>
.VPNoticeIcon {
  display: block;
}
</style>
