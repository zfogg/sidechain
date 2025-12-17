# Email Forwarder Lambda Configuration
# This Lambda function receives emails sent to hi@sidechain.live and forwards them to zach.fogg@gmail.com

variable "email_forwarder_enabled" {
  description = "Enable email forwarder Lambda"
  type        = bool
  default     = false
}

variable "forward_to_email" {
  description = "Email address to forward messages to"
  type        = string
  default     = "zach.fogg@gmail.com"
}

variable "receive_email_domain" {
  description = "Domain to receive emails on"
  type        = string
  default     = "sidechain.live"
}

variable "email_bucket_name" {
  description = "S3 bucket name for storing received emails"
  type        = string
  default     = "sidechain-email-receipts-345594574298"
}

# S3 bucket for storing received emails
resource "aws_s3_bucket" "email_receipts" {
  count  = var.email_forwarder_enabled ? 1 : 0
  bucket = var.email_bucket_name

  tags = {
    Name        = "Sidechain Email Receipts"
    Environment = "production"
    Purpose     = "SES email forwarding"
  }
}

# Block public access to email bucket
resource "aws_s3_bucket_public_access_block" "email_receipts" {
  count  = var.email_forwarder_enabled ? 1 : 0
  bucket = aws_s3_bucket.email_receipts[0].id

  block_public_acls       = true
  block_public_policy     = true
  ignore_public_acls      = true
  restrict_public_buckets = true
}

# Bucket policy to allow SES to put objects
resource "aws_s3_bucket_policy" "email_receipts" {
  count  = var.email_forwarder_enabled ? 1 : 0
  bucket = aws_s3_bucket.email_receipts[0].id

  policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Sid    = "AllowSESPutObject"
        Effect = "Allow"
        Principal = {
          Service = "ses.amazonaws.com"
        }
        Action   = "s3:PutObject"
        Resource = "${aws_s3_bucket.email_receipts[0].arn}/*"
        Condition = {
          StringEquals = {
            "aws:SourceAccount" = data.aws_caller_identity.current.account_id
          }
        }
      }
    ]
  })
}

# Get current AWS account ID
data "aws_caller_identity" "current" {}

# IAM role for Lambda function
resource "aws_iam_role" "email_forwarder" {
  count = var.email_forwarder_enabled ? 1 : 0
  name  = "sidechain-email-forwarder-role"

  assume_role_policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Action = "sts:AssumeRole"
        Effect = "Allow"
        Principal = {
          Service = "lambda.amazonaws.com"
        }
      }
    ]
  })

  tags = {
    Name        = "Sidechain Email Forwarder Role"
    Environment = "production"
  }
}

# IAM policy for Lambda to read from S3
resource "aws_iam_role_policy" "email_forwarder_s3" {
  count  = var.email_forwarder_enabled ? 1 : 0
  name   = "email-forwarder-s3-policy"
  role   = aws_iam_role.email_forwarder[0].id
  policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Effect = "Allow"
        Action = [
          "s3:GetObject"
        ]
        Resource = "${aws_s3_bucket.email_receipts[0].arn}/*"
      }
    ]
  })
}

# IAM policy for Lambda to send emails via SES
resource "aws_iam_role_policy" "email_forwarder_ses" {
  count  = var.email_forwarder_enabled ? 1 : 0
  name   = "email-forwarder-ses-policy"
  role   = aws_iam_role.email_forwarder[0].id
  policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Effect = "Allow"
        Action = [
          "ses:SendEmail",
          "ses:SendRawEmail"
        ]
        Resource = "*"
      }
    ]
  })
}

# IAM policy for CloudWatch Logs
resource "aws_iam_role_policy" "email_forwarder_logs" {
  count  = var.email_forwarder_enabled ? 1 : 0
  name   = "email-forwarder-logs-policy"
  role   = aws_iam_role.email_forwarder[0].id
  policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Effect = "Allow"
        Action = [
          "logs:CreateLogGroup",
          "logs:CreateLogStream",
          "logs:PutLogEvents"
        ]
        Resource = "arn:aws:logs:us-west-2:*:*"
      }
    ]
  })
}

