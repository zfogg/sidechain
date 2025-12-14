# Sidechain Website & Admin Dashboard

> **Vision**: A comprehensive web presence for Sidechain with marketing pages, user profiles, and a powerful admin dashboard
> **Stack**: Next.js 14+ (App Router), TypeScript, Tailwind CSS, shadcn/ui
> **Last Updated**: December 13, 2025

---

## Executive Summary

The Sidechain website serves multiple purposes:
1. **Marketing Site**: Landing page, features, pricing (if applicable), download links
2. **Web Profiles**: Public user profiles viewable in browser (like instagram.com/username)
3. **Web Feed**: Browse posts/content in browser (discovery for non-plugin users)
4. **Admin Dashboard**: Comprehensive admin interface for moderation, analytics, and system management

**Note on Stream.io Moderation**: Stream.io provides a built-in moderation dashboard for chat/messaging that handles:
- Message moderation (flag, delete, edit)
- Image moderation (auto-flagging inappropriate images)
- User bans at the chat level
- Activity feed moderation
- Channel management

We should **link to the Stream.io dashboard** from our admin panel rather than duplicating this functionality. Our admin dashboard focuses on **Sidechain-specific** moderation (posts, comments, reports, user accounts).

---

## Priority Legend

- **P0 - Critical**: Must have for launch
- **P1 - High**: Expected for professional product
- **P2 - Medium**: Enhances experience
- **P3 - Low**: Future enhancements

---

## Architecture

### Tech Stack
```
Frontend:
├── Next.js 14+ (App Router)
├── TypeScript
├── Tailwind CSS
├── shadcn/ui components
├── React Query (data fetching)
├── Zustand (state management)
├── Recharts (analytics charts)
└── Lucide Icons

Backend:
└── Existing Go backend API (backend/)

Authentication:
├── NextAuth.js
├── JWT from backend API
└── Admin-specific roles/permissions
```

### Project Structure
```
website/
├── app/
│   ├── (marketing)/          # Public marketing pages
│   │   ├── page.tsx          # Landing page
│   │   ├── features/
│   │   ├── download/
│   │   └── about/
│   ├── (profiles)/           # Public user profiles
│   │   └── [username]/
│   ├── (feed)/               # Public feed browsing
│   │   ├── explore/
│   │   └── post/[id]/
│   ├── admin/                # Admin dashboard (protected)
│   │   ├── layout.tsx
│   │   ├── page.tsx          # Dashboard home
│   │   ├── users/
│   │   ├── content/
│   │   ├── moderation/
│   │   ├── analytics/
│   │   ├── challenges/
│   │   └── system/
│   └── api/                  # API routes (proxy to backend)
├── components/
│   ├── ui/                   # shadcn/ui components
│   ├── marketing/
│   ├── profiles/
│   ├── feed/
│   └── admin/
├── lib/
│   ├── api.ts                # API client
│   ├── auth.ts               # Auth helpers
│   └── utils.ts
└── public/
    └── assets/
```

---

## P0: Critical - Must Have for Launch

### 1. Project Setup

**TODO**:
- [ ] **1.1** Initialize Next.js 14 project with TypeScript
  ```bash
  npx create-next-app@latest website --typescript --tailwind --app --src-dir
  ```

- [ ] **1.2** Setup shadcn/ui
  ```bash
  npx shadcn-ui@latest init
  npx shadcn-ui@latest add button card input table dialog sheet ...
  ```

- [ ] **1.3** Configure API client
  - Create typed API client for backend
  - Setup React Query for data fetching
  - Handle authentication tokens
  - Error handling and retries

- [ ] **1.4** Setup authentication
  - NextAuth.js configuration
  - Login via backend API
  - Session management
  - Protected routes for admin

- [ ] **1.5** Configure environment variables
  ```env
  NEXT_PUBLIC_API_URL=https://api.sidechain.app
  NEXT_PUBLIC_CDN_URL=https://cdn.sidechain.app
  NEXTAUTH_SECRET=xxx
  NEXTAUTH_URL=https://sidechain.app
  ```

### 2. Marketing Landing Page

