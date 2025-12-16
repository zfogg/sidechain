import { useParams, Navigate } from 'react-router-dom'
import { useQuery } from '@tanstack/react-query'
import { useUserStore } from '@/stores/useUserStore'
import { UserClient } from '@/api/UserClient'
import { FeedList } from '@/components/feed/FeedList'
import { Button } from '@/components/ui/button'
import { Spinner } from '@/components/ui/spinner'
import { useFollowMutation } from '@/hooks/mutations/useFollowMutation'

interface ProfileUser {
  id: string
  username: string
  displayName: string
  bio?: string
  profilePictureUrl?: string
  followerCount: number
  followingCount: number
  postCount: number
  isFollowing: boolean
  isOwnProfile: boolean
  createdAt: string
  socialLinks?: {
    twitter?: string
    instagram?: string
    website?: string
  }
}

/**
 * Profile - User profile page with posts and follower info
 *
 * Features:
 * - Display user profile information
 * - Show user's posts feed
 * - Follow/unfollow button
 * - Follower/following counts
 * - User metadata (join date, bio, links)
 * - Edit profile button for own profile
 */
export function Profile() {
  const { username } = useParams<{ username: string }>()
  const { user: currentUser } = useUserStore()
  const { mutate: toggleFollow, isPending: isFollowPending } = useFollowMutation()

  if (!username) {
    return <Navigate to="/feed" replace />
  }

  // Fetch user profile
  const {
    data: profile,
    isLoading,
    error,
  } = useQuery<ProfileUser>({
    queryKey: ['profile', username],
    queryFn: async () => {
      const result = await UserClient.getProfile(username)
      if (result.isError()) {
        throw new Error(result.getError())
      }
      return result.getValue()
    },
  })

  if (isLoading) {
    return (
      <div className="min-h-screen bg-gradient-to-br from-bg-primary via-bg-secondary to-bg-tertiary flex items-center justify-center">
        <Spinner size="lg" />
      </div>
    )
  }

  if (error || !profile) {
    return (
      <div className="min-h-screen bg-gradient-to-br from-bg-primary via-bg-secondary to-bg-tertiary flex items-center justify-center">
        <div className="text-center">
          <div className="text-xl font-bold text-foreground mb-2">User not found</div>
          <div className="text-muted-foreground">@{username} doesn't exist</div>
        </div>
      </div>
    )
  }

  const handleFollow = () => {
    toggleFollow({ userId: profile.id, shouldFollow: !profile.isFollowing })
  }

  return (
    <div className="min-h-screen bg-gradient-to-br from-bg-primary via-bg-secondary to-bg-tertiary">
      {/* Profile Header */}
      <div className="bg-card/50 border-b border-border">
        <div className="max-w-4xl mx-auto px-4 py-12">
          <div className="flex gap-6">
            {/* Avatar */}
            <div className="flex-shrink-0">
              <img
                src={
                  profile.profilePictureUrl ||
                  `https://api.dicebear.com/7.x/avataaars/svg?seed=${profile.id}`
                }
                alt={profile.displayName}
                className="w-24 h-24 rounded-full border-4 border-border"
              />
            </div>

            {/* Profile Info */}
            <div className="flex-1">
              <div className="flex items-start justify-between mb-2">
                <div>
                  <h1 className="text-3xl font-bold text-foreground">{profile.displayName}</h1>
                  <p className="text-muted-foreground text-lg">@{profile.username}</p>
                </div>

                {/* Follow Button */}
                {!profile.isOwnProfile && (
                  <Button
                    onClick={handleFollow}
                    disabled={isFollowPending}
                    className={`h-10 px-6 font-semibold transition-all ${
                      profile.isFollowing
                        ? 'bg-bg-secondary/80 border border-border/50 text-foreground hover:bg-red-500/20 hover:border-red-500/30 hover:text-red-400'
                        : 'bg-gradient-to-r from-coral-pink to-rose-pink hover:from-coral-pink/90 hover:to-rose-pink/90 text-white'
                    }`}
                  >
                    {profile.isFollowing ? 'Following' : 'Follow'}
                  </Button>
                )}

                {/* Edit Profile Button */}
                {profile.isOwnProfile && (
                  <Button className="h-10 px-6 bg-border/50 hover:bg-border/70 text-foreground font-semibold">
                    Edit Profile
                  </Button>
                )}
              </div>

              {/* Bio */}
              {profile.bio && <p className="text-foreground mb-4">{profile.bio}</p>}

              {/* Social Links */}
              {profile.socialLinks && (
                <div className="flex gap-3 mb-4">
                  {profile.socialLinks.twitter && (
                    <a
                      href={profile.socialLinks.twitter}
                      target="_blank"
                      rel="noopener noreferrer"
                      className="text-coral-pink hover:text-rose-pink transition-colors text-sm"
                    >
                      🐦 Twitter
                    </a>
                  )}
                  {profile.socialLinks.instagram && (
                    <a
                      href={profile.socialLinks.instagram}
                      target="_blank"
                      rel="noopener noreferrer"
                      className="text-coral-pink hover:text-rose-pink transition-colors text-sm"
                    >
                      📷 Instagram
                    </a>
                  )}
                  {profile.socialLinks.website && (
                    <a
                      href={profile.socialLinks.website}
                      target="_blank"
                      rel="noopener noreferrer"
                      className="text-coral-pink hover:text-rose-pink transition-colors text-sm"
                    >
                      🌐 Website
                    </a>
                  )}
                </div>
              )}

              {/* Stats */}
              <div className="flex gap-6 text-sm">
                <div>
                  <span className="font-semibold text-foreground">{profile.postCount}</span>
                  <span className="text-muted-foreground ml-1">Posts</span>
                </div>
                <div
                  className="cursor-pointer hover:text-foreground transition-colors"
                  onClick={() => {
                    // TODO: Open followers modal
                  }}
                >
                  <span className="font-semibold text-foreground">{profile.followerCount}</span>
                  <span className="text-muted-foreground ml-1">Followers</span>
                </div>
                <div
                  className="cursor-pointer hover:text-foreground transition-colors"
                  onClick={() => {
                    // TODO: Open following modal
                  }}
                >
                  <span className="font-semibold text-foreground">{profile.followingCount}</span>
                  <span className="text-muted-foreground ml-1">Following</span>
                </div>
                <div>
                  <span className="text-muted-foreground">
                    Joined {new Date(profile.createdAt).toLocaleDateString()}
                  </span>
                </div>
              </div>
            </div>
          </div>
        </div>
      </div>

      {/* User Posts Feed */}
      <main className="max-w-4xl mx-auto px-4 py-8">
        <div>
          <h2 className="text-xl font-bold text-foreground mb-6">
            {profile.isOwnProfile ? 'Your Posts' : `${profile.displayName}'s Posts`}
          </h2>

          {/* Custom feed for user's posts - would use userProfileFeed type */}
          <div className="bg-card border border-border rounded-lg p-8 text-center text-muted-foreground">
            User's posts would display here (connected to feed backend)
          </div>
        </div>
      </main>
    </div>
  )
}
