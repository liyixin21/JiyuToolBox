// https://vitepress.dev/guide/custom-theme
import { h } from 'vue'
import DefaultTheme from 'vitepress/theme'
import './style.css'
import Waline from '../../components/waline.vue'

/** @type {import('vitepress').Theme} */
export default {
  lastUpdated: true,
  extends: DefaultTheme,
  Layout: () => {
    return h(DefaultTheme.Layout, null, {
      'doc-after': () => h(Waline)
      // https://vitepress.dev/guide/extending-default-theme#layout-slots
    })
  },
  enhanceApp({ app }) {
    // ...
    app.component('Waline', Waline)
  }
}
