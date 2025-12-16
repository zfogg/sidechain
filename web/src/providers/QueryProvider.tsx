import { ReactNode } from 'react'
import { QueryClient, QueryClientProvider } from '@tanstack/react-query'

/**
 * Create a QueryClient instance with sensible defaults
 * Separate instance prevents sharing state between tests
 */
const createQueryClient = () => {
  return new QueryClient({
    defaultOptions: {
      queries: {
        // Cache successful queries for 1 minute
        staleTime: 60 * 1000,
        // Keep unused queries in cache for 5 minutes before garbage collection
        gcTime: 5 * 60 * 1000,
        // Retry failed requests once
        retry: 1,
        // Disable automatic refetching in background
        refetchOnWindowFocus: false,
      },
      mutations: {
        // Retry failed mutations once
        retry: 1,
      },
    },
  })
}

// Singleton instance for the app
const queryClient = createQueryClient()

interface QueryProviderProps {
  children: ReactNode
}

/**
 * QueryProvider - Wraps the app with React Query's QueryClientProvider
 * Enables useQuery, useMutation, and related hooks
 *
 * Mirrors C++ caching patterns with configurable stale time and garbage collection
 */
export function QueryProvider({ children }: QueryProviderProps) {
  return (
    <QueryClientProvider client={queryClient}>
      {children}
    </QueryClientProvider>
  )
}

// Export for testing purposes
export { queryClient, createQueryClient }
