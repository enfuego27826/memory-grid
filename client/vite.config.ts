import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

// Served same-origin from mg_server in production; in dev, proxy the API + WS
// to the C++ backend so the client uses relative URLs everywhere.
export default defineConfig({
  plugins: [react()],
  server: {
    proxy: {
      '/create': 'http://localhost:9001',
      '/join': 'http://localhost:9001',
      '/ws': { target: 'ws://localhost:9001', ws: true },
    },
  },
  build: { outDir: 'dist', chunkSizeWarningLimit: 1200 },
});
