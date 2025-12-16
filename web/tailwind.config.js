/** @type {import('tailwindcss').Config} */
export default {
  darkMode: 'class',
  content: [
    './index.html',
    './src/**/*.{js,ts,jsx,tsx}',
  ],
  theme: {
    extend: {
      colors: {
        // From C++ plugin Colors.h
        'coral-pink': '#FD7792',
        'rose-pink': '#EDA1C1',
        'lavender': '#DCAEE8',
        'sky-blue': '#B4E4FF',
        'soft-blue': '#95BDFF',
        'cream-yellow': '#F8F7BA',
        'mint-green': '#B5FFC7',

        // Dark theme backgrounds (from C++ plugin)
        'bg-primary': '#1C1C20',
        'bg-secondary': '#26262C',
        'bg-tertiary': '#2E2E34',
        'bg-hover': '#363640',

        // Semantic colors
        border: 'hsl(var(--border))',
        input: 'hsl(var(--input))',
        ring: 'hsl(var(--ring))',
        background: 'hsl(var(--background))',
        foreground: 'hsl(var(--foreground))',
        primary: {
          DEFAULT: '#FD7792', // coral-pink
          foreground: '#FFFFFF',
        },
        secondary: {
          DEFAULT: '#EDA1C1', // rose-pink
          foreground: '#FFFFFF',
        },
        accent: {
          DEFAULT: '#DCAEE8', // lavender
          foreground: '#FFFFFF',
        },
        destructive: {
          DEFAULT: 'hsl(var(--destructive))',
          foreground: 'hsl(var(--destructive-foreground))',
        },
        muted: {
          DEFAULT: 'hsl(var(--muted))',
          foreground: 'hsl(var(--muted-foreground))',
        },
        popover: {
          DEFAULT: 'hsl(var(--popover))',
          foreground: 'hsl(var(--popover-foreground))',
        },
        card: {
          DEFAULT: 'hsl(var(--card))',
          foreground: 'hsl(var(--card-foreground))',
        },
      },
      borderRadius: {
        lg: 'var(--radius)',
        md: 'calc(var(--radius) - 2px)',
        sm: 'calc(var(--radius) - 4px)',
      },
      fontFamily: {
        sans: ['Inter', 'system-ui', 'sans-serif'],
        mono: ['JetBrains Mono', 'Menlo', 'monospace'],
      },
      animation: {
        'waveform': 'waveform 2s ease-in-out infinite',
        'pulse-slow': 'pulse 3s cubic-bezier(0.4, 0, 0.6, 1) infinite',
      },
      keyframes: {
        waveform: {
          '0%, 100%': { transform: 'scaleY(1)' },
          '50%': { transform: 'scaleY(1.2)' },
        },
      },
    },
  },
  plugins: [
    require('@tailwindcss/forms'),
    require('@tailwindcss/typography'),
  ],
}
