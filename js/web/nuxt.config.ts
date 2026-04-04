// https://nuxt.com/docs/api/configuration/nuxt-config
export default defineNuxtConfig({
  modules: [
    '@nuxt/eslint',
    '@nuxt/ui',
  ],

  imports: {
    dirs: ['types'],
  },

  devtools: {
    enabled: true,
  },

  css: ['~/assets/css/main.css'],

  compatibilityDate: '2025-01-15',

  nitro: {
    experimental: {
      websocket: true,
    },
  },

  eslint: {
    config: {
      stylistic: {
        braceStyle: '1tbs',
        semi: true,
      },
    },
  },
});
