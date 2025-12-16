import { ChannelList } from '@/components/chat/ChannelList'
import { MessageThread } from '@/components/chat/MessageThread'
import { NewMessageDialog } from '@/components/chat/NewMessageDialog'
import { Button } from '@/components/ui/button'
import { Spinner } from '@/components/ui/spinner'
import { useChatStore } from '@/stores/useChatStore'
import { useEffect, useState } from 'react'
import { useParams } from 'react-router-dom'
import { Channel, useChatContext } from 'stream-chat-react'

/**
 * MessagesContent - Inner component that uses Chat context
 * Must be wrapped in Chat component to use useChatContext
 */
function MessagesContent() {
  const { channelId } = useParams<{ channelId: string }>()
  const { client } = useChatContext()
  const { isStreamConnected } = useChatStore()
  const [channel, setChannel] = useState<any>(null)
  const [isLoading, setIsLoading] = useState(!!channelId)
  const [showNewMessageDialog, setShowNewMessageDialog] = useState(false)

  useEffect(() => {
    if (!channelId || !client || !isStreamConnected) {
      setIsLoading(false)
      return
    }

    const loadChannel = async () => {
      try {
        const ch = client.channel('messaging', channelId)
        await ch.watch()
        setChannel(ch)
      } catch (error) {
        console.error('Failed to load channel:', error)
        setChannel(null)
      } finally {
        setIsLoading(false)
      }
    }

    loadChannel()
  }, [channelId, client, isStreamConnected])

  return (
    <div className="h-full bg-bg-primary flex overflow-hidden">
      {/* Channel List Sidebar - Desktop */}
      <div className="w-80 border-r border-border hidden lg:flex lg:flex-col flex-shrink-0 bg-bg-secondary overflow-y-auto">
        <ChannelList onNewMessageClick={() => setShowNewMessageDialog(true)} />
      </div>

      {/* Main Content */}
      <div className="flex-1 flex flex-col bg-bg-primary min-w-0 overflow-hidden">
        {/* Mobile: Show conversation list when no channel selected */}
        {!channelId && (
          <div className="lg:hidden flex-1 overflow-y-auto">
            <ChannelList onNewMessageClick={() => setShowNewMessageDialog(true)} />
          </div>
        )}

        {/* Desktop & Mobile: Show selected conversation */}
        {channelId && (
          <>
            {isLoading ? (
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
            )}
          </>
        )}

        {/* Desktop: Show empty state when no channel selected */}
        {!channelId && (
          <div className="hidden lg:flex flex-1 flex-col items-center justify-center p-8">
            <div className="text-center space-y-6 max-w-md">
              <div className="text-6xl">💬</div>
              <div>
                <h2 className="text-2xl font-bold text-foreground mb-2">No conversation selected</h2>
                <p className="text-muted-foreground mb-6">
                  Choose from your existing conversations or start a new message
                </p>
              </div>

              {/* New Message button */}
              <Button
                onClick={() => setShowNewMessageDialog(true)}
                className="bg-gradient-to-r from-coral-pink to-rose-pink hover:from-coral-pink/90 hover:to-rose-pink/90 text-white font-semibold"
              >
                + Start New Conversation
              </Button>
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
 * - Channel list sidebar with conversations (desktop)
 * - Channel list as full-width view on mobile when no conversation selected
 * - New message dialog with user search
 * - Message thread view with inline replies
 * - File uploads via message input
 * - Typing indicators
 * - Reactions on messages
 * - Search within conversation
 *
 * Layout:
 * - Desktop: Sidebar (conversations) + Main area (chat or empty state)
 * - Mobile: Conversations list OR Chat view (toggled by selecting a conversation)
 *
 * Note: Wrapped by ChatProvider at app level which provides Chat context
 */
export function Messages() {
  // MessagesContent is already wrapped in Chat context from ChatProvider
  return <MessagesContent />
}
