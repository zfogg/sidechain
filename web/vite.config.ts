import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'
import path from 'path'

// https://vite.dev/config/
export default defineConfig({
  plugins: [react()],
  resolve: {
    alias: {
      '@': path.resolve(__dirname, './src'),
    },
  },
  server: {
    proxy: {
      '/api': {
        target: process.env.VITE_API_URL || 'http://localhost:8787',
        changeOrigin: true,
        ws: true,
      }
    }
  },
  build: {
    rollupOptions: {
      output: {
        manualChunks: {
          'stream-io': ['stream-chat', 'stream-chat-react'],
          'audio': ['howler'],
          'vendor': ['react', 'react-dom', 'react-router-dom'],
          'query': ['@tanstack/react-query'],
        }
      }
    }
  }
})
