# Sidechain Web Frontend - Feature Implementation TODO

## Overview
Build feature parity with the VST plugin, minus audio recording (replaced with file upload). The web frontend should be a fully-featured social platform for sharing and discovering loops.

---

## Phase 1: Core Feed & Discovery (Foundation)
*Complete the basic social feed experience*

### Feed Management
- [ ] **Feed Timeline View** - User's followers' posts
  - [ ] Fetch timeline posts with pagination
  - [ ] Infinite scroll implementation
  - [ ] Loading states and skeleton loaders
  - [ ] Empty state messaging

- [ ] **Global Feed View** - All public posts
  - [ ] Browse all community posts
  - [ ] Global discovery of new producers

- [ ] **Trending Feed View** - Popular posts
  - [ ] Algorithm-based trending calculation
  - [ ] Time-windowed trending (today, this week, etc.)

- [ ] **For You Feed View** - Personalized recommendations
  - [ ] Integration with Gorse recommendation engine
  - [ ] Personalized feed based on user behavior
  - [ ] Real-time updates as user engages

### Post Interactions (Core)
- [ ] **Like/Unlike Posts**
  - [ ] Visual feedback and optimistic updates
  - [ ] Like count updates
  - [ ] Like animation/feedback

- [ ] **Save/Unsave Posts**
  - [ ] Add to personal collection
  - [ ] Saved posts view/page
  - [ ] Save count tracking

- [ ] **Repost/Unrepost**
  - [ ] Share to followers
  - [ ] Repost count tracking
  - [ ] Visual indication of own reposts

- [ ] **Reactions/Emojis**
  - [ ] Multi-emoji reaction system
  - [ ] Emoji picker UI
  - [ ] Reaction count aggregation

- [ ] **Audio Playback**
  - [ ] Global audio player (persistent across page navigation)
  - [ ] Play/pause controls
  - [ ] Progress bar and seeking
  - [ ] Volume control
  - [ ] Current time and duration display
  - [ ] Waveform visualization
  - [ ] Now playing indicator on posts

---

## Phase 2: Post Creation & Upload
*Enable users to share their loops*

### File Upload System
- [ ] **Loop File Upload**
  - [ ] Drag-and-drop file input
  - [ ] File browser input fallback
  - [ ] File type validation (MP3, WAV, etc.)
  - [ ] File size limits (show before upload)
  - [ ] Multiple file format support

- [ ] **Upload Progress**
  - [ ] Progress bar with percentage
  - [ ] Upload speed display
  - [ ] Cancel upload functionality
  - [ ] Retry on failure
  - [ ] Chunk-based upload for large files

### Loop Metadata
- [ ] **Audio Metadata Form**
  - [ ] Loop title/name
  - [ ] Description/notes
  - [ ] Genre tags (multi-select)
  - [ ] BPM (tempo) input
  - [ ] Key signature (C, Cm, D, etc.)
  - [ ] DAW/software used
  - [ ] Mood/vibe tags

- [ ] **Waveform Generation**
  - [ ] Generate waveform during upload
  - [ ] Display preview waveform
  - [ ] Cache waveform on backend

- [ ] **Preview Before Upload**
  - [ ] Play uploaded file
  - [ ] Edit metadata before final submission
  - [ ] Cancel and re-upload option

### Draft Management
- [ ] **Save Drafts Locally**
  - [ ] Auto-save draft metadata to localStorage
  - [ ] Persist incomplete uploads
  - [ ] Drafts list view
  - [ ] Resume uploads from draft
  - [ ] Delete drafts

- [ ] **Draft Recovery**
  - [ ] Recover unsaved posts on page reload
  - [ ] Show notification for existing drafts

---

## Phase 3: Comments & Discussions
*Foster community engagement*

### Comments System
- [ ] **View Comments**
  - [ ] Fetch comments with pagination
  - [ ] Infinite scroll for comment threads
  - [ ] Nested reply threads
  - [ ] Sort by newest/oldest/most liked

- [ ] **Create Comments**
  - [ ] Comment text input
  - [ ] Reply to specific comments
  - [ ] @mention users in comments
  - [ ] Emoji support in comments
  - [ ] Markdown preview (bold, italic, code)

- [ ] **Comment Interactions**
  - [ ] Like/unlike comments
  - [ ] Comment like count display
  - [ ] Delete own comments
  - [ ] Edit own comments
  - [ ] Report inappropriate comments

- [ ] **Comment Notifications**
  - [ ] Notify when someone replies to user's comment
  - [ ] Notify when comment is liked
  - [ ] Notify on @mentions in comments

---

## Phase 4: User Profiles & Social
*Build follower relationships and user discovery*

