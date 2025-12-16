import { BrowserRouter, Routes, Route, Navigate } from 'react-router-dom'
import { QueryProvider } from '@/providers/QueryProvider'
import { AuthProvider } from '@/providers/AuthProvider'
import { ChatProvider } from '@/providers/ChatProvider'
import { useUserStore } from '@/stores/useUserStore'

// Pages
import { Login } from '@/pages/Login'
import { AuthCallback } from '@/pages/AuthCallback'
import { DeviceClaim } from '@/pages/DeviceClaim'
import { Feed } from '@/pages/Feed'
import { Settings } from '@/pages/Settings'

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
 * App - Root component with routing and providers
 */
function App() {
  return (
    <BrowserRouter>
      <QueryProvider>
        <AuthProvider>
          <ChatProvider>
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

            {/* Placeholder for other routes */}
            <Route path="/" element={<Navigate to="/feed" replace />} />
            <Route path="*" element={<Navigate to="/feed" replace />} />
            </Routes>
          </ChatProvider>
        </AuthProvider>
      </QueryProvider>
    </BrowserRouter>
  )
}

export default App
