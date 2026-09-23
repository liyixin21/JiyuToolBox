<script setup>
/**
 * 公告面板内容（标题、正文、链接、右下角操作按钮）。
 * 被 Notice.vue 在"置顶弹窗"与"悬浮窗"两种模式下复用，避免重复模板。
 */
import { withBase } from 'vitepress'

defineProps({
  tag: { type: String, default: '' },
  title: { type: String, default: '' },
  paragraphs: { type: Array, default: () => [] },
  links: { type: Array, default: () => [] },
  // 可选配图，渲染在正文下方（比如正文最后一行是群号时的二维码）
  image: { type: Object, default: null },
  // 'modal'：显示"关闭"与"不再显示"；'floating'：只显示"收起"，避免误点持久隐藏
  mode: { type: String, default: 'modal' },
})

const emit = defineEmits(['go', 'close', 'never', 'collapse'])
</script>

<template>
  <span v-if="tag" class="VPNotice-tag">{{ tag }}</span>

  <h3 v-if="title" id="vp-notice-title" class="VPNotice-title">{{ title }}</h3>

  <div class="VPNotice-body">
    <p v-for="(paragraph, index) in paragraphs" :key="index" class="VPNotice-text">{{ paragraph }}</p>
  </div>

  <img
    v-if="image && image.src"
    class="VPNotice-image"
    :class="{ 'is-modal': mode === 'modal' }"
    :src="image.src"
    :alt="image.alt || ''"
    :width="mode === 'modal' ? (image.width || 180) : (image.widthSmall || 150)"
  />

  <div v-if="links.length" class="VPNotice-links">
    <a
      v-for="(item, index) in links"
      :key="index"
      class="VPNotice-link"
      :class="item.theme === 'alt' ? 'is-alt' : 'is-brand'"
      :href="withBase(item.link || '#')"
      @click.prevent="emit('go', item.link)"
    >{{ item.text }}</a>
  </div>

  <!-- 右下角操作区 -->
  <div class="VPNotice-actions">
    <!-- 悬浮窗：只有"收起"，避免在这里误触持久隐藏 -->
    <button
      v-if="mode === 'floating'"
      class="VPNotice-action is-primary"
      type="button"
      @click="emit('collapse')"
    >收起</button>

    <!-- 置顶弹窗：不再显示在左，关闭在最右 -->
    <template v-else>
      <button class="VPNotice-action" type="button" @click="emit('never')">不再显示</button>
      <button class="VPNotice-action is-primary" type="button" @click="emit('close')">关闭</button>
    </template>
  </div>
</template>

<style scoped>
.VPNotice-tag {
  align-self: flex-start;
  padding: 2px 8px;
  border-radius: 6px;
  background-color: var(--vp-c-brand-soft);
  color: var(--vp-c-brand-1);
  font-size: 12px;
  font-weight: 600;
  line-height: 20px;
}

.VPNotice-title {
  margin: 0;
  padding: 0;
  border: none;
  color: var(--vp-c-text-1);
  font-size: 16px;
  font-weight: 700;
  line-height: 1.5;
}

.VPNotice-body {
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.VPNotice-text {
  margin: 0;
  color: var(--vp-c-text-2);
  font-size: 14px;
  line-height: 1.7;
}

.VPNotice-image {
  align-self: flex-start;
  height: auto;
  border-radius: 8px;
}

/* 弹窗是居中大面板，配图可略大一些 */
.VPNotice-image.is-modal {
  align-self: center;
}

.VPNotice-links {
  display: flex;
  flex-wrap: wrap;
  gap: 10px;
  margin-top: 2px;
}

.VPNotice-link {
  display: inline-block;
  padding: 0 16px;
  border: 1px solid transparent;
  border-radius: 20px;
  font-size: 14px;
  font-weight: 600;
  line-height: 34px;
  white-space: nowrap;
  transition: color 0.25s, border-color 0.25s, background-color 0.25s;
}

.VPNotice-link.is-brand {
  background-color: var(--vp-c-brand-1);
  color: var(--vp-c-white);
}

.VPNotice-link.is-brand:hover {
  background-color: var(--vp-c-brand-2);
}

.VPNotice-link.is-alt {
  border-color: var(--vp-c-divider);
  background-color: var(--vp-c-bg-soft);
  color: var(--vp-c-text-1);
}

.VPNotice-link.is-alt:hover {
  border-color: var(--vp-c-brand-1);
  color: var(--vp-c-brand-1);
}

.VPNotice-actions {
  display: flex;
  justify-content: flex-end;
  gap: 8px;
  margin-top: 6px;
  padding-top: 12px;
  border-top: 1px solid var(--vp-c-divider);
}

.VPNotice-action {
  padding: 0 14px;
  border: 1px solid var(--vp-c-divider);
  border-radius: 8px;
  background-color: transparent;
  color: var(--vp-c-text-2);
  font-size: 13px;
  font-weight: 500;
  line-height: 30px;
  white-space: nowrap;
  cursor: pointer;
  transition: color 0.25s, border-color 0.25s, background-color 0.25s;
}

.VPNotice-action:hover {
  border-color: var(--vp-c-brand-1);
  color: var(--vp-c-brand-1);
}

.VPNotice-action.is-primary {
  border-color: transparent;
  background-color: var(--vp-c-brand-1);
  color: var(--vp-c-white);
}

.VPNotice-action.is-primary:hover {
  background-color: var(--vp-c-brand-2);
  color: var(--vp-c-white);
}
</style>
