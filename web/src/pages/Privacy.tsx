import { useEffect } from 'react'
import { useNavigate } from 'react-router-dom'
import { Button } from '@/components/ui/button'

export function Privacy() {
  const navigate = useNavigate()

  useEffect(() => {
    window.scrollTo(0, 0)
  }, [])

  return (
    <div className="min-h-screen flex flex-col bg-background">
      {/* Navigation Bar */}
      <nav className="sticky top-0 z-50 border-b border-border bg-background/95 backdrop-blur supports-[backdrop-filter]:bg-background/60">
        <div className="container mx-auto px-4 py-4 flex items-center justify-between">
          <button
            onClick={() => navigate('/login')}
            className="text-xl font-bold text-foreground hover:opacity-80"
          >
            Sidechain
          </button>
          <Button variant="outline" onClick={() => navigate('/login')}>
            Back to Login
          </Button>
        </div>
      </nav>

      {/* Main Content */}
      <div className="flex-1 container mx-auto px-4 py-12 max-w-3xl">
        <article className="prose prose-invert max-w-none">
          <h1 className="text-4xl font-bold mb-2">Privacy Policy</h1>
          <p className="text-muted-foreground mb-8">
            Last Updated: {new Date().getFullYear()}
          </p>

          <section className="mb-8">
            <h2 className="text-2xl font-semibold mt-8 mb-4">1. Introduction</h2>
            <p className="mb-4">
              Sidechain ("we", "us", "our", or "Company") operates the Sidechain application and website.
              This page informs you of our policies regarding the collection, use, and disclosure of
              personal data when you use our Service and the choices you have associated with that data.
            </p>
          </section>

          <section className="mb-8">
            <h2 className="text-2xl font-semibold mt-8 mb-4">2. Information Collection and Use</h2>
            <p className="mb-4">We collect several different types of information for various purposes to provide and improve our Service to you.</p>

            <h3 className="text-xl font-semibold mt-6 mb-3">2.1 Account Information</h3>
            <p className="mb-4">
              When you create an account, we collect information such as:
            </p>
            <ul className="list-disc list-inside mb-4 space-y-2">
              <li>Email address</li>
              <li>Username</li>
              <li>Password (hashed and encrypted)</li>
              <li>Profile information (avatar, bio, etc.)</li>
            </ul>

            <h3 className="text-xl font-semibold mt-6 mb-3">2.2 Audio Content</h3>
            <p className="mb-4">
              Audio files and project files you upload to Sidechain are stored on our secure servers
              and CDN. We retain these files according to your account settings and deletion preferences.
              You retain all ownership rights to your audio content.
            </p>

            <h3 className="text-xl font-semibold mt-6 mb-3">2.3 Usage Data</h3>
            <p className="mb-4">
              We may collect information about how you interact with our Service, including:
            </p>
            <ul className="list-disc list-inside mb-4 space-y-2">
              <li>Device information (hardware model, operating system)</li>
              <li>Browsing history and content interactions</li>
              <li>Search queries</li>
              <li>IP address and approximate location</li>
              <li>DAW usage information (application name, version)</li>
            </ul>

            <h3 className="text-xl font-semibold mt-6 mb-3">2.4 Communication Data</h3>
            <p className="mb-4">
              Messages, comments, and interactions you share on the platform are stored to facilitate
              social features and communication between users.
            </p>
          </section>

          <section className="mb-8">
            <h2 className="text-2xl font-semibold mt-8 mb-4">3. Use of Data</h2>
            <p className="mb-4">Sidechain uses the collected data for various purposes:</p>
            <ul className="list-disc list-inside mb-4 space-y-2">
              <li>To provide and maintain our Service</li>
              <li>To notify you about changes to our Service</li>
              <li>To provide customer support and respond to your inquiries</li>
              <li>To gather analysis or valuable information so that we can improve our Service</li>
              <li>To monitor the usage of our Service</li>
              <li>To detect, prevent and address fraud and security issues</li>
              <li>To comply with legal obligations</li>
            </ul>
          </section>

          <section className="mb-8">
            <h2 className="text-2xl font-semibold mt-8 mb-4">4. Security of Data</h2>
            <p className="mb-4">
              The security of your data is important to us, but remember that no method of transmission
              over the Internet or method of electronic storage is 100% secure. While we strive to use
              commercially acceptable means to protect your Personal Data, we cannot guarantee its
              absolute security.
            </p>
            <p className="mb-4">
              We implement industry-standard security measures including:
            </p>
            <ul className="list-disc list-inside mb-4 space-y-2">
              <li>Encrypted transmission (HTTPS)</li>
              <li>Secure password hashing</li>
              <li>Access controls and authentication</li>
              <li>Regular security audits</li>
            </ul>
          </section>

          <section className="mb-8">
            <h2 className="text-2xl font-semibold mt-8 mb-4">5. Third-Party Services</h2>
            <p className="mb-4">
              We use third-party services to provide certain functionality:
            </p>
            <ul className="list-disc list-inside mb-4 space-y-2">
              <li><strong>Stream.io</strong> - For social features and messaging</li>
              <li><strong>Audio CDN</strong> - For audio file storage and delivery</li>
              <li><strong>Authentication Services</strong> - For account management</li>
            </ul>
            <p className="mb-4">
              These third parties may collect and use your information in accordance with their own
              privacy policies. We encourage you to review their privacy policies before using their
              services.
            </p>
          </section>

          <section className="mb-8">
            <h2 className="text-2xl font-semibold mt-8 mb-4">6. Your Rights</h2>
            <p className="mb-4">
              You have the right to:
            </p>
            <ul className="list-disc list-inside mb-4 space-y-2">
              <li>Access the personal data we hold about you</li>
              <li>Correct inaccurate personal data</li>
              <li>Request deletion of your personal data</li>
              <li>Withdraw consent at any time</li>
              <li>Export your data in a portable format</li>
            </ul>
            <p className="mb-4">
              To exercise these rights, please contact us at privacy@sidechain.app
            </p>
          </section>

          <section className="mb-8">
            <h2 className="text-2xl font-semibold mt-8 mb-4">7. Data Retention</h2>
            <p className="mb-4">
              We retain personal data for as long as necessary to provide our Services or as required
              by law. You can request deletion of your account and associated data at any time through
              your account settings.
            </p>
          </section>

          <section className="mb-8">
            <h2 className="text-2xl font-semibold mt-8 mb-4">8. Changes to This Privacy Policy</h2>
            <p className="mb-4">
              We may update our Privacy Policy from time to time. We will notify you of any changes by
              posting the new Privacy Policy on this page and updating the "Last Updated" date at the
              top of this Privacy Policy.
            </p>
          </section>

          <section className="mb-8">
            <h2 className="text-2xl font-semibold mt-8 mb-4">9. Contact Us</h2>
            <p className="mb-4">
              If you have any questions about this Privacy Policy, please contact us at:
            </p>
            <div className="bg-secondary p-4 rounded-lg">
              <p className="mb-2"><strong>Email:</strong> privacy@sidechain.app</p>
              <p><strong>Support:</strong> support@sidechain.app</p>
            </div>
          </section>
        </article>
      </div>

      {/* Footer */}
      <div className="border-t border-border mt-12">
        <div className="container mx-auto px-4 py-6 text-center text-muted-foreground">
          <p>&copy; {new Date().getFullYear()} Sidechain. All rights reserved.</p>
        </div>
      </div>
    </div>
  )
}
