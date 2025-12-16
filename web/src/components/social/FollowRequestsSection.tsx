import { useState } from 'react'
import { useQuery } from '@tanstack/react-query'
import { SocialClient } from '@/api/SocialClient'
import { Button } from '@/components/ui/button'
import {
  Dialog,
  DialogContent,
  DialogDescription,
  DialogHeader,
  DialogTitle,
  DialogTrigger,
} from '@/components/ui/dialog'
import { FollowRequestsList } from './FollowRequestsList'

/**
 * FollowRequestsSection - Manage follow requests from settings or profile menu
 *
 * Features:
 * - Shows badge with pending request count
 * - Opens dialog to view and manage requests
 * - Integrates with FollowRequestsList component
 */
export function FollowRequestsSection() {
  const [isOpen, setIsOpen] = useState(false)

  // Get count of pending requests
  const { data: requests = [] } = useQuery({
    queryKey: ['followRequestsCount'],
    queryFn: async () => {
      const result = await SocialClient.getPendingFollowRequests(1, 0)
      if (result.isError()) {
        return []
      }
      return result.getValue()
    },
    staleTime: 1 * 60 * 1000, // 1 minute
    gcTime: 10 * 60 * 1000, // 10 minutes
  })

  if (!requests || requests.length === 0) {
    return null
  }

  return (
    <Dialog open={isOpen} onOpenChange={setIsOpen}>
      <DialogTrigger asChild>
        <Button variant="outline" className="relative w-full justify-start gap-2">
          <span>🔔</span>
          <span>Follow Requests</span>
          {requests.length > 0 && (
            <span className="absolute top-1 right-2 inline-flex items-center justify-center px-2 py-0.5 text-xs font-bold leading-none text-white transform translate-x-1/2 -translate-y-1/2 bg-coral-pink rounded-full">
              {requests.length}
            </span>
          )}
        </Button>
      </DialogTrigger>

      <DialogContent className="sm:max-w-md max-h-[600px] overflow-y-auto">
        <DialogHeader>
          <DialogTitle>Follow Requests</DialogTitle>
          <DialogDescription>
            Manage pending follow requests to your account
          </DialogDescription>
        </DialogHeader>

        <FollowRequestsList />
      </DialogContent>
    </Dialog>
  )
}
