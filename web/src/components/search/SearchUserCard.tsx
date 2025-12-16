import { useNavigate } from 'react-router-dom'
import { SearchUser } from '@/api/SearchClient'
import { Button } from '@/components/ui/button'
import { useFollowMutation } from '@/hooks/mutations/useFollowMutation'
import { DirectMessageButton } from '@/components/chat/DirectMessageButton'
import { ReportButton } from '@/components/report/ReportButton'

interface SearchUserCardProps {
  user: SearchUser
}

/**
 * SearchUserCard - User result card in search
 */
export function SearchUserCard({ user }: SearchUserCardProps) {
  const navigate = useNavigate()
  const { mutate: toggleFollow, isPending: isFollowPending } = useFollowMutation()

  return (
    <div className="bg-card border border-border rounded-lg p-4 hover:border-coral-pink/50 transition-colors">
      {/* Avatar & Name */}
      <div className="flex items-start gap-3 mb-3">
        <img
          src={user.profilePictureUrl || `https://api.dicebear.com/7.x/avataaars/svg?seed=${user.id}`}
          alt={user.displayName}
          className="w-12 h-12 rounded-full cursor-pointer hover:opacity-80 transition-opacity"
          onClick={() => navigate(`/profile/${user.username}`)}
        />
        <div className="flex-1 min-w-0">
          <h4 className="font-semibold text-foreground truncate hover:text-coral-pink cursor-pointer transition-colors"
            onClick={() => navigate(`/profile/${user.username}`)}
          >
            {user.displayName}
          </h4>
          <p className="text-sm text-muted-foreground truncate">@{user.username}</p>
        </div>
      </div>

      {/* Bio */}
      {user.bio && (
        <p className="text-sm text-muted-foreground mb-3 line-clamp-2">{user.bio}</p>
      )}

      {/* Followers */}
      <div className="text-xs text-muted-foreground mb-3">
        👥 {user.followerCount.toLocaleString()} follower{user.followerCount !== 1 ? 's' : ''}
      </div>

      {/* Actions */}
      <div className="flex gap-2">
        <Button
          onClick={() => navigate(`/profile/${user.username}`)}
          variant="outline"
          size="sm"
          className="flex-1"
        >
          View Profile
        </Button>
        <DirectMessageButton userId={user.id} displayName={user.displayName} />
        <ReportButton
          type="user"
          targetId={user.id}
          targetName={user.displayName}
          variant="ghost"
          size="sm"
        />
      </div>
    </div>
  )
}
