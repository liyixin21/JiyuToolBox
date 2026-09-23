<script setup>
/**
 * 站点公告：按 .vitepress/notice.mjs 配置渲染。
 *   mode = 'modal'    置顶弹窗：右下角显示"不再显示"与"关闭"
 *   mode = 'floating' 悬浮窗：右下角只显示"收起"
 *
 * 面板隐藏后，右下角固定保留一个圆形图标按钮，点击即可重新打开公告（并撤回"不再显示"）；
 * 切换页面也会保留，位置始终在右下角。
 */
import { computed, onBeforeUnmount, onMounted, watch } from 'vue'
import { useRoute, useRouter } from 'vitepress'
import { useScrollLock } from '@vueuse/core'
import notice from '../notice.mjs'
import {
  closeNotice,
  collapseNotice,
  isCollapsed,
  isDismissed,
  isNoticeEnabled,
  neverShowNotice,
  openNotice,
  readMemory,
} from './notice-state'
import NoticeIcon from './NoticeIcon.vue'
import NoticePanel from './NoticePanel.vue'

const route = useRoute()
const router = useRouter()
let timer = null

const isExcluded = computed(() => {
  const paths = notice.excludePaths || []
  if (!paths.length) return false
  const current = String(route.path || '/').replace(/\.html$/, '')
  return paths.some((item) => {
    const target = String(item).replace(/\.html$/, '')
    return target === current || (target !== '/' && current.startsWith(target))
  })
})

const isDialog = computed(() => notice.mode === 'modal')
const active = computed(() => isNoticeEnabled && !isExcluded.value)

// 悬浮窗收起后不显示面板；置顶弹窗无"收起"概念
const panelVisible = computed(
  () => active.value && !isDismissed.value && (isDialog.value || !isCollapsed.value)
)
// 面板不可见时，右下角保留唤起入口——这正是"关闭后还能重新打开"的入口
const triggerVisible = computed(() => active.value && !panelVisible.value)

const isLocked = useScrollLock(isDialog.value && typeof document !== 'undefined' ? document.body : null)

watch(panelVisible, (shown) => {
  // 仅置顶弹窗锁定页面滚动
  isLocked.value = isDialog.value && shown
})

onMounted(() => {
  window.addEventListener('keydown', handleKeydown)
  if (!isNoticeEnabled || isExcluded.value || readMemory()) return
  timer = window.setTimeout(() => {
    isDismissed.value = false
    isCollapsed.value = !!notice.floating?.defaultCollapsed
  }, Number(notice.delay) || 0)
})

onBeforeUnmount(() => {
  window.removeEventListener('keydown', handleKeydown)
  if (timer) window.clearTimeout(timer)
})

const handleKeydown = (event) => {
  if (event.key !== 'Escape' || !panelVisible.value) return
  if (notice.modal?.closeOnEsc !== true) return
  closeNotice()
}

const handleBackdropClick = () => {
  if (notice.modal?.closeOnOverlayClick === true) closeNotice()
}

const paragraphs = computed(() => {
  const raw = notice.content
  if (!raw) return []
  return Array.isArray(raw) ? raw : [raw]
})

const links = computed(() => notice.links || [])
const noticeImage = computed(() => (notice.image && notice.image.src ? notice.image : null))
const showTag = computed(() => Boolean(notice.tag))
const position = computed(() => notice.floating?.position === 'bottom-left' ? 'is-left' : 'is-right')
const offset = computed(() => notice.floating?.offset ?? 24)
const panelStyle = computed(() => isDialog.value ? { maxWidth: `${notice.modal?.width || 480}px` } : null)
// 注意：computed 内部必须显式取 .value，否则 CSS 变量会变成 "[object Object]px"
const wrapperStyle = computed(() => ({ '--vp-notice-offset': `${offset.value}px` }))

const go = (link) => {
  if (!link) return
  // 置顶弹窗带遮罩会挡住页面，任何跳转前都先关掉；悬浮窗不遮挡阅读，保持展开
  if (isDialog.value) closeNotice()
  // 外链（如备用下载）在新标签页打开，当前页保持不动
  if (/^(https?:)?\/\//.test(link)) {
    window.open(link, '_blank', 'noopener')
    return
  }
  router.go(link)
}
</script>

