import { apiClient } from './client'
import { Outcome } from './types'

export type ReportType = 'post' | 'comment' | 'user'

export type ReportReason =
  | 'spam'
  | 'harassment'
  | 'hate_speech'
  | 'misleading'
  | 'copyright'
  | 'inappropriate_content'
  | 'other'

/**
 * ReportClient - Handle user reports for posts, comments, and users
 *
 * API Endpoints:
 * POST /reports - Submit a new report
 * GET /reports - Get user's submitted reports (admin only)
 * GET /reports/:id - Get report details (admin only)
 * PATCH /reports/:id - Update report status (admin only)
 */
export class ReportClient {
  /**
   * Submit a report for a post, comment, or user
   */
  static async submitReport(
    type: ReportType,
    targetId: string,
    reason: ReportReason,
    description?: string
  ): Promise<Outcome<{ reportId: string }>> {
    const result = await apiClient.post<{ report_id: string }>('/reports', {
      type,
      target_id: targetId,
      reason,
      description: description?.trim(),
    })

    if (result.isError()) {
      return Outcome.error(result.getError())
    }

    return Outcome.ok({
      reportId: result.getValue().report_id,
    })
  }

  /**
   * Get reason options for reporting UI
   */
  static getReportReasons(): { value: ReportReason; label: string; description: string }[] {
    return [
      {
        value: 'spam',
        label: 'Spam',
        description: 'Repeated unwanted content or advertising',
      },
      {
        value: 'harassment',
        label: 'Harassment',
        description: 'Bullying, threatening, or insulting behavior',
      },
      {
        value: 'hate_speech',
        label: 'Hate Speech',
        description: 'Content targeting people based on protected characteristics',
      },
      {
        value: 'misleading',
        label: 'Misleading',
        description: 'False or misleading information',
      },
      {
        value: 'copyright',
        label: 'Copyright Violation',
        description: 'Unauthorized use of copyrighted material',
      },
      {
        value: 'inappropriate_content',
        label: 'Inappropriate Content',
        description: 'Explicit, graphic, or otherwise inappropriate content',
      },
      {
        value: 'other',
        label: 'Other',
        description: 'Other reason (please describe)',
      },
    ]
  }
}
