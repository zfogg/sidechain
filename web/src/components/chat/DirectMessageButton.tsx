import { useState } from 'react'
import { useNavigate } from 'react-router-dom'
import { ChatClient } from '@/api/ChatClient'
import { Button } from '@/components/ui/button'

interface DirectMessageButtonProps {
  userId: string
  displayName?: string
}

/**
 * DirectMessageButton - Opens direct message with a user
 * Used in profile pages and user cards
 */
export function DirectMessageButton({ userId }: DirectMessageButtonProps) {
  const navigate = useNavigate()
  const [isLoading, setIsLoading] = useState(false)

  const handleStartDM = async () => {
    setIsLoading(true)
    try {
      const result = await ChatClient.getOrCreateDirectChannel(userId)

      if (result.isOk()) {
        navigate(`/messages/${result.getValue()}`)
      } else {
        console.error('Failed to create DM channel:', result.getError())
      }
    } finally {
      setIsLoading(false)
    }
  }

  return (
    <Button
      onClick={handleStartDM}
      disabled={isLoading}
      variant="outline"
      className="gap-2"
    >
      <span>💬</span>
      {isLoading ? 'Opening...' : 'Message'}
    </Button>
  )
}
