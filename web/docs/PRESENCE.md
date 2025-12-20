# User Presence in Sidechain Web

## Architecture Overview

Sidechain web uses **GetStream.io Chat API exclusively** for user presence tracking. The plugin and web both connect directly to GetStream.io's presence system, which automatically manages online/offline status and real-time updates.

**Key**: No custom backend presence management needed. GetStream.io handles all presence lifecycle.

## GetStream.io Presence Fields

Each user object in GetStream.io includes:

- **`online`** (boolean): Automatically managed by GetStream.io
  - `true` when user is connected to chat
  - `false` after ~30 seconds of inactivity

- **`status`** (string): Custom status message set by user
  - Examples: "jamming", "in studio", "listening to loops"
  - Max 255 characters

- **`invisible`** (boolean): User appears offline while remaining connected
  - Persists across disconnections until explicitly set to `false`
  - Hides presence from other users

- **`last_active`** (ISO timestamp): Automatically updated
  - Updated on connection
  - Updated every 15 minutes of connection
  - Used to show "last seen" on profiles

## Web Implementation

### Chat SDK Integration

The web frontend uses `getstream.io/stream-chat-js` SDK which automatically handles presence:

```typescript
import StreamChat from 'stream-chat';

const client = StreamChat.getInstance(API_KEY);

// User connects - automatically marked online
await client.connectUser(
  { id: userId, name: username },
  userToken
);

// Listen to presence changes
client.on('user.presence.changed', (event) => {
  console.log(`User ${event.user.id} is now ${event.user.online ? 'online' : 'offline'}`);
  updateUserPresenceUI(event.user);
});

// Update custom status
await client.upsertUsers([
  {
    id: userId,
    status: "in studio",  // Custom field
    invisible: false
  }
]);

// Set invisible (appear offline)
await client.upsertUsers([
  {
    id: userId,
    invisible: true
  }
]);
```

### React Components

**User Presence Indicator** (`src/components/presence/UserPresenceIndicator.tsx`):

```typescript
interface UserPresenceIndicatorProps {
  user: ChatUser;
  showLastActive?: boolean;
}

export const UserPresenceIndicator: React.FC<UserPresenceIndicatorProps> = ({
  user,
  showLastActive = false
}) => {
  if (user.online) {
    return <span className="text-green-500">● Online</span>;
  }

  if (showLastActive && user.last_active) {
    const ago = formatTimeAgo(user.last_active);
    return <span className="text-gray-500">Last active: {ago}</span>;
  }

  return <span className="text-gray-500">● Offline</span>;
};
```

**Profile Card with Presence** (`src/components/profile/ProfileCard.tsx`):

```typescript
export const ProfileCard: React.FC<ProfileCardProps> = ({ user }) => {
  return (
    <div className="p-4 border rounded-lg">
      <h3>{user.name}</h3>
      <UserPresenceIndicator user={user} showLastActive={true} />
      {user.status && <p className="text-sm text-gray-600">{user.status}</p>}
    </div>
  );
};
```

### Zustand State Management

**Presence Store** (`src/stores/presenceStore.ts`):

```typescript
interface PresenceState {
  // Current user
  currentUserStatus: string | null;
  isInvisible: boolean;

  // Followed users presence
  followedUsersPresence: Map<string, ChatUser>;

  // Actions
  setCurrentUserStatus: (status: string) => Promise<void>;
  setInvisible: (invisible: boolean) => Promise<void>;
  updateUserPresence: (userId: string, user: ChatUser) => void;
}

export const usePresenceStore = create<PresenceState>((set) => ({
  currentUserStatus: null,
  isInvisible: false,
  followedUsersPresence: new Map(),

  setCurrentUserStatus: async (status) => {
    try {
      await chatClient.upsertUsers([{
        id: currentUserId,
        status
      }]);
      set({ currentUserStatus: status });
    } catch (error) {
      console.error('Failed to update status:', error);
    }
  },

  setInvisible: async (invisible) => {
    try {
      await chatClient.upsertUsers([{
        id: currentUserId,
        invisible
      }]);
      set({ isInvisible: invisible });
    } catch (error) {
      console.error('Failed to set invisible:', error);
    }
  },

  updateUserPresence: (userId, user) => {
    set(state => {
      const updated = new Map(state.followedUsersPresence);
      updated.set(userId, user);
      return { followedUsersPresence: updated };
    });
  }
}));
```