**TODO**:
- [ ] **2.1** Hero section
  - Compelling headline: "The Social Network Inside Your DAW"
  - Subheadline explaining the value prop
  - CTA button: "Download for Free"
  - Animated preview of plugin UI

- [ ] **2.2** Features section
  - Grid of feature cards with icons
  - Features: Record & Share, Social Feed, Stories, DMs, MIDI Challenges, etc.
  - Animated illustrations or screenshots

- [ ] **2.3** DAW compatibility section
  - Icons for supported DAWs (Ableton, FL Studio, Logic, etc.)
  - "Works with your favorite DAW"

- [ ] **2.4** How it works section
  - Step-by-step visual guide
  - 1. Install plugin → 2. Record a loop → 3. Share with community

- [ ] **2.5** Testimonials/Social proof
  - Producer quotes
  - User count ("10,000+ producers")
  - Featured artists

- [ ] **2.6** Download section
  - Download buttons for Mac/Windows
  - System requirements
  - Version info

- [ ] **2.7** Footer
  - Links: About, Privacy, Terms, Support, Contact
  - Social links
  - Newsletter signup

### 3. Admin Dashboard - Core Infrastructure

**TODO**:
- [ ] **3.1** Admin layout
  - Sidebar navigation
  - Header with user info and logout
  - Breadcrumb navigation
  - Dark mode support
  - Responsive design (mobile-friendly)

- [ ] **3.2** Admin authentication
  - Login page for admins
  - Admin role verification from backend
  - Session persistence
  - Logout functionality

- [ ] **3.3** Dashboard home page
  - Key metrics cards (users, posts, engagement)
  - Recent activity feed
  - Quick action buttons
  - System health status indicators

- [ ] **3.4** Admin API routes
  - Proxy routes to backend with admin auth
  - Error handling
  - Logging

### 4. Admin - User Management

**TODO**:
- [ ] **4.1** Users list page (`/admin/users`)
  - Paginated table of users
  - Columns: Avatar, Username, Email, Joined, Followers, Posts, Status
  - Search by username/email
  - Filters: Verified, Private, Suspended, Has 2FA
  - Sort by any column
  - Bulk actions (suspend, verify, export)

- [ ] **4.2** User detail page (`/admin/users/[id]`)
  - Profile summary (avatar, bio, stats)
  - Account info (email, verified, 2FA, created date)
  - Activity timeline (recent posts, follows, comments)
  - Devices/sessions list
  - Reports against this user
  - Action buttons: Reset Password, Verify, Suspend, Delete

- [ ] **4.3** User actions
  - Reset password (send email)
  - Manually verify email
  - Disable 2FA (with confirmation)
  - Set/remove verified badge
  - Suspend account (with reason and duration)
  - Delete account (with confirmation)
  - Export user data (GDPR)

### 5. Admin - Content Management

**TODO**:
- [ ] **5.1** Posts list page (`/admin/content/posts`)
  - Paginated table of posts
  - Columns: User, Preview (audio player), BPM, Key, Genre, Likes, Date, Status
  - Filters: Archived, Has MIDI, Has Project File, Reported
  - Search by user or content
  - Audio player preview inline
  - Bulk actions (archive, delete, feature)

- [ ] **5.2** Post detail page (`/admin/content/posts/[id]`)
  - Audio player (full waveform)
  - Metadata (BPM, key, genre, DAW)
  - Engagement stats (likes, plays, comments, downloads)
  - Comments list (with moderation)
  - Reactions breakdown
  - Remix chain visualization
  - Reports against this post
  - Action buttons: Archive, Delete, Feature

- [ ] **5.3** Comments management (`/admin/content/comments`)
  - List of recent comments
  - Filters: Reported, Deleted
  - Search by content or user
  - Inline moderation (delete, hide)

- [ ] **5.4** Stories management (`/admin/content/stories`)
  - List of active stories
  - Filter: Expired, Reported
  - Preview playback
  - Delete action

### 6. Admin - Moderation Queue

