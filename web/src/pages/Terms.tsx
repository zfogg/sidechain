import { useEffect } from 'react'
import { useNavigate } from 'react-router-dom'
import { Button } from '@/components/ui/button'

export function Terms() {
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
          <h1 className="text-4xl font-bold mb-2">Terms of Service</h1>
          <p className="text-muted-foreground mb-8">
            Last Updated: {new Date().getFullYear()}
          </p>

          <section className="mb-8">
            <h2 className="text-2xl font-semibold mt-8 mb-4">1. Acceptance of Terms</h2>
            <p className="mb-4">
              By accessing and using Sidechain ("the Service"), you accept and agree to be bound by
              the terms and provision of this agreement. If you do not agree to abide by the above,
              please do not use this service.
            </p>
          </section>

          <section className="mb-8">
            <h2 className="text-2xl font-semibold mt-8 mb-4">2. Use License</h2>
            <p className="mb-4">
              Permission is granted to temporarily download one copy of the materials (information or
              software) on Sidechain for personal, non-commercial transitory viewing only. This is the
              grant of a license, not a transfer of title, and under this license you may not:
            </p>
            <ul className="list-disc list-inside mb-4 space-y-2">
              <li>Modifying or copying the materials</li>
              <li>Using the materials for any commercial purpose or for any public display</li>
              <li>Attempting to decompile or reverse engineer any software contained on the Service</li>
              <li>Removing any copyright or other proprietary notations from the materials</li>
              <li>Transferring the materials to another person or "mirroring" the materials on any other server</li>
              <li>Violating any applicable laws or regulations</li>
              <li>Harassing or causing distress or inconvenience to any person</li>
            </ul>
          </section>

          <section className="mb-8">
            <h2 className="text-2xl font-semibold mt-8 mb-4">3. Disclaimer</h2>
            <p className="mb-4">
              The materials on Sidechain's website are provided on an 'as is' basis. Sidechain makes no
              warranties, expressed or implied, and hereby disclaims and negates all other warranties
              including, without limitation, implied warranties or conditions of merchantability, fitness
              for a particular purpose, or non-infringement of intellectual property or other violation
              of rights.
            </p>
          </section>

          <section className="mb-8">
            <h2 className="text-2xl font-semibold mt-8 mb-4">4. Limitations</h2>
            <p className="mb-4">
              In no event shall Sidechain or its suppliers be liable for any damages (including, without
              limitation, damages for loss of data or profit, or due to business interruption) arising out
              of the use or inability to use the materials on Sidechain's website, even if Sidechain or a
              Sidechain authorized representative has been notified orally or in writing of the possibility
              of such damage.
            </p>
          </section>

          <section className="mb-8">
            <h2 className="text-2xl font-semibold mt-8 mb-4">5. Accuracy of Materials</h2>
            <p className="mb-4">
              The materials appearing on Sidechain's website could include technical, typographical, or
              photographic errors. Sidechain does not warrant that any of the materials on its website are
              accurate, complete, or current. Sidechain may make changes to the materials contained on its
              website at any time without notice.
            </p>
          </section>

          <section className="mb-8">
            <h2 className="text-2xl font-semibold mt-8 mb-4">6. Links</h2>
            <p className="mb-4">
              Sidechain has not reviewed all of the sites linked to its website and is not responsible for
              the contents of any such linked site. The inclusion of any link does not imply endorsement by
              Sidechain of the site. Use of any such linked website is at the user's own risk.
            </p>
          </section>

          <section className="mb-8">
            <h2 className="text-2xl font-semibold mt-8 mb-4">7. Modifications</h2>
            <p className="mb-4">
              Sidechain may revise these terms of service for its website at any time without notice. By
              using this website, you are agreeing to be bound by the then current version of these terms
              of service.
            </p>
          </section>

          <section className="mb-8">
            <h2 className="text-2xl font-semibold mt-8 mb-4">8. User Content</h2>
            <p className="mb-4">
              You retain all rights to any content you submit, post, or display on or through the Service
              ("User Content"). By submitting User Content to Sidechain, you grant Sidechain a worldwide,
              non-exclusive, royalty-free license to use, reproduce, adapt, publish, and distribute your
              User Content in connection with providing and promoting the Service.
            </p>

            <h3 className="text-xl font-semibold mt-6 mb-3">8.1 Audio Content Ownership</h3>
            <p className="mb-4">
              You acknowledge that you own or have obtained proper rights to all audio content, project files,
              and other materials you upload to Sidechain. You are solely responsible for ensuring that your
              uploads do not infringe upon any third-party intellectual property rights.
            </p>

            <h3 className="text-xl font-semibold mt-6 mb-3">8.2 Prohibited Content</h3>
            <p className="mb-4">
              You agree not to upload, post, or share content that:
            </p>
            <ul className="list-disc list-inside mb-4 space-y-2">
              <li>Infringes upon copyright, trademark, or other intellectual property rights</li>
              <li>Contains obscene, explicit, or offensive material</li>
              <li>Harasses, threatens, or abuses others</li>
              <li>Contains malware, viruses, or other harmful code</li>
              <li>Violates any applicable laws or regulations</li>
              <li>Impersonates another person or entity</li>
            </ul>
          </section>

          <section className="mb-8">
            <h2 className="text-2xl font-semibold mt-8 mb-4">9. Account Responsibility</h2>
            <p className="mb-4">
              You are responsible for maintaining the confidentiality of your account information and
              password. You agree to accept responsibility for all activities that occur under your account.
              You must notify Sidechain immediately of any unauthorized use of your account.
            </p>
          </section>

          <section className="mb-8">
            <h2 className="text-2xl font-semibold mt-8 mb-4">10. Governing Law</h2>
            <p className="mb-4">
              These terms and conditions of use are governed by and construed in accordance with the laws
              of the United States, and you irrevocably submit to the exclusive jurisdiction of the courts
              located in the United States.
            </p>
          </section>

          <section className="mb-8">
            <h2 className="text-2xl font-semibold mt-8 mb-4">11. Termination</h2>
            <p className="mb-4">
              Sidechain may terminate your account and access to the Service at any time, for any reason,
              with or without notice. Upon termination, your right to use the Service will immediately cease.
            </p>
          </section>

          <section className="mb-8">
            <h2 className="text-2xl font-semibold mt-8 mb-4">12. Acceptable Use Policy</h2>
            <p className="mb-4">
              You agree to use Sidechain only for lawful purposes and in a way that does not infringe upon
              the rights of others or restrict their use and enjoyment of the Service. Prohibited behavior
              includes:
            </p>
            <ul className="list-disc list-inside mb-4 space-y-2">
              <li>Harassment or causing distress or inconvenience to any person</li>
              <li>Obscene or abusive content</li>
              <li>Disrupting the normal flow of dialogue within Sidechain's communities</li>
              <li>Spam and unsolicited advertising</li>
              <li>Attempting to gain unauthorized access to systems or networks</li>
              <li>Any form of fraud or deception</li>
            </ul>
          </section>

          <section className="mb-8">
            <h2 className="text-2xl font-semibold mt-8 mb-4">13. Intellectual Property Rights</h2>
            <p className="mb-4">
              All content included on the Service, such as text, graphics, logos, images, audio clips, and
              software, is the property of Sidechain or its content suppliers and is protected by international
              copyright laws. The compilation (meaning the collection, arrangement, and assembly) of all content
              on the Service is the exclusive property of Sidechain.
            </p>
          </section>

          <section className="mb-8">
            <h2 className="text-2xl font-semibold mt-8 mb-4">14. Limitation of Liability</h2>
            <p className="mb-4">
              In no case shall Sidechain, its directors, officers, employees, or agents be liable to you for
              any indirect, incidental, special, or consequential damages arising out of or in connection with
              your use of the Service, even if Sidechain has been advised of the possibility of such damages.
            </p>
          </section>

          <section className="mb-8">
            <h2 className="text-2xl font-semibold mt-8 mb-4">15. Contact Information</h2>
            <p className="mb-4">
              If you have any questions about these Terms of Service, please contact us at:
            </p>
            <div className="bg-secondary p-4 rounded-lg">
              <p className="mb-2"><strong>Email:</strong> legal@sidechain.app</p>
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