## Real-time Presence Updates

### Event Handling

The web frontend listens to GetStream.io presence events:

```typescript
useEffect(() => {
  const handlePresenceChange = (event: StreamEvent<UserPresenceChangedEvent>) => {
    const { user } = event;

    // Update local presence state
    presenceStore.updateUserPresence(user.id, user);

    // Update feed UI if user is in feed
    feedStore.updateAuthorPresence(user.id, user.online);
  };

  chatClient?.on('user.presence.changed', handlePresenceChange);
  chatClient?.on('user.online', handlePresenceChange);
  chatClient?.on('user.offline', handlePresenceChange);

  return () => {
    chatClient?.off('user.presence.changed', handlePresenceChange);
  };
}, [chatClient]);
```

### Display in Feed

When showing posts in the feed, display author's current presence:

```typescript
const FeedPost: React.FC<FeedPostProps> = ({ post }) => {
  return (
    <div className="post">
      <div className="post-author">
        <img src={post.author.avatar_url} alt="" />
        <div>
          <h4>{post.author.name}</h4>
          <UserPresenceIndicator user={post.author} />
        </div>
      </div>
      <audio src={post.audio_url} controls />
    </div>
  );
};
```

## Backend Integration (Minimal)

The backend only provides authentication tokens:

**GET `/api/v1/auth/stream-token`**

```typescript
// Response
{
  "status": "success",
  "token": "eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9...",
  "user_id": "user-123",
  "api_key": "public-key"
}
```

The frontend uses this token to connect directly to GetStream.io:

```typescript
const result = await authClient.getStreamToken();
if (result.isOk()) {
  const { token, api_key, user_id } = result.getValue();

  const chatClient = StreamChat.getInstance(api_key);
  await chatClient.connectUser({ id: user_id }, token);
}
```

## Settings: Show/Hide Presence

Users can control visibility of their presence:

**Activity Status Settings Component** (`src/components/settings/ActivityStatusSettings.tsx`):

```typescript
export const ActivityStatusSettings: React.FC = () => {
  const [showActivityStatus, setShowActivityStatus] = useState(true);
  const [showLastActive, setShowLastActive] = useState(true);

  const handleSave = async () => {
    // Store in user profile (via backend API)
    // This is metadata about privacy preferences
    await settingsClient.updateActivityStatus({
      show_activity_status: showActivityStatus,
      show_last_active: showLastActive
    });
  };

  return (
    <div>
      <label>
        <input
          type="checkbox"
          checked={showActivityStatus}
          onChange={e => setShowActivityStatus(e.target.checked)}
        />
        Show when I'm online/offline
      </label>
      <label>
        <input
          type="checkbox"
          checked={showLastActive}
          onChange={e => setShowLastActive(e.target.checked)}
        />
        Show when I was last active
      </label>
      <button onClick={handleSave}>Save</button>
    </div>
  );
};
```

When fetching user profiles, respect these privacy settings:

```typescript
const getUserProfile = (userId: string) => {
  const user = await userClient.getProfile(userId);

  // Check user's privacy preferences
  if (!user.show_activity_status) {
    user.online = undefined;  // Hide online status
    user.last_active = undefined;  // Hide last active
  }
  if (!user.show_last_active) {
    user.last_active = undefined;
  }

  return user;
};
```

## Testing Presence

### Test 1: Online Status
```
1. Open web app and go to feed
2. See current user marked "Online" in profile
3. Close tab/app
4. Wait 30+ seconds
5. Refresh with different account
6. See first user marked "Offline"
```

### Test 2: Custom Status
```
1. Go to settings/profile
2. Set status to "jamming"
3. Save
4. See status updates in real-time for followers
```

### Test 3: Invisible Mode
```
1. Go to settings
2. Enable "Invisible mode"
3. Other users don't see you online (despite being connected)
4. You can still see messages and activity
```

### Test 4: Last Active
```
1. Connect user A
2. Have user B follow user A
3. User B can see "Last active: 2 minutes ago"
4. Wait 15+ minutes of connection
5. Verify timestamp automatically updates in feed
```

## References

- **Stream Chat JS SDK**: https://getstream.io/chat/docs/javascript/
- **Presence Events**: https://getstream.io/chat/docs/javascript/event_object/
- **User Presence Format**: https://getstream.io/chat/docs/javascript/presence_format/
- **Stream Chat React SDK**: https://getstream.io/chat/docs/react/
