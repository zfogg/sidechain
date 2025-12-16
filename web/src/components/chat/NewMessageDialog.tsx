import { useState, useCallback } from 'react'
import { useNavigate } from 'react-router-dom'
import { useChatContext } from 'stream-chat-react'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { Spinner } from '@/components/ui/spinner'
import { useUserStore } from '@/stores/useUserStore'
import { SearchClient } from '@/api/SearchClient'

interface UserSearchResult {
  id: string
  username: string
  displayName: string
  profilePictureUrl?: string
}

/**
 * NewMessageDialog - Create a new direct message conversation
 * Allows searching for users and starting a conversation with them
 */
export function NewMessageDialog({ isOpen, onClose }: { isOpen: boolean; onClose: () => void }) {
  const navigate = useNavigate()
  const { client } = useChatContext()
  const { user: currentUser } = useUserStore()

  const [searchQuery, setSearchQuery] = useState('')
  const [searchResults, setSearchResults] = useState<UserSearchResult[]>([])
  const [isSearching, setIsSearching] = useState(false)
  const [selectedUser, setSelectedUser] = useState<UserSearchResult | null>(null)
  const [isCreatingChannel, setIsCreatingChannel] = useState(false)

  // Search for users by name or username
  const handleSearch = useCallback(async (query: string) => {
    if (!query.trim()) {
      setSearchResults([])
      return
    }

    setIsSearching(true)
    try {
      const result = await SearchClient.searchUsers(query)

      if (result.isOk()) {
        const users = result.getValue()
        // SearchClient already returns camelCase fields, just use them directly
        setSearchResults(users as UserSearchResult[])
      } else {
        console.error('Search failed:', result.getError())
        setSearchResults([])
      }
    } catch (error) {
      console.error('Failed to search users:', error)
      setSearchResults([])
    } finally {
      setIsSearching(false)
    }
  }, [])

  const handleCreateChannel = useCallback(
    async (targetUser: UserSearchResult) => {
      if (!client || !currentUser) return

      setIsCreatingChannel(true)
      try {
        // Step 1: Sync the target user to Stream Chat with backend admin credentials
        // This ensures they exist in Stream Chat before channel creation
        const syncResponse = await fetch(`${import.meta.env.VITE_API_URL}/auth/sync-user/${targetUser.id}`, {
          method: 'POST',
          headers: {
            'Authorization': `Bearer ${localStorage.getItem('auth_token')}`,
            'Content-Type': 'application/json',
          },
        })

        if (!syncResponse.ok) {
          const error = await syncResponse.json()
          console.error('Failed to sync user to Stream Chat:', error)
          // Continue anyway - user might already be synced
        }

        // Step 2: Create a deterministic channel ID using hash of sorted user IDs
        // Channel ID must be ≤64 characters for Stream Chat
        const userIds = [currentUser.id, targetUser.id].sort()
        const combinedIds = userIds.join('|')

        // Simple hash function to create a short, deterministic ID
        let hash = 0
        for (let i = 0; i < combinedIds.length; i++) {
          const char = combinedIds.charCodeAt(i)
          hash = ((hash << 5) - hash) + char
          hash = hash & hash // Convert to 32bit integer
        }
        const hashStr = Math.abs(hash).toString(36)
        const channelId = `dm-${hashStr}`

        // Step 3: Create the channel with both users as members
        // Following the C++ plugin pattern: just create the channel directly
        const channel = client.channel('messaging', channelId, {
          members: [currentUser.id, targetUser.id],
        })

        await channel.create()

        // Navigate to the new channel
        navigate(`/messages/${channelId}`)
        onClose()
      } catch (error) {
        console.error('Failed to create channel:', error)
      } finally {
        setIsCreatingChannel(false)
      }
    },
    [client, currentUser, navigate, onClose]
  )

  if (!isOpen) return null

  return (
    <div className="fixed inset-0 z-50 bg-black/80 flex items-center justify-center">
      <div className="rounded-lg shadow-xl max-w-md w-full mx-4 max-h-96 flex flex-col" style={{ backgroundColor: '#26262c' }}>
        {/* Header */}
        <div className="p-6 border-b border-border flex items-center justify-between">
          <h2 className="text-xl font-bold text-foreground">New Message</h2>
          <button
            onClick={onClose}
            className="text-muted-foreground hover:text-foreground transition-colors"
          >
            ✕
          </button>
        </div>

        {/* Search Input */}
        <div className="p-4 border-b border-border">
          <div className="relative">
            <Input
              type="text"
              placeholder="Search users by name or username..."
              value={searchQuery}
              onChange={(e) => {
                setSearchQuery(e.target.value)
                handleSearch(e.target.value)
              }}
              className="border-border text-foreground placeholder:text-muted-foreground"
              style={{ backgroundColor: '#2e2e34' }}
              autoFocus
            />
            {isSearching && (
              <div className="absolute right-3 top-1/2 -translate-y-1/2">
                <div className="animate-spin">⟳</div>
              </div>
            )}
          </div>
        </div>

        {/* Search Results */}
        <div className="flex-1 overflow-y-auto">
          {searchResults.length === 0 ? (
            <div className="p-6 text-center text-muted-foreground">
              {searchQuery ? 'No users found' : 'Start typing to search for users'}
            </div>
          ) : (
            <div className="divide-y divide-border">
              {searchResults.map((user) => (
                <button
                  key={user.id}
                  onClick={() => setSelectedUser(user)}
                  className={`w-full p-4 text-left transition-colors ${
                    selectedUser?.id === user.id
                      ? 'border-l-2 border-coral-pink'
                      : ''
                  }`}
                  style={{
                    backgroundColor:
                      selectedUser?.id === user.id
                        ? 'rgba(253, 119, 146, 0.1)'
                        : '#2e2e34',
                  }}
                >
                  <div className="flex items-center gap-3">
                    <div className="relative flex-shrink-0">
                      <img
                        src={user.profilePictureUrl || `https://api.dicebear.com/7.x/avataaars/svg?seed=${user.id}`}
                        alt={user.displayName}
                        className="w-12 h-12 rounded-full object-cover"
                      />
                      {/* Online indicator could go here */}
                    </div>
                    <div className="min-w-0 flex-1">
                      <div className="font-medium text-foreground truncate">{user.displayName}</div>
                      <div className="text-sm text-muted-foreground truncate">@{user.username}</div>
                    </div>
                  </div>
                </button>
              ))}
            </div>
          )}
        </div>

        {/* Footer */}
        <div className="p-4 border-t border-border flex gap-3">
          <Button variant="outline" onClick={onClose} className="flex-1">
            Cancel
          </Button>
          <Button
            onClick={() => selectedUser && handleCreateChannel(selectedUser)}
            disabled={!selectedUser || isCreatingChannel}
            className="flex-1 bg-gradient-to-r from-coral-pink to-rose-pink hover:from-coral-pink/90 hover:to-rose-pink/90"
          >
            {isCreatingChannel ? 'Creating...' : 'Message'}
          </Button>
        </div>
      </div>
    </div>
  )
}
