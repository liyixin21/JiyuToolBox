// https://vitepress.dev/guide/custom-theme
import { h } from 'vue'
import DefaultTheme from 'vitepress/theme'
import './style.css'
import Waline from '../../components/waline.vue'
import Notice from './Notice.vue'

/** @type {import('vitepress').Theme} */
export default {
  lastUpdated: true,
  extends: DefaultTheme,
  Layout: () => {
    return h(DefaultTheme.Layout, null, {
      // 公告挂在 layout-top，内部用 Teleport 挂到 body 并覆盖整个视口
      'layout-top': () => h(Notice),
      'doc-after': () => h(Waline)
      // https://vitepress.dev/guide/extending-default-theme#layout-slots
    })
  },
  enhanceApp({ app }) {
    // ...
    app.component('Waline', Waline)
  }
}
