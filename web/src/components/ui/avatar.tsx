import { useState } from 'react'
import { createImagePlaceholder } from '@/utils/imageOptimization'

interface AvatarProps {
  src?: string
  alt: string
  size?: 'sm' | 'md' | 'lg' | 'xl'
  className?: string
  lazy?: boolean
}

const sizeMap = {
  sm: 'w-8 h-8',
  md: 'w-10 h-10',
  lg: 'w-16 h-16',
  xl: 'w-24 h-24',
}

/**
 * Avatar - Lazy-loaded image component with fallback
 * Supports local fallback placeholder
 */
export function Avatar({ src, alt, size = 'md', className = '', lazy = true }: AvatarProps) {
  const [imageSrc, setImageSrc] = useState(src || createImagePlaceholder())
  const [isLoading, setIsLoading] = useState(!!src)

  const handleLoad = () => {
    setIsLoading(false)
  }

  const handleError = () => {
    setImageSrc(createImagePlaceholder())
    setIsLoading(false)
  }

  return (
    <div className={`${sizeMap[size]} rounded-full bg-muted overflow-hidden flex-shrink-0 ${className}`}>
      <img
        src={imageSrc}
        alt={alt}
        loading={lazy ? 'lazy' : 'eager'}
        onLoad={handleLoad}
        onError={handleError}
        className={`w-full h-full object-cover ${isLoading ? 'animate-pulse' : ''}`}
      />
    </div>
  )
}
