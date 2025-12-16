import {
  Select,
  SelectContent,
  SelectItem,
  SelectTrigger,
  SelectValue,
} from '@/components/ui/select'

export type CommentAudience = 'everyone' | 'followers' | 'none'

interface CommentAudienceSelectorProps {
  value: CommentAudience
  onChange: (audience: CommentAudience) => void
}

const audienceOptions: Record<CommentAudience, { label: string; description: string; icon: string }> = {
  everyone: {
    label: 'Everyone',
    description: 'Anyone can comment on your post',
    icon: '🌍',
  },
  followers: {
    label: 'Followers Only',
    description: 'Only your followers can comment',
    icon: '👥',
  },
  none: {
    label: 'No Comments',
    description: 'Disable comments on this post',
    icon: '🚫',
  },
}

/**
 * CommentAudienceSelector - Control who can comment on a post
 *
 * Features:
 * - Select audience (everyone, followers, none)
 * - Visual feedback with icons
 */
export function CommentAudienceSelector({
  value,
  onChange,
}: CommentAudienceSelectorProps) {
  return (
    <div className="space-y-2">
      <label className="block text-sm font-medium text-foreground">
        Who can comment?
      </label>

      <Select value={value} onValueChange={(v) => onChange(v as CommentAudience)}>
        <SelectTrigger className="w-full">
          <SelectValue />
        </SelectTrigger>
        <SelectContent>
          {(Object.entries(audienceOptions) as Array<[CommentAudience, typeof audienceOptions['everyone']]>).map(
            ([key, option]) => (
              <SelectItem key={key} value={key}>
                <span className="flex items-center gap-2">
                  <span>{option.icon}</span>
                  {option.label}
                </span>
              </SelectItem>
            )
          )}
        </SelectContent>
      </Select>

      {/* Description */}
      <p className="text-xs text-muted-foreground">
        {audienceOptions[value].description}
      </p>
    </div>
  )
}
