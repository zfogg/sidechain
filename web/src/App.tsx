import { Suspense, lazy, BrowserRouter, Routes, Route, Navigate, useEffect } from 'react-router-dom'
import { QueryProvider } from '@/providers/QueryProvider'
import { AuthProvider } from '@/providers/AuthProvider'
import { ChatProvider } from '@/providers/ChatProvider'
import { useUserStore } from '@/stores/useUserStore'
import { Navigation } from '@/components/layout/Navigation'
import { RouteLoader } from '@/components/layout/RouteLoader'
import { PerformanceMonitor } from '@/components/dev/PerformanceMonitor'
import { useWebVitals } from '@/hooks/useWebVitals'

// Auth Routes (eager load - required before auth check)
import { Login } from '@/pages/Login'
import { AuthCallback } from '@/pages/AuthCallback'
import { DeviceClaim } from '@/pages/DeviceClaim'

// Protected Routes (lazy load - reduces initial bundle)
const Feed = lazy(() => import('@/pages/Feed').then(m => ({ default: m.Feed })))
const Settings = lazy(() => import('@/pages/Settings').then(m => ({ default: m.Settings })))
const Upload = lazy(() => import('@/pages/Upload').then(m => ({ default: m.Upload })))
const Profile = lazy(() => import('@/pages/Profile').then(m => ({ default: m.Profile })))
const Notifications = lazy(() => import('@/pages/Notifications').then(m => ({ default: m.Notifications })))
const Messages = lazy(() => import('@/pages/Messages').then(m => ({ default: m.Messages })))
const Search = lazy(() => import('@/pages/Search').then(m => ({ default: m.Search })))
const Discovery = lazy(() => import('@/pages/Discovery').then(m => ({ default: m.Discovery })))
const Playlists = lazy(() => import('@/pages/Playlists').then(m => ({ default: m.Playlists })))
const PlaylistDetail = lazy(() => import('@/pages/PlaylistDetail').then(m => ({ default: m.PlaylistDetail })))

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

  // Track Web Vitals in development
  useWebVitals()

  return (
    <>
      {isAuthenticated && <Navigation />}
      <PerformanceMonitor />
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

        {/* Discovery Routes (Protected) */}
        <Route
          path="/discover"
          element={
            <ProtectedRoute>
              <Suspense fallback={<RouteLoader />}>
                <Discovery />
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