**TODO**:
- [ ] **6.1** Reports queue page (`/admin/moderation`)
  - Priority-sorted list of pending reports
  - Columns: Type, Target, Reporter, Reason, Date, Status
  - Filters: Status (pending, reviewing, resolved), Type (post, comment, user), Reason
  - Quick preview of reported content
  - Batch actions

- [ ] **6.2** Report detail modal/page
  - Full view of reported content (with audio player if post)
  - Reporter info and reason
  - Report history (if multiple reports on same content)
  - Previous moderation actions on target
  - Action buttons: Dismiss, Warn User, Remove Content, Ban User
  - Notes field for moderation reason

- [ ] **6.3** Action workflow
  - Dismiss report (with reason)
  - Warn user (sends notification)
  - Remove content (post/comment)
  - Suspend user (duration picker)
  - Ban user (permanent, with appeal info)
  - All actions logged to audit trail

- [ ] **6.4** Moderation history (`/admin/moderation/history`)
  - Audit log of all moderation actions
  - Filter by moderator, action type, date
  - Export for compliance

- [ ] **6.5** Stream.io integration notice
  - Link to Stream.io moderation dashboard
  - Explanation: "For message and chat moderation, use the Stream.io dashboard"
  - Button: "Open Stream.io Dashboard" (external link)

---

## P1: High Priority - Expected for Professional Product

### 7. Admin - Analytics Dashboard

**TODO**:
- [ ] **7.1** Overview page (`/admin/analytics`)
  - Date range picker (last 7d, 30d, 90d, custom)
  - Key metrics cards with trends
    - Total Users / New Users this period
    - Total Posts / New Posts this period
    - Total Plays / Plays this period
    - DAU / WAU / MAU
  - User growth chart (line chart)
  - Engagement chart (bar chart: likes, comments, plays)
  - Geographic distribution (map, if data available)

- [ ] **7.2** Users analytics (`/admin/analytics/users`)
  - Signups over time (area chart)
  - Retention cohorts (heatmap)
  - User activity distribution
  - Top users by metric (table)
  - DAW usage breakdown (pie chart)
  - Genre preferences (pie chart)

- [ ] **7.3** Content analytics (`/admin/analytics/content`)
  - Posts over time (area chart)
  - Engagement rates over time
  - Top posts by metric (carousel)
  - Genre distribution (pie chart)
  - BPM distribution (histogram)
  - Key distribution (bar chart)
  - MIDI usage stats

- [ ] **7.4** Engagement analytics (`/admin/analytics/engagement`)
  - Likes/comments/plays over time
  - Engagement by hour/day (heatmap)
  - Follow network stats
  - Message volume (if accessible)
  - Story views over time

- [ ] **7.5** Export functionality
  - Export any chart data as CSV
  - Scheduled reports (email)

### 8. Admin - MIDI Challenges Management

**TODO**:
- [ ] **8.1** Challenges list (`/admin/challenges`)
  - Table: Title, Status, Dates, Entries, Winner
  - Filters: Active, Upcoming, Ended
  - Create new challenge button

- [ ] **8.2** Create challenge form
  - Title, description (rich text)
  - Start date, end date, voting end date
  - Constraints builder:
    - BPM range
    - Key requirement
    - Scale restriction
    - Duration limits
    - Note count limits
  - Preview of constraint rules

- [ ] **8.3** Challenge detail page (`/admin/challenges/[id]`)
  - Challenge info and status
  - Entries list with audio preview
  - Vote counts and rankings
  - Actions: Edit, End Early, Announce Winner, Delete

- [ ] **8.4** Challenge notifications
  - Send reminder to participants
  - Announce voting open
  - Announce winner

### 9. Admin - System Monitoring

**TODO**:
- [ ] **9.1** System health page (`/admin/system`)
  - Service status cards (green/yellow/red):
    - API Server
    - Database
    - S3/CDN
    - Stream.io
    - Gorse Recommendations
    - Email Service (SES)
    - WebSocket Server
  - Last checked timestamp
  - Refresh button

- [ ] **9.2** Audio queue monitor
  - Pending jobs count
  - Processing jobs count
  - Failed jobs count
  - Average processing time
  - Queue depth chart over time
  - Failed jobs list with retry option

