import { useCallback } from 'react'
import { ChannelList as StreamChannelList } from 'stream-chat-react'
import { useNavigate } from 'react-router-dom'

/**
 * ChannelList - Stream Chat channel list component
 * Shows all direct message channels with unread indicators
 */
export function ChannelList() {
  const navigate = useNavigate()

  const handleSelectChannel = useCallback(
    (channelId: string) => {
      navigate(`/messages/${channelId}`)
    },
    [navigate]
  )

  return (
    <div className="h-full border-r border-border overflow-hidden">
      <div className="p-4 border-b border-border">
        <h2 className="text-lg font-bold text-foreground">Messages</h2>
      </div>
      <StreamChannelList
        filters={{ type: 'messaging' }}
        sort={{ last_message_at: -1 }}
        options={{ state: true, presence: true }}
        onSelectChannel={(channel) => handleSelectChannel(channel.id)}
      />
    </div>
  )
}