<template>
  <Teleport to="body">
    <!-- 置顶弹窗：遮罩 + 居中面板 -->
    <Transition name="vp-notice">
      <div
        v-if="panelVisible && isDialog"
        class="VPNotice is-modal"
        role="dialog"
        aria-modal="true"
        aria-live="polite"
        :aria-labelledby="notice.title ? 'vp-notice-title' : undefined"
      >
        <div class="VPNotice-backdrop" @click="handleBackdropClick" />

        <div class="VPNotice-panel is-modal-panel" :style="panelStyle">
          <NoticePanel
            :tag="showTag ? notice.tag : ''"
            :title="notice.title"
            :paragraphs="paragraphs"
            :links="links"
            :image="noticeImage"
            mode="modal"
            @go="go"
            @close="closeNotice"
            @never="neverShowNotice"
            @collapse="collapseNotice"
          />
        </div>
      </div>
    </Transition>

    <!-- 悬浮窗面板：仅 floating 模式 -->
    <Transition name="vp-notice">
      <div
        v-if="panelVisible && !isDialog"
        class="VPNotice is-floating"
        :class="position"
        :style="wrapperStyle"
        role="status"
        aria-live="polite"
        :aria-labelledby="notice.title ? 'vp-notice-title' : undefined"
      >
        <div class="VPNotice-panel">
          <NoticePanel
            :tag="showTag ? notice.tag : ''"
            :title="notice.title"
            :paragraphs="paragraphs"
            :links="links"
            :image="noticeImage"
            mode="floating"
            @go="go"
            @close="closeNotice"
            @never="neverShowNotice"
            @collapse="collapseNotice"
          />
        </div>
      </div>
    </Transition>

    <!-- 右下角圆形图标：任何模式下关闭/收起后都保留，点击重新打开 -->
    <Transition name="vp-notice">
      <div
        v-if="triggerVisible"
        class="VPNotice is-floating"
        :class="position"
        :style="wrapperStyle"
      >
        <button
          class="VPNotice-trigger"
          type="button"
          aria-label="查看公告"
          title="查看公告"
          @click="openNotice()"
        >
          <NoticeIcon class="VPNotice-trigger-icon" :name="notice.icon" :size="24" :stroke-width="1.7" />
        </button>
      </div>
    </Transition>
  </Teleport>
</template>

<style scoped>
.VPNotice {
  position: fixed;
  z-index: 60;
}

/* ---------- 置顶弹窗 ---------- */
.VPNotice.is-modal {
  inset: 0;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 20px;
}

.VPNotice-backdrop {
  position: absolute;
  inset: 0;
  background-color: rgba(0, 0, 0, 0.5);
  backdrop-filter: blur(2px);
}

/* ---------- 右下角位置 ---------- */
.VPNotice.is-floating {
  bottom: var(--vp-notice-offset, 24px);
}

.VPNotice.is-floating.is-right {
  right: var(--vp-notice-offset, 24px);
}

.VPNotice.is-floating.is-left {
  left: var(--vp-notice-offset, 24px);
}

/* ---------- 面板外壳 ---------- */
.VPNotice-panel {
  position: relative;
  display: flex;
  flex-direction: column;
  gap: 10px;
  width: min(340px, calc(100vw - 48px));
  padding: 18px 20px 14px;
  border: 1px solid var(--vp-c-divider);
  border-radius: 12px;
  background-color: var(--vp-c-bg-elv);
  box-shadow: var(--vp-shadow-4);
  text-align: left;
}

.VPNotice-panel.is-modal-panel {
  width: 100%;
  gap: 14px;
  padding: 24px 26px 18px;
  box-shadow: var(--vp-shadow-5);
}

/* ---------- 圆形唤起按钮 ---------- */
.VPNotice-trigger {
  display: flex;
  align-items: center;
  justify-content: center;
  width: 46px;
  height: 46px;
  padding: 0;
  border: 1px solid var(--vp-c-divider);
  border-radius: 50%;
  background-color: var(--vp-c-bg-elv);
  color: var(--vp-c-brand-1);
  box-shadow: var(--vp-shadow-3);
  cursor: pointer;
  transition: border-color 0.25s, color 0.25s, transform 0.25s;
}

.VPNotice-trigger:hover {
  border-color: var(--vp-c-brand-1);
  transform: translateY(-2px);
}

.VPNotice-trigger-icon {
  display: block;
}

/* ---------- 出现动画 ---------- */
.vp-notice-enter-active,
.vp-notice-leave-active {
  transition: opacity 0.25s ease;
}

.vp-notice-enter-from,
.vp-notice-leave-to {
  opacity: 0;
}

@media (prefers-reduced-motion: reduce) {
  .vp-notice-enter-active,
  .vp-notice-leave-active,
  .VPNotice-trigger {
    transition: none;
  }
}

@media (max-width: 640px) {
  .VPNotice.is-floating {
    --vp-notice-offset: 16px;
  }
}
</style>