### User Profiles
- [ ] **View User Profiles**
  - [ ] User avatar/profile picture
  - [ ] User bio/description
  - [ ] Follower/following counts
  - [ ] User's public posts feed
  - [ ] User's saved collections/playlists

- [ ] **Edit Own Profile**
  - [ ] Edit display name
  - [ ] Edit bio/description
  - [ ] Upload profile picture
  - [ ] Cover image/header
  - [ ] Social links (Twitter, Instagram, etc.)
  - [ ] Pro/verified badge request

- [ ] **User Statistics**
  - [ ] Total posts uploaded
  - [ ] Total followers
  - [ ] Total likes received
  - [ ] Join date
  - [ ] Last active

### Follow System
- [ ] **Follow/Unfollow Users**
  - [ ] Follow button on profiles
  - [ ] Follow/unfollow from post cards
  - [ ] Follow notification to user
  - [ ] Followers list view
  - [ ] Following list view

- [ ] **Follow Notifications**
  - [ ] Notify when someone follows
  - [ ] Block/mute followers

### User Discovery
- [ ] **Trending Producers Page**
  - [ ] Most followed users this week
  - [ ] Fastest growing creators
  - [ ] Users by genre

- [ ] **Suggested Follows**
  - [ ] Recommend users to follow based on listening history
  - [ ] Display on sidebar/recommendation widget

- [ ] **User Search**
  - [ ] Search by username/display name
  - [ ] Filter by genre specialization
  - [ ] View search results with profiles

---

## Phase 5: Notifications
*Keep users informed of activity*

### Notification System
- [ ] **Notification Types**
  - [ ] Post liked
  - [ ] Post commented
  - [ ] Comment replied to
  - [ ] New follower
  - [ ] User followed back
  - [ ] @mention in comment
  - [ ] Repost of user's post
  - [ ] Playlist shared with user

- [ ] **Notification Bell/Center**
  - [ ] Notification bell icon in header
  - [ ] Unread notification count
  - [ ] Notification center/inbox page
  - [ ] Real-time notification updates (WebSocket)
  - [ ] Mark notifications as read
  - [ ] Delete notifications
  - [ ] Clear all notifications

- [ ] **Notification Preferences**
  - [ ] Toggle notification types on/off
  - [ ] Email notification settings
  - [ ] Push notification settings (if PWA)
  - [ ] Mute specific users' notifications

---

## Phase 6: Direct Messaging & Chat
*Enable private communication*

### Stream Chat Integration
- [ ] **Messaging UI**
  - [ ] Channel list sidebar
  - [ ] Message thread view
  - [ ] Typing indicators
  - [ ] Read receipts
  - [ ] User online/offline status

- [ ] **Send Messages**
  - [ ] Text message input
  - [ ] Emoji picker
  - [ ] Message reactions
  - [ ] Edit sent messages
  - [ ] Delete messages

- [ ] **Message Features**
  - [ ] File/image sharing
  - [ ] Message search within thread
  - [ ] Pinned messages
  - [ ] Thread replies

- [ ] **Channel Management**
  - [ ] Create direct message with user
  - [ ] Start group chat
  - [ ] Channel settings/info
  - [ ] Leave/mute channels
  - [ ] Add/remove members from group

---

## Phase 7: Search & Filtering
*Help users find what they're looking for*

### Global Search
- [ ] **Search UI**
  - [ ] Search bar in header
  - [ ] Search suggestions as user types
  - [ ] Search results page

- [ ] **Search Filters**
  - [ ] Search by loop name/title
  - [ ] Filter by genre
  - [ ] Filter by BPM range
  - [ ] Filter by key signature
  - [ ] Filter by DAW
  - [ ] Filter by date (newest, oldest)
  - [ ] Sort results (relevance, trending, newest)

- [ ] **Advanced Search**
  - [ ] Search by producer name
  - [ ] Search by mood/vibe tags
  - [ ] Saved search queries
  - [ ] Search history

### Collections/Playlists
- [ ] **Create Playlists**
  - [ ] Create public/private playlists
  - [ ] Add posts to playlists
  - [ ] Reorder posts in playlist
  - [ ] Edit playlist metadata (name, description, cover)
  - [ ] Delete playlists

- [ ] **Share Playlists**
  - [ ] Share playlist link
  - [ ] Embed playlist
  - [ ] Collaborate on playlists (invite others)

---

## Phase 8: Real-Time Updates
*Keep content fresh and engaging*

### WebSocket Integration
- [ ] **Feed Updates**
  - [ ] New posts appear in real-time
  - [ ] Like counts update live
  - [ ] Comment counts update live
  - [ ] User follows in real-time