- [ ] **9.3** WebSocket monitor
  - Active connections count
  - Online users count
  - Messages per minute chart
  - Connection errors

- [ ] **9.4** Error logs viewer
  - Recent errors table
  - Filter by service, level, date
  - Error detail modal
  - Link to full logs (external)

### 10. Public User Profiles

**TODO**:
- [ ] **10.1** Profile page (`/[username]`)
  - Profile header (avatar, display name, bio, stats)
  - Verified badge if applicable
  - Follow button (links to app/plugin)
  - Social links
  - "Open in Sidechain" CTA button

- [ ] **10.2** Posts grid/list
  - User's public posts
  - Audio preview on hover
  - Engagement stats (likes, plays)
  - Pagination or infinite scroll

- [ ] **10.3** Story highlights row
  - Display story highlights (like Instagram)
  - Click to view (modal or page)

- [ ] **10.4** SEO optimization
  - Meta tags (title, description, image)
  - Open Graph tags
  - Twitter Card tags
  - JSON-LD structured data

### 11. Public Post View

**TODO**:
- [ ] **11.1** Post page (`/post/[id]`)
  - Large audio player with waveform
  - Post metadata (BPM, key, genre, DAW)
  - Author info with link to profile
  - Engagement stats
  - Comments (read-only or login to comment)
  - Share buttons
  - "Open in Sidechain" CTA

- [ ] **11.2** Embed support
  - Embeddable player (iframe)
  - oEmbed endpoint
  - Share to social media

---

## P2: Medium Priority - Enhances Experience

### 12. Public Explore/Feed

**TODO**:
- [ ] **12.1** Explore page (`/explore`)
  - Trending posts grid
  - Featured producers
  - Genre filters
  - "Download the plugin to post your own loops"

- [ ] **12.2** Search functionality
  - Search users
  - Search posts (by title, tags)
  - Filters: genre, BPM range, key

- [ ] **12.3** Genre pages (`/explore/[genre]`)
  - Posts filtered by genre
  - Top producers in genre

### 13. Marketing Pages

**TODO**:
- [ ] **13.1** Features page (`/features`)
  - Detailed feature breakdown
  - Screenshots and demos
  - Comparison with other tools

- [ ] **13.2** Download page (`/download`)
  - Platform-specific download buttons
  - System requirements
  - Installation instructions
  - Version history/changelog

- [ ] **13.3** About page (`/about`)
  - Company/team info
  - Mission statement
  - Contact information

- [ ] **13.4** Support/FAQ page (`/support`)
  - FAQ accordion
  - Troubleshooting guides
  - Contact form
  - Link to Discord/community

- [ ] **13.5** Legal pages
  - Privacy Policy (`/privacy`)
  - Terms of Service (`/terms`)
  - DMCA/Copyright (`/copyright`)

### 14. Admin - Advanced Features

**TODO**:
- [ ] **14.1** Feature flags management
  - List of feature flags
  - Toggle on/off
  - Percentage rollout
  - User-specific overrides

- [ ] **14.2** Rate limit configuration
  - View current limits
  - Adjust limits per endpoint
  - IP whitelist/blacklist

- [ ] **14.3** Announcement system
  - Create announcements
  - Target: All users, specific segments
  - Schedule announcements
  - Announcement history

- [ ] **14.4** Admin user management
  - List admin users
  - Add/remove admins
  - Role-based permissions (view-only, moderator, full admin)
  - Activity log per admin

---

## P3: Low Priority - Future Enhancements

### 15. Advanced Features

**TODO**:
- [ ] **15.1** Real-time updates
  - WebSocket connection for admin dashboard
  - Live report notifications
  - Live metrics updates

- [ ] **15.2** Mobile admin app (PWA)
  - Installable PWA
  - Push notifications for reports
  - Quick moderation actions

- [ ] **15.3** API documentation
  - Public API docs page
  - OpenAPI/Swagger integration
  - Interactive API explorer

- [ ] **15.4** Blog/News section
  - Blog posts for updates
  - CMS integration (MDX or headless CMS)

- [ ] **15.5** Localization
  - Multi-language support
  - i18n setup

---

