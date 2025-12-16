/**
 * RouteLoader - Loading state component for lazy-loaded routes
 * Displayed during route transitions when chunks are being loaded
 */
export function RouteLoader() {
  return (
    <div className="flex items-center justify-center min-h-screen bg-bg-primary">
      <div className="flex flex-col items-center gap-4">
        <div className="w-12 h-12 border-4 border-border border-t-coral-pink rounded-full animate-spin" />
        <p className="text-sm text-muted-foreground">Loading...</p>
      </div>
    </div>
  )
}
