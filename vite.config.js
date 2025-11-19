import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

export default defineConfig({
  plugins: [react()],
  base: '/Rhapsodia_Literaria_70/',
  build: {
    outDir: 'docs',   // VERY IMPORTANT
  }
})

