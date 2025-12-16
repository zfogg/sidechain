import { useParams } from 'react-router-dom'
import { Channel, Chat, useChatContext } from 'stream-chat-react'
import { useEffect, useState } from 'react'
import { ChannelList } from '@/components/chat/ChannelList'
import { MessageThread } from '@/components/chat/MessageThread'
import { NewMessageDialog } from '@/components/chat/NewMessageDialog'
import { Spinner } from '@/components/ui/spinner'
import { Button } from '@/components/ui/button'

/**
 * MessagesContent - Inner component that uses Chat context
 * Must be wrapped in Chat component to use useChatContext
 */
function MessagesContent() {
  const { channelId } = useParams<{ channelId: string }>()
  const { client } = useChatContext()
  const [channel, setChannel] = useState<any>(null)
  const [isLoading, setIsLoading] = useState(!!channelId)
  const [showNewMessageDialog, setShowNewMessageDialog] = useState(false)
  const [mobileShowChannels, setMobileShowChannels] = useState(false)

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
    <div className="min-h-screen bg-bg-primary flex">
      {/* Channel List Sidebar - Desktop */}
      <div className="w-72 border-r border-border hidden lg:flex flex-col">
        <ChannelList onNewMessageClick={() => setShowNewMessageDialog(true)} />
      </div>

      {/* Main Content */}
      <div className="flex-1 flex flex-col bg-bg-primary">
        {/* Mobile Header */}
        <div className="lg:hidden border-b border-border p-4 flex items-center justify-between bg-bg-secondary">
          <h1 className="text-xl font-bold text-foreground">Messages</h1>
          <Button
            size="sm"
            onClick={() => setMobileShowChannels(!mobileShowChannels)}
            variant="outline"
          >
            {mobileShowChannels ? '✕' : '☰'}
          </Button>
        </div>

        {/* Mobile Channel List - Overlay */}
        {mobileShowChannels && (
          <div className="lg:hidden absolute inset-0 z-40">
            <div className="absolute inset-0 bg-black/50" onClick={() => setMobileShowChannels(false)} />
            <div className="absolute left-0 top-0 bottom-0 w-72 bg-bg-secondary z-50 flex flex-col">
              <ChannelList onNewMessageClick={() => setShowNewMessageDialog(true)} />
            </div>
          </div>
        )}

        {/* Main Chat Area */}
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
                <div className="text-5xl">⚠️</div>
                <div className="text-lg font-semibold text-foreground">Failed to load conversation</div>
                <p className="text-sm text-muted-foreground">Please try selecting another conversation</p>
              </div>
            </div>
          )
        ) : (
          <div className="flex-1 flex flex-col items-center justify-center p-8">
            <div className="text-center space-y-6 max-w-md">
              <div className="text-6xl">💬</div>
              <div>
                <h2 className="text-2xl font-bold text-foreground mb-2">No conversation selected</h2>
                <p className="text-muted-foreground mb-6">
                  Choose from your existing conversations or start a new message
                </p>
              </div>

              {/* Desktop: Show "New Message" button for guidance */}
              <div className="hidden lg:block">
                <Button
                  onClick={() => setShowNewMessageDialog(true)}
                  className="bg-gradient-to-r from-coral-pink to-rose-pink hover:from-coral-pink/90 hover:to-rose-pink/90 text-white font-semibold"
                >
                  + Start New Conversation
                </Button>
              </div>
            </div>
          </div>
        )}
      </div>

      {/* New Message Dialog */}
      <NewMessageDialog isOpen={showNewMessageDialog} onClose={() => setShowNewMessageDialog(false)} />
    </div>
  )
}

/**
 * Messages - Direct messaging page
 *
 * Features:
 * - Channel list sidebar with conversations
 * - New message dialog with user search
 * - Message thread view with inline replies
 * - File uploads via message input
 * - Typing indicators
 * - Reactions on messages
 * - Search within conversation
 *
 * Note: Wrapped by ChatProvider at app level which provides Chat context
 */
export function Messages() {
  // MessagesContent is already wrapped in Chat context from ChatProvider
  return <MessagesContent />
}
