import { useParams } from 'react-router-dom'
import { Channel } from 'stream-chat-react'
import { useChatContext } from 'stream-chat-react'
import { useEffect, useState } from 'react'
import { ChannelList } from '@/components/chat/ChannelList'
import { MessageThread } from '@/components/chat/MessageThread'
import { Spinner } from '@/components/ui/spinner'

/**
 * Messages - Direct messaging page
 *
 * Features:
 * - Channel list sidebar with conversations
 * - Message thread view with inline replies
 * - File uploads via message input
 * - Typing indicators
 * - Reactions on messages
 * - Search within conversation
 */
export function Messages() {
  const { channelId } = useParams<{ channelId: string }>()
  const { client } = useChatContext()
  const [channel, setChannel] = useState<any>(null)
  const [isLoading, setIsLoading] = useState(!!channelId)

  useEffect(() => {
    if (!channelId || !client) return

    const loadChannel = async () => {
      try {
        const ch = client.channel('messaging', channelId)
        await ch.watch()
        setChannel(ch)
      } catch (error) {
        console.error('Failed to load channel:', error)
      } finally {
        setIsLoading(false)
      }
    }

    loadChannel()
  }, [channelId, client])

  return (
    <div className="min-h-screen bg-background flex">
      {/* Channel List Sidebar */}
      <div className="w-80 border-r border-border hidden lg:flex flex-col">
        <ChannelList />
      </div>

      {/* Main Content */}
      <div className="flex-1 flex flex-col">
        {channelId ? (
          isLoading ? (
            <div className="flex-1 flex items-center justify-center">
              <Spinner size="lg" />
            </div>
          ) : channel ? (
            <Channel channel={channel}>
              <MessageThread channelId={channelId} />
            </Channel>
          ) : (
            <div className="flex-1 flex items-center justify-center">
              <div className="text-center space-y-4">
                <div className="text-4xl">💬</div>
                <div className="text-muted-foreground">Failed to load conversation</div>
              </div>
            </div>
          )
        ) : (
          <div className="flex-1 flex items-center justify-center">
            <div className="text-center space-y-4">
              <div className="text-4xl">💬</div>
              <div className="text-muted-foreground text-lg">
                Select a conversation to start messaging
              </div>
              <p className="text-sm text-muted-foreground max-w-sm">
                Choose from your existing conversations or find users you want to message
              </p>
            </div>
          </div>
        )}
      </div>

      {/* Mobile Channel List - Overlay */}
      <div className="lg:hidden fixed inset-0 pointer-events-none z-40">
        {/* Could add a slide-out drawer here for mobile */}
      </div>
    </div>
  )
}