- [ ] **Live Notifications**
  - [ ] Push notifications via WebSocket
  - [ ] Toast/badge notifications
  - [ ] Sound notifications (optional)

- [ ] **Typing Indicators**
  - [ ] Show when someone is typing a comment
  - [ ] Show in direct messages

---

## Phase 9: Discovery & Recommendations
*Leverage Gorse for personalized content*

### Recommendations
- [ ] **Recommended Posts Widget**
  - [ ] Show on feed sidebar
  - [ ] "You might like" section
  - [ ] Genre-specific recommendations

- [ ] **Recommended Producers**
  - [ ] Based on listening history
  - [ ] Based on follows
  - [ ] "People also follow" sections

- [ ] **Trending This Week**
  - [ ] Genre-specific trending
  - [ ] Genre breakdown view

---

## Phase 10: User Settings & Preferences
*Personalization and control*

### Account Settings
- [ ] **Profile Settings** (covered in Phase 4)

- [ ] **Privacy Settings**
  - [ ] Public/private profile toggle
  - [ ] Allow/disallow comments on posts
  - [ ] Allow DMs from followers only
  - [ ] Block users list
  - [ ] Blocked by users (if visible)

- [ ] **Content Preferences**
  - [ ] Preferred genres (for recommendations)
  - [ ] Preferred DAWs (for discovery)
  - [ ] Content moderation settings
  - [ ] NSFW content filtering

- [ ] **Theme & Accessibility**
  - [ ] Dark/light mode toggle
  - [ ] Font size adjustment
  - [ ] Reduce motion settings
  - [ ] Keyboard shortcuts help

- [ ] **Audio Preferences**
  - [ ] Auto-play on scroll
  - [ ] Volume memory
  - [ ] Audio quality preference

- [ ] **Email Preferences**
  - [ ] Email notification frequency
  - [ ] Digest email settings
  - [ ] Newsletter subscription

---

## Phase 11: Admin Features
*Moderation and platform management*

### Content Moderation
- [ ] **Report System**
  - [ ] Report posts, comments, users
  - [ ] Report reasons (spam, copyright, etc.)
  - [ ] Report submission and tracking

- [ ] **Admin Dashboard** (separate route/permissions)
  - [ ] Pending reports queue
  - [ ] User moderation actions
  - [ ] Content removal tools
  - [ ] Ban/suspend users
  - [ ] Analytics and platform stats

---

## Phase 12: Performance & Optimization
*Ensure smooth user experience at scale*

### Performance
- [ ] **Code Splitting**
  - [ ] Route-based code splitting
  - [ ] Component lazy loading
  - [ ] Image lazy loading

- [ ] **Caching**
  - [ ] HTTP caching headers
  - [ ] Service Worker caching
  - [ ] Browser caching strategy

- [ ] **Image Optimization**
  - [ ] Avatar thumbnail optimization
  - [ ] Waveform image optimization
  - [ ] Responsive images
  - [ ] WebP format support with fallback

- [ ] **Database Query Optimization**
  - [ ] N+1 query elimination
  - [ ] Query result caching
  - [ ] Pagination limits

### Bundle Size
- [ ] **Analyze Bundle**
  - [ ] Identify large dependencies
  - [ ] Remove unused code
  - [ ] Tree shake unused exports

- [ ] **Monitoring**
  - [ ] Web Vitals tracking (LCP, FID, CLS)
  - [ ] Error boundary tracking
  - [ ] Performance metrics dashboard

---

## Phase 13: Testing
*Ensure quality and reliability*

### Unit Tests
- [ ] **API Client Tests**
  - [ ] Test all API methods
  - [ ] Test error handling
  - [ ] Test request/response transformation

- [ ] **Store Tests**
  - [ ] Test Zustand store mutations
  - [ ] Test optimistic updates
  - [ ] Test error rollback

- [ ] **Model Tests**
  - [ ] Test data model validation
  - [ ] Test serialization/deserialization

### Component Tests
- [ ] **Feed Components**
  - [ ] Test FeedList rendering
  - [ ] Test PostCard interactions
  - [ ] Test infinite scroll

- [ ] **Form Components**
  - [ ] Test upload form validation
  - [ ] Test login form
  - [ ] Test profile edit form

- [ ] **Integration Tests**
  - [ ] Test complete user flows
  - [ ] Test OAuth flow
  - [ ] Test post creation to feed display

### E2E Tests
- [ ] **Critical User Flows**
  - [ ] Login → View Feed → Like Post → Comment
  - [ ] Upload Loop → View in Feed → Share
  - [ ] Follow User → See Their Posts → Message
  - [ ] Create Playlist → Add Posts → Share

---

## Phase 14: Deployment & DevOps
*Get the app in front of users*

