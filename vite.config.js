import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

export default defineConfig({
  plugins: [react()],
  base: '/Rhapsodia_Literaria_7.0/',   // MUST MATCH NEW REPO NAME
  build: {
    outDir: 'docs'                    // REQUIRED for GitHub Pages
  }
})