# Lambda function for email forwarding
# Note: This requires the Lambda code to be built and packaged first
# See the deployment instructions in the README
resource "aws_lambda_function" "email_forwarder" {
  count            = var.email_forwarder_enabled ? 1 : 0
  filename         = "${path.module}/../backend/cmd/email-forwarder/lambda.zip"
  function_name    = "sidechain-email-forwarder"
  role             = aws_iam_role.email_forwarder[0].arn
  handler          = "email-forwarder"
  runtime          = "provided.al2"
  timeout          = 60
  memory_size      = 512

  environment {
    variables = {
      S3_BUCKET_NAME    = aws_s3_bucket.email_receipts[0].id
      FORWARD_TO_EMAIL  = var.forward_to_email
    }
  }

  depends_on = [
    aws_iam_role_policy.email_forwarder_s3,
    aws_iam_role_policy.email_forwarder_ses,
    aws_iam_role_policy.email_forwarder_logs
  ]

  tags = {
    Name        = "Sidechain Email Forwarder"
    Environment = "production"
  }
}

# Lambda permission for SNS to invoke
resource "aws_lambda_permission" "email_forwarder_sns" {
  count         = var.email_forwarder_enabled ? 1 : 0
  statement_id  = "AllowExecutionFromSNS"
  action        = "lambda:InvokeFunction"
  function_name = aws_lambda_function.email_forwarder[0].function_name
  principal     = "sns.amazonaws.com"
  source_arn    = aws_sns_topic.email_receipts[0].arn
}

# SNS topic for SES receipt notifications
resource "aws_sns_topic" "email_receipts" {
  count = var.email_forwarder_enabled ? 1 : 0
  name  = "sidechain-email-receipts"

  tags = {
    Name        = "Sidechain Email Receipts"
    Environment = "production"
  }
}

# SNS topic policy to allow SES to publish
resource "aws_sns_topic_policy" "email_receipts" {
  count  = var.email_forwarder_enabled ? 1 : 0
  arn    = aws_sns_topic.email_receipts[0].arn
  policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Sid    = "AllowSESPublish"
        Effect = "Allow"
        Principal = {
          Service = "ses.amazonaws.com"
        }
        Action   = "SNS:Publish"
        Resource = aws_sns_topic.email_receipts[0].arn
        Condition = {
          StringEquals = {
            "aws:SourceAccount" = data.aws_caller_identity.current.account_id
          }
        }
      }
    ]
  })
}

# SNS subscription to Lambda
resource "aws_sns_topic_subscription" "email_forwarder" {
  count     = var.email_forwarder_enabled ? 1 : 0
  topic_arn = aws_sns_topic.email_receipts[0].arn
  protocol  = "lambda"
  endpoint  = aws_lambda_function.email_forwarder[0].arn
}

# SES receipt rule set
resource "aws_ses_receipt_rule_set" "main" {
  count         = var.email_forwarder_enabled ? 1 : 0
  rule_set_name = "sidechain-email-forwarder"
}

# Make it the active rule set
resource "aws_ses_active_receipt_rule_set" "main" {
  count         = var.email_forwarder_enabled ? 1 : 0
  rule_set_name = aws_ses_receipt_rule_set.main[0].rule_set_name
}

# SES receipt rule to forward hi@sidechain.live
resource "aws_ses_receipt_rule" "email_forwarder" {
  count = var.email_forwarder_enabled ? 1 : 0

  name          = "forward-hi-sidechain"
  rule_set_name = aws_ses_receipt_rule_set.main[0].rule_set_name
  enabled       = true
  scan_enabled  = true

  recipients = ["hi@${var.receive_email_domain}"]

  # Store email in S3
  s3_action {
    position          = 1
    bucket_name       = aws_s3_bucket.email_receipts[0].id
    object_key_prefix = ""
  }

  # Send notification to SNS
  sns_action {
    position  = 2
    topic_arn = aws_sns_topic.email_receipts[0].arn
  }

  depends_on = [
    aws_s3_bucket_policy.email_receipts,
    aws_sns_topic_policy.email_receipts,
    aws_ses_active_receipt_rule_set.main
  ]
}

# Outputs
output "email_forwarder_lambda_name" {
  description = "Name of the email forwarder Lambda function"
  value       = try(aws_lambda_function.email_forwarder[0].function_name, null)
}

output "email_receipts_bucket" {
  description = "S3 bucket name for email receipts"
  value       = try(aws_s3_bucket.email_receipts[0].id, null)
}

output "sns_topic_arn" {
  description = "SNS topic ARN for email receipt notifications"
  value       = try(aws_sns_topic.email_receipts[0].arn, null)
}
