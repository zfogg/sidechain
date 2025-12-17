import { useState } from 'react'
import { useNavigate } from 'react-router-dom'
import { useUserStore } from '@/stores/useUserStore'
import { Button } from '@/components/ui/button'
import { Input } from '@/components/ui/input'
import { Label } from '@/components/ui/label'
import { Tabs, TabsContent, TabsList, TabsTrigger } from '@/components/ui/tabs'
import { FollowRequestsSection } from '@/components/social/FollowRequestsSection'

type SettingTab = 'profile' | 'privacy' | 'audio' | 'notifications' | 'appearance' | 'account'

export function Settings() {
  const navigate = useNavigate()
  const { user, logout } = useUserStore()
  const [activeTab, setActiveTab] = useState<SettingTab>('profile')
  const [isSaving, setIsSaving] = useState(false)

  // Profile state
  const [displayName, setDisplayName] = useState(user?.displayName || '')
  const [bio, setBio] = useState(user?.bio || '')
  const [socialLinks, setSocialLinks] = useState({
    twitter: user?.socialLinks?.twitter || '',
    instagram: user?.socialLinks?.instagram || '',
    website: user?.socialLinks?.website || '',
  })

  // Privacy state
  const [isPublic, setIsPublic] = useState(user?.isPublic !== false)
  const [isPrivate, setIsPrivate] = useState(user?.isPrivate || false)
  const [allowComments, setAllowComments] = useState(user?.allowComments !== false)
  const [allowDmsFromFollowersOnly, setAllowDmsFromFollowersOnly] = useState(
    user?.allowDmsFromFollowersOnly || false
  )

  // Audio preferences
  const [autoPlayOnScroll, setAutoPlayOnScroll] = useState(true)
  const [audioQuality, setAudioQuality] = useState<'low' | 'medium' | 'high'>('high')

  // Notification preferences
  const [notifyLikes, setNotifyLikes] = useState(true)
  const [notifyComments, setNotifyComments] = useState(true)
  const [notifyFollows, setNotifyFollows] = useState(true)
  const [notifyReposts, setNotifyReposts] = useState(true)
  const [notifyMentions, setNotifyMentions] = useState(true)
  const [emailDigestFrequency, setEmailDigestFrequency] = useState<'never' | 'daily' | 'weekly'>('weekly')

  // Appearance
  const [theme, setTheme] = useState<'dark' | 'light'>('dark')
  const [fontSize, setFontSize] = useState<'small' | 'normal' | 'large'>('normal')

  const handleSaveProfile = async () => {
    setIsSaving(true)
    try {
      // TODO: Call API to update profile
      // await updateProfile({ displayName, bio, socialLinks })
      console.log('Saving profile:', { displayName, bio, socialLinks })
    } finally {
      setIsSaving(false)
    }
  }

  const handleSavePrivacy = async () => {
    setIsSaving(true)
    try {
      // TODO: Call API to update privacy settings
      console.log('Saving privacy:', { isPublic, isPrivate, allowComments, allowDmsFromFollowersOnly })
    } finally {
      setIsSaving(false)
    }
  }

  const handleSaveAudio = async () => {
    setIsSaving(true)
    try {
      // TODO: Call API to save audio preferences
      console.log('Saving audio:', { autoPlayOnScroll, audioQuality })
    } finally {
      setIsSaving(false)
    }
  }

  const handleSaveNotifications = async () => {
    setIsSaving(true)
    try {
      // TODO: Call API to update notification preferences
      console.log('Saving notifications:', {
        notifyLikes,
        notifyComments,
        notifyFollows,
        notifyReposts,
        notifyMentions,
        emailDigestFrequency,
      })
    } finally {
      setIsSaving(false)
    }
  }

  const handleLogout = () => {
    logout()
    navigate('/login')
  }

  if (!user) {
    return <div>Loading...</div>
  }

  return (
    <div className="min-h-screen bg-gradient-to-br from-bg-primary via-bg-secondary to-bg-tertiary p-8">
      <div className="max-w-4xl mx-auto">
        <div className="mb-8">
          <h1 className="text-4xl font-bold text-foreground mb-2">Settings</h1>
          <p className="text-muted-foreground">Manage your account and preferences</p>
        </div>

        <Tabs value={activeTab} onValueChange={(v) => setActiveTab(v as SettingTab)} className="w-full">
          <TabsList className="grid w-full grid-cols-6 mb-8">
            <TabsTrigger value="profile">Profile</TabsTrigger>
            <TabsTrigger value="privacy">Privacy</TabsTrigger>
            <TabsTrigger value="audio">Audio</TabsTrigger>
            <TabsTrigger value="notifications">Notifications</TabsTrigger>
            <TabsTrigger value="appearance">Appearance</TabsTrigger>
            <TabsTrigger value="account">Account</TabsTrigger>
          </TabsList>

          {/* Profile Settings */}
          <TabsContent value="profile">
            <div className="bg-card border border-border/50 rounded-2xl p-8 space-y-8">
              <div>
                <h2 className="text-2xl font-bold text-foreground mb-6">Profile Settings</h2>

                <div className="space-y-6">
                  {/* Display Name */}
                  <div className="space-y-3">
                    <Label htmlFor="displayName" className="text-sm font-semibold text-foreground">
                      Display Name
                    </Label>
                    <Input
                      id="displayName"
                      value={displayName}
                      onChange={(e) => setDisplayName(e.target.value)}
                      placeholder="Your display name"
                      className="h-11 px-4 bg-bg-secondary/80 border-border/50 text-foreground text-sm focus:border-coral-pink focus:ring-1 focus:ring-coral-pink/50"
                    />
                  </div>

                  {/* Bio */}
                  <div className="space-y-3">
                    <Label htmlFor="bio" className="text-sm font-semibold text-foreground">
                      Bio
                    </Label>
                    <textarea
                      id="bio"
                      value={bio}
                      onChange={(e) => setBio(e.target.value)}
                      placeholder="Tell us about yourself"
                      rows={4}
                      className="w-full px-4 py-2 rounded-lg bg-bg-secondary/80 border border-border/50 text-foreground text-sm placeholder:text-muted-foreground/50 focus:border-coral-pink focus:ring-1 focus:ring-coral-pink/50"
                    />
                  </div>

                  {/* Social Links */}
                  <div className="space-y-4 border-t border-border/30 pt-6">
                    <h3 className="text-lg font-semibold text-foreground">Social Links</h3>

                    <div className="space-y-3">
                      <Label htmlFor="twitter" className="text-sm font-semibold text-foreground">
                        Twitter
                      </Label>
                      <Input
                        id="twitter"
                        value={socialLinks.twitter}
                        onChange={(e) => setSocialLinks({ ...socialLinks, twitter: e.target.value })}
                        placeholder="https://twitter.com/username"
                        className="h-11 px-4 bg-bg-secondary/80 border-border/50 text-foreground text-sm focus:border-coral-pink focus:ring-1 focus:ring-coral-pink/50"
                      />
                    </div>

                    <div className="space-y-3">
                      <Label htmlFor="instagram" className="text-sm font-semibold text-foreground">
                        Instagram
                      </Label>
                      <Input
                        id="instagram"
                        value={socialLinks.instagram}
                        onChange={(e) => setSocialLinks({ ...socialLinks, instagram: e.target.value })}
                        placeholder="https://instagram.com/username"
                        className="h-11 px-4 bg-bg-secondary/80 border-border/50 text-foreground text-sm focus:border-coral-pink focus:ring-1 focus:ring-coral-pink/50"
                      />
                    </div>

                    <div className="space-y-3">
                      <Label htmlFor="website" className="text-sm font-semibold text-foreground">
                        Website
                      </Label>
                      <Input
                        id="website"
                        value={socialLinks.website}
                        onChange={(e) => setSocialLinks({ ...socialLinks, website: e.target.value })}
                        placeholder="https://yourwebsite.com"
                        className="h-11 px-4 bg-bg-secondary/80 border-border/50 text-foreground text-sm focus:border-coral-pink focus:ring-1 focus:ring-coral-pink/50"
                      />
                    </div>
                  </div>

                  <Button
                    onClick={handleSaveProfile}
                    disabled={isSaving}
                    className="w-full h-11 bg-gradient-to-r from-coral-pink to-rose-pink hover:from-coral-pink/90 hover:to-rose-pink/90 text-white text-sm font-semibold transition-all duration-200"
                  >
                    {isSaving ? 'Saving...' : 'Save Profile'}
                  </Button>
                </div>
              </div>
            </div>
          </TabsContent>

          {/* Privacy Settings */}
          <TabsContent value="privacy">
            <div className="bg-card border border-border/50 rounded-2xl p-8 space-y-8">
              <h2 className="text-2xl font-bold text-foreground">Privacy & Control</h2>

              {/* Follow Requests Section */}
              <div className="space-y-4 border-b border-border/30 pb-6">
                <h3 className="font-semibold text-foreground">Follow Requests</h3>
                <p className="text-sm text-muted-foreground">
                  Manage pending follow requests if your account is private
                </p>
                <FollowRequestsSection />
              </div>

              <div className="space-y-6">
                {/* Public Profile Toggle */}
                <div className="flex items-center justify-between p-4 rounded-lg bg-bg-secondary/50 border border-border/30">
                  <div>
                    <p className="font-semibold text-foreground">Public Profile</p>
                    <p className="text-sm text-muted-foreground">Let others find and view your profile</p>
                  </div>
                  <input
                    type="checkbox"
                    checked={isPublic}
                    onChange={(e) => setIsPublic(e.target.checked)}
                    className="w-6 h-6 rounded cursor-pointer"
                  />
                </div>

                {/* Allow Comments Toggle */}
                <div className="flex items-center justify-between p-4 rounded-lg bg-bg-secondary/50 border border-border/30">
                  <div>
                    <p className="font-semibold text-foreground">Allow Comments</p>
                    <p className="text-sm text-muted-foreground">Let others comment on your posts</p>
                  </div>
                  <input
                    type="checkbox"
                    checked={allowComments}
                    onChange={(e) => setAllowComments(e.target.checked)}
                    className="w-6 h-6 rounded cursor-pointer"
                  />
                </div>

                {/* DMs from Followers Only Toggle */}
                <div className="flex items-center justify-between p-4 rounded-lg bg-bg-secondary/50 border border-border/30">
                  <div>
                    <p className="font-semibold text-foreground">DMs from Followers Only</p>
                    <p className="text-sm text-muted-foreground">Only followers can send you direct messages</p>
                  </div>
                  <input
                    type="checkbox"
                    checked={allowDmsFromFollowersOnly}
                    onChange={(e) => setAllowDmsFromFollowersOnly(e.target.checked)}
                    className="w-6 h-6 rounded cursor-pointer"
                  />
                </div>

                {/* Private Account Toggle */}
                <div className="flex items-center justify-between p-4 rounded-lg bg-bg-secondary/50 border border-border/30">
                  <div>
                    <p className="font-semibold text-foreground">Private Account</p>
                    <p className="text-sm text-muted-foreground">Require approval for new followers and hide posts from non-followers</p>
                  </div>
                  <input
                    type="checkbox"
                    checked={isPrivate}
                    onChange={(e) => setIsPrivate(e.target.checked)}
                    className="w-6 h-6 rounded cursor-pointer"
                  />
                </div>

                <Button
                  onClick={handleSavePrivacy}
                  disabled={isSaving}
                  className="w-full h-11 bg-gradient-to-r from-coral-pink to-rose-pink hover:from-coral-pink/90 hover:to-rose-pink/90 text-white text-sm font-semibold transition-all duration-200"
                >
                  {isSaving ? 'Saving...' : 'Save Privacy Settings'}
                </Button>
              </div>
            </div>
          </TabsContent>

          {/* Audio Settings */}
          <TabsContent value="audio">
            <div className="bg-card border border-border/50 rounded-2xl p-8 space-y-8">
              <h2 className="text-2xl font-bold text-foreground">Audio Preferences</h2>

              <div className="space-y-6">
                {/* Auto-play Toggle */}
                <div className="flex items-center justify-between p-4 rounded-lg bg-bg-secondary/50 border border-border/30">
                  <div>
                    <p className="font-semibold text-foreground">Auto-play on Scroll</p>
                    <p className="text-sm text-muted-foreground">Automatically play loops as you scroll the feed</p>
                  </div>
                  <input
                    type="checkbox"
                    checked={autoPlayOnScroll}
                    onChange={(e) => setAutoPlayOnScroll(e.target.checked)}
                    className="w-6 h-6 rounded cursor-pointer"
                  />
                </div>

                {/* Audio Quality */}
                <div className="space-y-3">
                  <Label htmlFor="audioQuality" className="text-sm font-semibold text-foreground">
                    Audio Quality
                  </Label>
                  <select
                    id="audioQuality"
                    value={audioQuality}
                    onChange={(e) => setAudioQuality(e.target.value as 'low' | 'medium' | 'high')}
                    className="w-full h-11 px-4 rounded-lg bg-bg-secondary/80 border border-border/50 text-foreground text-sm focus:border-coral-pink focus:ring-1 focus:ring-coral-pink/50"
                  >
                    <option value="low">Low (reduces data usage)</option>
                    <option value="medium">Medium (balanced)</option>
                    <option value="high">High (best quality)</option>
                  </select>
                </div>

                <Button
                  onClick={handleSaveAudio}
                  disabled={isSaving}
                  className="w-full h-11 bg-gradient-to-r from-coral-pink to-rose-pink hover:from-coral-pink/90 hover:to-rose-pink/90 text-white text-sm font-semibold transition-all duration-200"
                >
                  {isSaving ? 'Saving...' : 'Save Audio Settings'}
                </Button>
              </div>
            </div>
          </TabsContent>

          {/* Notification Settings */}
          <TabsContent value="notifications">
            <div className="bg-card border border-border/50 rounded-2xl p-8 space-y-8">
              <h2 className="text-2xl font-bold text-foreground">Notification Preferences</h2>

              <div className="space-y-4 border-b border-border/30 pb-6">
                <h3 className="font-semibold text-foreground">In-App Notifications</h3>

                <div className="flex items-center justify-between p-4 rounded-lg bg-bg-secondary/50 border border-border/30">
                  <div>
                    <p className="font-semibold text-foreground">Post Likes</p>
                    <p className="text-sm text-muted-foreground">Notify when someone likes your posts</p>
                  </div>
                  <input
                    type="checkbox"
                    checked={notifyLikes}
                    onChange={(e) => setNotifyLikes(e.target.checked)}
                    className="w-6 h-6 rounded cursor-pointer"
                  />
                </div>

                <div className="flex items-center justify-between p-4 rounded-lg bg-bg-secondary/50 border border-border/30">
                  <div>
                    <p className="font-semibold text-foreground">Comments</p>
                    <p className="text-sm text-muted-foreground">Notify when someone comments on your posts</p>
                  </div>
                  <input
                    type="checkbox"
                    checked={notifyComments}
                    onChange={(e) => setNotifyComments(e.target.checked)}
                    className="w-6 h-6 rounded cursor-pointer"
                  />
                </div>

                <div className="flex items-center justify-between p-4 rounded-lg bg-bg-secondary/50 border border-border/30">
                  <div>
                    <p className="font-semibold text-foreground">New Followers</p>
                    <p className="text-sm text-muted-foreground">Notify when someone follows you</p>
                  </div>
                  <input
                    type="checkbox"
                    checked={notifyFollows}
                    onChange={(e) => setNotifyFollows(e.target.checked)}
                    className="w-6 h-6 rounded cursor-pointer"
                  />
                </div>

                <div className="flex items-center justify-between p-4 rounded-lg bg-bg-secondary/50 border border-border/30">
                  <div>
                    <p className="font-semibold text-foreground">Reposts</p>
                    <p className="text-sm text-muted-foreground">Notify when someone reposts your content</p>
                  </div>
                  <input
                    type="checkbox"
                    checked={notifyReposts}
                    onChange={(e) => setNotifyReposts(e.target.checked)}
                    className="w-6 h-6 rounded cursor-pointer"
                  />
                </div>

                <div className="flex items-center justify-between p-4 rounded-lg bg-bg-secondary/50 border border-border/30">
                  <div>
                    <p className="font-semibold text-foreground">@Mentions</p>
                    <p className="text-sm text-muted-foreground">Notify when someone mentions you in comments</p>
                  </div>
                  <input
                    type="checkbox"
                    checked={notifyMentions}
                    onChange={(e) => setNotifyMentions(e.target.checked)}
                    className="w-6 h-6 rounded cursor-pointer"
                  />
                </div>
              </div>

              <div className="space-y-3">
                <h3 className="font-semibold text-foreground">Email Digest</h3>
                <select
                  value={emailDigestFrequency}
                  onChange={(e) => setEmailDigestFrequency(e.target.value as 'never' | 'daily' | 'weekly')}
                  className="w-full h-11 px-4 rounded-lg bg-bg-secondary/80 border border-border/50 text-foreground text-sm focus:border-coral-pink focus:ring-1 focus:ring-coral-pink/50"
                >
                  <option value="never">Never</option>
                  <option value="daily">Daily</option>
                  <option value="weekly">Weekly</option>
                </select>
              </div>

              <Button
                onClick={handleSaveNotifications}
                disabled={isSaving}
                className="w-full h-11 bg-gradient-to-r from-coral-pink to-rose-pink hover:from-coral-pink/90 hover:to-rose-pink/90 text-white text-sm font-semibold transition-all duration-200"
              >
                {isSaving ? 'Saving...' : 'Save Notification Settings'}
              </Button>
            </div>
          </TabsContent>

          {/* Appearance Settings */}
          <TabsContent value="appearance">
            <div className="bg-card border border-border/50 rounded-2xl p-8 space-y-8">
              <h2 className="text-2xl font-bold text-foreground">Appearance</h2>

              <div className="space-y-6">
                {/* Theme */}
                <div className="space-y-3">
                  <Label className="text-sm font-semibold text-foreground">Theme</Label>
                  <div className="flex gap-4">
                    <button
                      onClick={() => setTheme('dark')}
                      className={`flex-1 p-4 rounded-lg border-2 transition-all ${
                        theme === 'dark'
                          ? 'border-coral-pink bg-bg-secondary/50'
                          : 'border-border/30 bg-bg-secondary/20 hover:bg-bg-secondary/30'
                      }`}
                    >
                      <p className="font-semibold text-foreground">Dark</p>
                      <p className="text-xs text-muted-foreground">Easy on the eyes</p>
                    </button>
                    <button
                      onClick={() => setTheme('light')}
                      className={`flex-1 p-4 rounded-lg border-2 transition-all ${
                        theme === 'light'
                          ? 'border-coral-pink bg-bg-secondary/50'
                          : 'border-border/30 bg-bg-secondary/20 hover:bg-bg-secondary/30'
                      }`}
                    >
                      <p className="font-semibold text-foreground">Light</p>
                      <p className="text-xs text-muted-foreground">Bright and clear</p>
                    </button>
                  </div>
                </div>

                {/* Font Size */}
                <div className="space-y-3">
                  <Label htmlFor="fontSize" className="text-sm font-semibold text-foreground">
                    Font Size
                  </Label>
                  <select
                    id="fontSize"
                    value={fontSize}
                    onChange={(e) => setFontSize(e.target.value as 'small' | 'normal' | 'large')}
                    className="w-full h-11 px-4 rounded-lg bg-bg-secondary/80 border border-border/50 text-foreground text-sm focus:border-coral-pink focus:ring-1 focus:ring-coral-pink/50"
                  >
                    <option value="small">Small</option>
                    <option value="normal">Normal</option>
                    <option value="large">Large (better for accessibility)</option>
                  </select>
                </div>

                <Button
                  onClick={() => console.log('Save appearance')}
                  className="w-full h-11 bg-gradient-to-r from-coral-pink to-rose-pink hover:from-coral-pink/90 hover:to-rose-pink/90 text-white text-sm font-semibold transition-all duration-200"
                >
                  Save Appearance Settings
                </Button>
              </div>
            </div>
          </TabsContent>

          {/* Account Settings */}
          <TabsContent value="account">
            <div className="bg-card border border-border/50 rounded-2xl p-8 space-y-8">
              <h2 className="text-2xl font-bold text-foreground">Account</h2>

              <div className="space-y-6">
                {/* Account Info */}
                <div className="p-4 rounded-lg bg-bg-secondary/50 border border-border/30 space-y-3">
                  <div>
                    <p className="text-xs text-muted-foreground">Email</p>
                    <p className="font-semibold text-foreground">{user.email}</p>
                  </div>
                  <div>
                    <p className="text-xs text-muted-foreground">Username</p>
                    <p className="font-semibold text-foreground">@{user.username}</p>
                  </div>
                  <div>
                    <p className="text-xs text-muted-foreground">Joined</p>
                    <p className="font-semibold text-foreground">
                      {new Date(user.createdAt || Date.now()).toLocaleDateString()}
                    </p>
                  </div>
                </div>

                {/* Danger Zone */}
                <div className="space-y-4 border-t border-border/30 pt-6">
                  <h3 className="font-semibold text-red-400">Danger Zone</h3>

                  <Button
                    onClick={handleLogout}
                    className="w-full h-11 bg-red-600/20 hover:bg-red-600/30 border border-red-500/30 text-red-400 text-sm font-semibold transition-all duration-200"
                  >
                    Log Out
                  </Button>

                  <Button
                    onClick={() => console.log('Change password')}
                    className="w-full h-11 bg-red-600/20 hover:bg-red-600/30 border border-red-500/30 text-red-400 text-sm font-semibold transition-all duration-200"
                  >
                    Change Password
                  </Button>

                  <Button
                    onClick={() => console.log('Delete account')}
                    className="w-full h-11 bg-red-600/20 hover:bg-red-600/30 border border-red-500/30 text-red-400 text-sm font-semibold transition-all duration-200"
                  >
                    Delete Account
                  </Button>
                </div>
              </div>
            </div>
          </TabsContent>
        </Tabs>
      </div>
    </div>
  )
}
