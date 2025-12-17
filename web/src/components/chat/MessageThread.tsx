import { useEffect, useState } from 'react'
import { MessageList, MessageInput, ChannelHeader } from 'stream-chat-react'
import { useChatContext } from 'stream-chat-react'

interface MessageThreadProps {
  channelId: string
}

/**
 * MessageThread - Main message viewing and input area
 * Shows message list, allows composing replies and uploading files
 */
export function MessageThread({ channelId }: MessageThreadProps) {
  const { channel: contextChannel } = useChatContext()
  const [isReady, setIsReady] = useState(false)

  useEffect(() => {
    // The channel should already be set in the Channel component wrapper
    // Just ensure it's watched for real-time updates
    if (contextChannel) {
      contextChannel.watch().then(() => {
        setIsReady(true)
      }).catch((err) => {
        console.error('Failed to watch channel:', err)
        setIsReady(true) // Set ready anyway to show any error state
      })
    }
  }, [channelId, contextChannel])

  if (!isReady) {
    return (
      <div className="h-full w-full flex items-center justify-center" style={{ backgroundColor: '#26262c' }}>
        <p className="text-foreground">Initializing channel...</p>
      </div>
    )
  }

  return (
    <div className="h-full w-full flex flex-col overflow-hidden" style={{ backgroundColor: '#26262c' }}>
      {/* Channel Header */}
      <div className="flex-shrink-0 border-b border-border overflow-hidden" style={{ backgroundColor: '#2e2e34' }}>
        <ChannelHeader />
      </div>

      {/* Messages and Input Container */}
      <div className="flex-1 min-h-0 overflow-x-hidden w-full flex flex-col">
        {/* Messages */}
        <div className="flex-1 overflow-auto w-full">
          <MessageList />
        </div>

        {/* Message Input */}
        <div className="flex-shrink-0 w-full" style={{ backgroundColor: '#2e2e34' }}>
          <MessageInput />
        </div>
      </div>
    </div>
  )
}
