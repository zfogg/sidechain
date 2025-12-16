import { BrowserRouter, Routes, Route, Navigate } from 'react-router-dom'
import { QueryProvider } from '@/providers/QueryProvider'
import { AuthProvider } from '@/providers/AuthProvider'
import { ChatProvider } from '@/providers/ChatProvider'
import { useUserStore } from '@/stores/useUserStore'
import { Navigation } from '@/components/layout/Navigation'

// Pages
import { Login } from '@/pages/Login'
import { AuthCallback } from '@/pages/AuthCallback'
import { DeviceClaim } from '@/pages/DeviceClaim'
import { Feed } from '@/pages/Feed'
import { Settings } from '@/pages/Settings'
import { Upload } from '@/pages/Upload'
import { Profile } from '@/pages/Profile'
import { Notifications } from '@/pages/Notifications'
import { Messages } from '@/pages/Messages'
import { Search } from '@/pages/Search'

/**
 * ProtectedRoute - Route guard that redirects to login if not authenticated
 */
function ProtectedRoute({ children }: { children: React.ReactNode }) {
  const { isAuthenticated } = useUserStore()

  if (!isAuthenticated) {
    return <Navigate to="/login" replace />
  }

  return <>{children}</>
}

/**
 * AppContent - Routes and Navigation
 */
function AppContent() {
  const { isAuthenticated } = useUserStore()

  return (
    <>
      {isAuthenticated && <Navigation />}
      <Routes>
        {/* Auth Routes */}
        <Route path="/login" element={<Login />} />
        <Route path="/auth/callback" element={<AuthCallback />} />
        <Route path="/claim/:deviceId" element={<DeviceClaim />} />

        {/* Feed Routes (Protected) */}
        <Route
          path="/feed"
          element={
            <ProtectedRoute>
              <Feed />
            </ProtectedRoute>
          }
        />

        {/* Settings Routes (Protected) */}
        <Route
          path="/settings"
          element={
            <ProtectedRoute>
              <Settings />
            </ProtectedRoute>
          }
        />

        {/* Notifications Routes (Protected) */}
        <Route
          path="/notifications"
          element={
            <ProtectedRoute>
              <Notifications />
            </ProtectedRoute>
          }
        />

        {/* Messages Routes (Protected) */}
        <Route
          path="/messages/:channelId?"
          element={
            <ProtectedRoute>
              <Messages />
            </ProtectedRoute>
          }
        />

        {/* Search Routes (Protected) */}
        <Route
          path="/search"
          element={
            <ProtectedRoute>
              <Search />
            </ProtectedRoute>
          }
        />

        {/* Upload Routes (Protected) */}
        <Route
          path="/upload"
          element={
            <ProtectedRoute>
              <Upload />
            </ProtectedRoute>
          }
        />

        {/* Profile Routes (Protected) */}
        <Route
          path="/profile/:username"
          element={
            <ProtectedRoute>
              <Profile />
            </ProtectedRoute>
          }
        />

        {/* Placeholder for other routes */}
        <Route path="/" element={<Navigate to="/feed" replace />} />
        <Route path="*" element={<Navigate to="/feed" replace />} />
      </Routes>
    </>
  )
}

/**
 * App - Root component with routing and providers
 */
function App() {
  return (
    <BrowserRouter>
      <QueryProvider>
        <AuthProvider>
          <ChatProvider>
            <AppContent />
          </ChatProvider>
        </AuthProvider>
      </QueryProvider>
    </BrowserRouter>
  )
}

export default App
