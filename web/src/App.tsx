import { Suspense, lazy } from 'react'
import { BrowserRouter, Routes, Route, Navigate } from 'react-router-dom'
import { QueryProvider } from '@/providers/QueryProvider'
import { AuthProvider } from '@/providers/AuthProvider'
import { ChatProvider } from '@/providers/ChatProvider'
import { useUserStore } from '@/stores/useUserStore'
import { Navigation } from '@/components/layout/Navigation'
import { RouteLoader } from '@/components/layout/RouteLoader'
import { PerformanceMonitor } from '@/components/dev/PerformanceMonitor'
import { useWebVitals } from '@/hooks/useWebVitals'
import { useKeyboardShortcuts } from '@/hooks/useKeyboardShortcuts'
import { KeyboardShortcutsDialog } from '@/components/shortcuts/KeyboardShortcutsDialog'

// Auth Routes (eager load - required before auth check)
import { Login } from '@/pages/Login'
import { AuthCallback } from '@/pages/AuthCallback'
import { DeviceClaim } from '@/pages/DeviceClaim'
import { Privacy } from '@/pages/Privacy'
import { Terms } from '@/pages/Terms'

// Protected Routes (lazy load - reduces initial bundle)
const Feed = lazy(() => import('@/pages/Feed').then(m => ({ default: m.Feed })))
const Settings = lazy(() => import('@/pages/Settings').then(m => ({ default: m.Settings })))
const Upload = lazy(() => import('@/pages/Upload').then(m => ({ default: m.Upload })))
const Profile = lazy(() => import('@/pages/Profile').then(m => ({ default: m.Profile })))
const Notifications = lazy(() => import('@/pages/Notifications').then(m => ({ default: m.Notifications })))
const Messages = lazy(() => import('@/pages/Messages').then(m => ({ default: m.Messages })))
const Search = lazy(() => import('@/pages/Search').then(m => ({ default: m.Search })))
const Playlists = lazy(() => import('@/pages/Playlists').then(m => ({ default: m.Playlists })))
const PlaylistDetail = lazy(() => import('@/pages/PlaylistDetail').then(m => ({ default: m.PlaylistDetail })))
const Activity = lazy(() => import('@/pages/Activity').then(m => ({ default: m.Activity })))
const Remixes = lazy(() => import('@/pages/Remixes').then(m => ({ default: m.Remixes })))

/**
 * ProtectedRoute - Route guard that redirects to login if not authenticated
 */
function ProtectedRoute({ children }: { children: React.ReactNode }) {
  const { isAuthenticated } = useUserStore()

  if (!isAuthenticated) {
    return <Navigate to="/login" replace />
  }

  return <ChatProvider>{children}</ChatProvider>
}

/**
 * RootRoute - Smart root route that shows login if not authenticated, feed if authenticated
 */
function RootRoute() {
  const { isAuthenticated } = useUserStore()

  if (!isAuthenticated) {
    return <Login />
  }

  return (
    <ChatProvider>
      <Suspense fallback={<RouteLoader />}>
        <Feed />
      </Suspense>
    </ChatProvider>
  )
}

/**
 * AppContent - Routes and Navigation
 */
function AppContent() {
  const { isAuthenticated } = useUserStore()

  // Track Web Vitals in development
  useWebVitals()

  // Setup keyboard shortcuts
  useKeyboardShortcuts()

  return (
    <div className="flex flex-col h-full w-full">
      {isAuthenticated && <nav className="flex-shrink-0"><Navigation /></nav>}
      <PerformanceMonitor />
      <KeyboardShortcutsDialog />
      <div className="flex-1 min-h-0 overflow-x-hidden">
        <Routes>
        {/* Public Routes - No Chat Provider */}
        <Route path="/login" element={<Login />} />
        <Route path="/auth/callback" element={<AuthCallback />} />
        <Route path="/claim/:deviceId" element={<DeviceClaim />} />
        <Route path="/privacy" element={<Privacy />} />
        <Route path="/terms" element={<Terms />} />

        {/* Protected Routes - Wrapped with Chat Provider */}
        <Route
          path="/feed"
          element={
            <ProtectedRoute>
              <Suspense fallback={<RouteLoader />}>
                <Feed />
              </Suspense>
            </ProtectedRoute>
          }
        />

        {/* Settings Routes (Protected) */}
        <Route
          path="/settings"
          element={
            <ProtectedRoute>
              <Suspense fallback={<RouteLoader />}>
                <Settings />
              </Suspense>
            </ProtectedRoute>
          }
        />

        {/* Notifications Routes (Protected) */}
        <Route
          path="/notifications"
          element={
            <ProtectedRoute>
              <Suspense fallback={<RouteLoader />}>
                <Notifications />
              </Suspense>
            </ProtectedRoute>
          }
        />

        {/* Messages Routes (Protected) */}
        <Route
          path="/messages/:channelId?"
          element={
            <ProtectedRoute>
              <Suspense fallback={<RouteLoader />}>
                <Messages />
              </Suspense>
            </ProtectedRoute>
          }
        />

        {/* Search Routes (Protected) */}
        <Route
          path="/search"
          element={
            <ProtectedRoute>
              <Suspense fallback={<RouteLoader />}>
                <Search />
              </Suspense>
            </ProtectedRoute>
          }
        />

        {/* Activity Routes (Protected) */}
        <Route
          path="/activity"
          element={
            <ProtectedRoute>
              <Suspense fallback={<RouteLoader />}>
                <Activity />
              </Suspense>
            </ProtectedRoute>
          }
        />

        {/* Upload Routes (Protected) */}
        <Route
          path="/upload"
          element={
            <ProtectedRoute>
              <Suspense fallback={<RouteLoader />}>
                <Upload />
              </Suspense>
            </ProtectedRoute>
          }
        />

        {/* Profile Routes (Protected) */}
        <Route
          path="/profile/:username"
          element={
            <ProtectedRoute>
              <Suspense fallback={<RouteLoader />}>
                <Profile />
              </Suspense>
            </ProtectedRoute>
          }
        />

        {/* Playlists Routes (Protected) */}
        <Route
          path="/playlists"
          element={
            <ProtectedRoute>
              <Suspense fallback={<RouteLoader />}>
                <Playlists />
              </Suspense>
            </ProtectedRoute>
          }
        />

        <Route
          path="/playlists/:playlistId"
          element={
            <ProtectedRoute>
              <Suspense fallback={<RouteLoader />}>
                <PlaylistDetail />
              </Suspense>
            </ProtectedRoute>
          }
        />

        {/* Remixes Routes (Protected) */}
        <Route
          path="/posts/:postId/remixes"
          element={
            <ProtectedRoute>
              <Suspense fallback={<RouteLoader />}>
                <Remixes />
              </Suspense>
            </ProtectedRoute>
          }
        />

        {/* Root Route - Smart routing: login if not authenticated, feed if authenticated */}
        <Route path="/" element={<RootRoute />} />
        <Route path="*" element={<Navigate to="/" replace />} />
      </Routes>
      </div>
    </div>
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
          <AppContent />
        </AuthProvider>
      </QueryProvider>
    </BrowserRouter>
  )
}

export default App
