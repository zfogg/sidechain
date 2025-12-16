import { useEffect } from 'react'
import { Channel, MessageList, MessageInput, Thread, Window, ChannelHeader } from 'stream-chat-react'
import { useChatContext } from 'stream-chat-react'

interface MessageThreadProps {
  channelId: string
}

/**
 * MessageThread - Main message viewing and input area
 * Shows message list, allows composing replies and uploading files
 */
export function MessageThread({ channelId }: MessageThreadProps) {
  const { client } = useChatContext()

  useEffect(() => {
    // Watch channel for new messages and updates
    const channel = client?.channel('messaging', channelId)
    if (channel) {
      channel.watch()
    }
  }, [channelId, client])

  return (
    <div className="h-full flex flex-col bg-background">
      {/* Channel Header with info */}
      <div className="border-b border-border bg-card/95 backdrop-blur sticky top-0 z-10">
        <ChannelHeader />
      </div>

      {/* Window manages layout for message thread + thread panel */}
      <Window>
        {/* Main Messages */}
        <div className="flex-1 overflow-hidden">
          <MessageList />
        </div>

        {/* Thread panel for replies */}
        <Thread />

        {/* Message Input */}
        <div className="border-t border-border bg-card/95 backdrop-blur p-4">
          <MessageInput />
        </div>
      </Window>
    </div>
  )
}