## Stream.io Dashboard Integration

### What Stream.io Provides (Use Their Dashboard)

Stream.io has a comprehensive moderation dashboard at `dashboard.getstream.io` that handles:

**Chat/Messaging Moderation**:
- View and moderate messages across all channels
- Flag inappropriate messages
- Delete messages
- Edit messages
- Block users from sending messages

**Image Moderation**:
- Auto-detection of inappropriate images
- Manual review queue
- Image blocking

**User Management (Chat-level)**:
- Ban users from chat
- Mute users
- Shadow ban
- Timeout

**Channel Management**:
- Create/delete channels
- Manage channel members
- Channel permissions

**Activity Feed Moderation**:
- Remove activities
- Flag activities
- User blocking at feed level

### Our Admin Dashboard Complements Stream.io

**Our Dashboard Handles** (Sidechain-specific):
- User accounts (suspend, verify, password reset)
- Audio posts (delete, archive, feature)
- Comments (our own comment system, not Stream chat)
- Reports (our Report model)
- Analytics (our metrics)
- MIDI challenges (our feature)
- System health (our infrastructure)

**Integration Points**:
- [ ] Add "Open Stream.io Dashboard" button in admin sidebar
- [ ] Add Stream.io status to system health page
- [ ] Document which moderation tasks go where
- [ ] Consider embedding Stream.io iframe (if they support it)

---

## Implementation Plan

### Phase 1: Foundation (Week 1-2)
1. Project setup (Next.js, Tailwind, shadcn)
2. API client and authentication
3. Admin layout and navigation
4. Dashboard home page with basic metrics

### Phase 2: Core Admin (Week 2-3)
1. User management pages
2. Content management pages
3. Basic moderation queue

### Phase 3: Marketing Site (Week 3-4)
1. Landing page
2. Download page
3. Features page

### Phase 4: Public Profiles (Week 4-5)
1. User profile pages
2. Post view pages
3. SEO optimization

### Phase 5: Analytics & Polish (Week 5-6)
1. Analytics dashboard
2. Advanced moderation features
3. System monitoring
4. Mobile responsiveness

### Phase 6: Launch Prep (Week 6-7)
1. Testing and QA
2. Performance optimization
3. Security audit
4. Documentation

---

## Design System

### Colors
```css
/* Primary - Electric Blue */
--primary-500: #3B82F6;
--primary-600: #2563EB;

/* Accent - Purple */
--accent-500: #8B5CF6;

/* Success */
--success-500: #22C55E;

/* Warning */
--warning-500: #F59E0B;

/* Error */
--error-500: #EF4444;

/* Neutrals (Dark mode first) */
--gray-900: #0F172A;
--gray-800: #1E293B;
--gray-700: #334155;
```

### Typography
- Headings: Inter or Plus Jakarta Sans
- Body: Inter
- Monospace: JetBrains Mono (for code/metrics)

### Components
- Use shadcn/ui as base
- Custom audio player component
- Custom waveform visualizer
- Custom charts (Recharts + custom styling)

---

## Security Considerations

- [ ] Admin routes protected by role-based auth
- [ ] CSRF protection on all forms
- [ ] Rate limiting on API routes
- [ ] Input sanitization
- [ ] Content Security Policy headers
- [ ] Audit logging for all admin actions
- [ ] Session timeout for admins
- [ ] 2FA enforcement for admin accounts

---

## Performance Considerations

- [ ] Static generation for marketing pages
- [ ] ISR for public profiles
- [ ] React Query caching
- [ ] Image optimization with next/image
- [ ] Code splitting
- [ ] Bundle analysis
- [ ] Core Web Vitals monitoring

---

## SEO Checklist

- [ ] Semantic HTML
- [ ] Meta tags on all pages
- [ ] Open Graph tags
- [ ] Twitter Card tags
- [ ] JSON-LD structured data
- [ ] Sitemap.xml
- [ ] robots.txt
- [ ] Canonical URLs
- [ ] Alt text on images
- [ ] Fast load times (Core Web Vitals)

---

*This TODO list should be treated as a living document. Update as features are completed and new requirements emerge.*