### Build & Deployment
- [ ] **Production Build**
  - [ ] Optimize Vite build
  - [ ] Environment-specific builds
  - [ ] Build artifact verification

- [ ] **Hosting**
  - [ ] Choose hosting provider (Vercel, Netlify, AWS, etc.)
  - [ ] Setup CI/CD pipeline
  - [ ] Automated deployments

- [ ] **Monitoring & Logging**
  - [ ] Error tracking (Sentry)
  - [ ] Analytics tracking (Mixpanel, PostHog)
  - [ ] Performance monitoring
  - [ ] Log aggregation

### Security
- [ ] **HTTPS & SSL**
  - [ ] SSL certificate setup
  - [ ] HSTS header

- [ ] **CORS Configuration**
  - [ ] Restrict API endpoints
  - [ ] Handle preflight requests

- [ ] **Security Headers**
  - [ ] Content-Security-Policy
  - [ ] X-Frame-Options
  - [ ] X-Content-Type-Options

---

## Phase 15: Future Enhancements
*Post-launch features*

### Advanced Features
- [ ] **Collaboration**
  - [ ] Collab requests/acceptance
  - [ ] Shared project workspace
  - [ ] Version control for loops

- [ ] **Monetization**
  - [ ] Loop licensing
  - [ ] Creator earnings
  - [ ] Subscription tiers

- [ ] **Creator Tools**
  - [ ] Analytics dashboard
  - [ ] Follower growth charts
  - [ ] Revenue tracking
  - [ ] A/B testing tools

- [ ] **Social Features**
  - [ ] Live streaming
  - [ ] Spaces/communities
  - [ ] Events/beatmaker battles
  - [ ] Producer challenges

- [ ] **AI Features**
  - [ ] AI remix suggestions
  - [ ] BPM/key detection
  - [ ] Mood analysis
  - [ ] Smart recommendations

---

## Implementation Priority Order

**Priority 1 (MVP - Weeks 1-2)**
- Phase 1: Core Feed & Discovery
- Phase 2: Post Creation & Upload
- Phase 3: Comments (basic)

**Priority 2 (Foundation - Weeks 3-4)**
- Phase 4: User Profiles & Social
- Phase 5: Notifications
- Phase 6: Direct Messaging

**Priority 3 (Polish - Weeks 5-6)**
- Phase 7: Search & Filtering
- Phase 8: Real-Time Updates
- Phase 9: Discovery & Recommendations

**Priority 4 (Launch Prep - Weeks 7-8)**
- Phase 10: User Settings
- Phase 13: Testing
- Phase 14: Deployment

**Priority 5 (Post-Launch)**
- Phase 11: Admin Features
- Phase 12: Performance Optimization
- Phase 15: Future Enhancements

---

## Dependency Tree

```
Phase 1 (Feed)
├─ Phase 3 (Comments) - requires posts to exist
├─ Phase 4 (Profiles) - requires users and posts
├─ Phase 5 (Notifications) - requires interactions
├─ Phase 6 (Chat) - requires users
├─ Phase 7 (Search) - requires posts and users
├─ Phase 8 (Real-time) - builds on Phases 1-7
├─ Phase 9 (Recommendations) - requires user data
└─ Phase 10 (Settings) - enhances all phases

Phase 2 (Upload)
├─ Phase 1 (Feed) - posts must exist to display
├─ Phase 3 (Comments) - on uploaded posts
└─ Phase 4 (Profiles) - show user's uploads

Phase 11 (Admin)
└─ All other phases - needs content to moderate

Phase 12 (Perf)
└─ All other phases - applies to everything

Phase 13 (Testing)
└─ All other phases - tests all features

Phase 14 (Deployment)
└─ All other phases - deploys everything
```

---

## Success Metrics

- **Feed Load Time**: < 2 seconds for initial load
- **Feed Scroll**: Smooth 60 FPS scrolling
- **Upload Speed**: < 30 seconds for typical 5MB file
- **Comment Creation**: < 1 second feedback
- **Real-time Updates**: < 500ms latency for WebSocket events
- **Mobile Responsive**: Works well on iOS/Android
- **Accessibility**: WCAG 2.1 AA compliance
- **Test Coverage**: > 80% code coverage
- **Performance**: Core Web Vitals all green

---

## Notes

- All timestamps should be relative ("2 hours ago", "yesterday")
- All media should be optimized (compressed, responsive)
- All forms should have proper validation and error messages
- All async operations need loading/error states
- Dark mode should be default (consistent with plugin)
- Mobile-first responsive design
- Keyboard navigation support throughout
- Proper focus management for accessibility
- Skeleton loaders for better perceived performance
