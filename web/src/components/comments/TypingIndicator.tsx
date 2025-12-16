import { useEffect } from 'react'

interface TypingIndicatorProps {
  usernames: string[]
}

/**
 * TypingIndicator - Shows who is typing in comments
 */
export function TypingIndicator({ usernames }: TypingIndicatorProps) {
  useEffect(() => {
    // Animation is handled by CSS animate-bounce classes
  }, [])

  if (usernames.length === 0) {
    return null
  }

  let text = ''
  if (usernames.length === 1) {
    text = `${usernames[0]} is typing`
  } else if (usernames.length === 2) {
    text = `${usernames[0]} and ${usernames[1]} are typing`
  } else {
    text = `${usernames.slice(0, 2).join(', ')} and ${usernames.length - 2} more are typing`
  }

  return (
    <div className="flex items-center gap-2 py-2 px-3 text-sm text-muted-foreground">
      <div className="flex gap-1">
        <div className="w-1.5 h-1.5 rounded-full bg-muted-foreground animate-bounce" />
        <div className="w-1.5 h-1.5 rounded-full bg-muted-foreground animate-bounce delay-100" />
        <div className="w-1.5 h-1.5 rounded-full bg-muted-foreground animate-bounce delay-200" />
      </div>
      <span>{text}</span>
    </div>
  )
}
