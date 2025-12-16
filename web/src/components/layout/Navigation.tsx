import { useNavigate, useLocation } from 'react-router-dom'
import { useUserStore } from '@/stores/useUserStore'
import { NotificationBell } from '@/components/notifications/NotificationBell'
import { Button } from '@/components/ui/button'

/**
 * Navigation - Top navigation bar with app logo, nav links, and user menu
 */
export function Navigation() {
  const navigate = useNavigate()
  const location = useLocation()
  const { user, logout } = useUserStore()

  const isActive = (path: string) => location.pathname === path

  const handleLogout = () => {
    logout()
    navigate('/login')
  }

  if (!user) return null

  return (
    <nav className="sticky top-0 z-50 bg-card/95 backdrop-blur border-b border-border">
      <div className="max-w-7xl mx-auto px-4">
        <div className="flex items-center justify-between h-16">
          {/* Logo & App Name */}
          <div
            className="flex items-center gap-3 cursor-pointer hover:opacity-80 transition-opacity"
            onClick={() => navigate('/feed')}
          >
            <div className="w-8 h-8 bg-gradient-to-br from-coral-pink to-rose-pink rounded-lg flex items-center justify-center">
              <span className="text-white font-bold text-lg">◈</span>
            </div>
            <span className="text-lg font-bold text-foreground hidden sm:inline">
              Sidechain
            </span>
          </div>

          {/* Center Nav Links */}
          <div className="hidden sm:flex items-center gap-1">
            <Button
              variant={isActive('/feed') ? 'default' : 'ghost'}
              onClick={() => navigate('/feed')}
              className="gap-2"
            >
              <span>📰</span>
              Feed
            </Button>
            <Button
              variant={isActive('/upload') ? 'default' : 'ghost'}
              onClick={() => navigate('/upload')}
              className="gap-2"
            >
              <span>⬆️</span>
              Upload
            </Button>
            <Button
              variant={isActive('/messages') ? 'default' : 'ghost'}
              onClick={() => navigate('/messages')}
              className="gap-2"
            >
              <span>💬</span>
              Messages
            </Button>
            <Button
              variant={isActive('/notifications') ? 'default' : 'ghost'}
              onClick={() => navigate('/notifications')}
              className="gap-2"
            >
              <span>🔔</span>
              Notifications
            </Button>
          </div>

          {/* Right Side - User Menu */}
          <div className="flex items-center gap-4">
            {/* Notification Bell (Mobile & Desktop) */}
            <div className="sm:hidden">
              <NotificationBell />
            </div>

            {/* User Profile & Settings */}
            <div className="flex items-center gap-2">
              <button
                onClick={() => navigate(`/profile/${user.username}`)}
                className="flex items-center gap-2 px-3 py-2 rounded-lg hover:bg-bg-secondary transition-colors"
                title="Your Profile"
              >
                <img
                  src={user.profilePictureUrl || `https://api.dicebear.com/7.x/avataaars/svg?seed=${user.id}`}
                  alt={user.displayName}
                  className="w-6 h-6 rounded-full"
                />
                <span className="text-sm font-medium text-foreground hidden sm:inline">
                  {user.displayName}
                </span>
              </button>

              {/* Settings Button */}
              <Button
                variant="ghost"
                onClick={() => navigate('/settings')}
                className="gap-2"
                title="Settings"
              >
                <span>⚙️</span>
                <span className="hidden sm:inline">Settings</span>
              </Button>

              {/* Logout Button */}
              <Button
                variant="ghost"
                onClick={handleLogout}
                className="gap-2 text-red-400 hover:text-red-300 hover:bg-red-400/10"
              >
                <span>🚪</span>
                <span className="hidden sm:inline">Logout</span>
              </Button>
            </div>
          </div>
        </div>

        {/* Mobile Navigation - Secondary row */}
        <div className="sm:hidden flex gap-1 pb-2 overflow-x-auto">
          <Button
            variant={isActive('/feed') ? 'default' : 'outline'}
            onClick={() => navigate('/feed')}
            size="sm"
            className="gap-1 whitespace-nowrap"
          >
            <span>📰</span>
            Feed
          </Button>
          <Button
            variant={isActive('/upload') ? 'default' : 'outline'}
            onClick={() => navigate('/upload')}
            size="sm"
            className="gap-1 whitespace-nowrap"
          >
            <span>⬆️</span>
            Upload
          </Button>
          <Button
            variant={isActive('/messages') ? 'default' : 'outline'}
            onClick={() => navigate('/messages')}
            size="sm"
            className="gap-1 whitespace-nowrap"
          >
            <span>💬</span>
            Messages
          </Button>
          <Button
            variant={isActive('/notifications') ? 'default' : 'outline'}
            onClick={() => navigate('/notifications')}
            size="sm"
            className="gap-1 whitespace-nowrap"
          >
            <span>🔔</span>
            Notifications
          </Button>
        </div>
      </div>
    </nav>
  )
}
