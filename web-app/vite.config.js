import { defineConfig } from 'vite';
import { svelte } from '@sveltejs/vite-plugin-svelte';

const ESP32_IP = process.env.ESP32_IP || 'studybud.local';

export default defineConfig({
  plugins: [svelte()],
  server: {
    proxy: {
      '/ws': {
        target: `ws://${ESP32_IP}`,
        ws: true,
      },
      '/api': {
        target: `http://${ESP32_IP}`,
      },
    },
  },
});
