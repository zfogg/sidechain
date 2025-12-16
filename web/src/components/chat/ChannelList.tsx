import { useCallback } from 'react'
import { ChannelList as StreamChannelList, useChatContext } from 'stream-chat-react'
import { useNavigate } from 'react-router-dom'
import { Button } from '@/components/ui/button'
import { Spinner } from '@/components/ui/spinner'

interface ChannelListProps {
  onNewMessageClick?: () => void
}

/**
 * ChannelList - Stream Chat channel list component
 * Shows all direct message channels with unread indicators
 */
export function ChannelList({ onNewMessageClick }: ChannelListProps) {
  const navigate = useNavigate()
  const { client } = useChatContext()

  const handleSelectChannel = useCallback(
    (channelId: string) => {
      navigate(`/messages/${channelId}`)
    },
    [navigate]
  )

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
        {!client ? (
          <div className="flex items-center justify-center h-full">
            <Spinner size="md" />
          </div>
        ) : (
          <StreamChannelList
            filters={{ type: 'messaging' }}
            sort={{ last_message_at: -1 }}
            options={{ state: true, presence: true }}
            onSelectChannel={(channel) => handleSelectChannel(channel.id)}
          />
        )}
      </div>
    </div>
  )
}
