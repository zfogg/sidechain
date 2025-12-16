import { ChannelList as StreamChannelList } from 'stream-chat-react'
import { useChatContext } from 'stream-chat-react'
import { Button } from '@/components/ui/button'

interface ChannelListProps {
  onNewMessageClick?: () => void
}

/**
 * ChannelList - Stream Chat channel list component
 * Shows all direct message channels with unread indicators
 */
export function ChannelList({ onNewMessageClick }: ChannelListProps) {
  const { client } = useChatContext()

  return (
    <div className="h-full flex flex-col bg-bg-secondary">
      {/* Header */}
      <div className="p-4 border-b border-border space-y-4">
        <div className="flex items-center justify-between">
          <h2 className="text-lg font-bold text-foreground">Messages</h2>
          <span className="text-sm text-muted-foreground">💬</span>
        </div>
        <Button
          onClick={onNewMessageClick}
          className="w-full bg-gradient-to-r from-coral-pink to-rose-pink hover:from-coral-pink/90 hover:to-rose-pink/90 text-white font-semibold"
        >
          + New Message
        </Button>
      </div>

      {/* Channel List */}
      <div className="flex-1 overflow-y-auto">
        <StreamChannelList
          filters={{ type: 'messaging', members: { $in: client?.userID ? [client.userID] : [] } }}
          sort={{ last_message_at: -1 }}
          options={{ state: true, presence: true }}
        />
      </div>
    </div>
  )
}
