# Email Forwarding Setup Guide

This document describes the complete email forwarding system for Sidechain, which automatically forwards emails received at `hi@sidechain.live` to your Gmail account.

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [How It Works](#how-it-works)
3. [DNS Configuration](#dns-configuration)
4. [AWS Infrastructure](#aws-infrastructure)
5. [Deployment](#deployment)
6. [Testing](#testing)
7. [Troubleshooting](#troubleshooting)
8. [Maintenance](#maintenance)
9. [Cost Estimates](#cost-estimates)

## Architecture Overview

The email forwarding system uses AWS services to create a serverless email processing pipeline:

```
External Email (Gmail)
         ↓
    [Internet]
         ↓
SES Inbound (hi@sidechain.live)
         ↓
    [SES Receipt Rule]
         ↓
    ┌───┴────────┐
    ↓            ↓
   S3       SNS Topic
 (Store)   (Notify)
            ↓
        Lambda Function
        (Process & Forward)
            ↓
        SES SendEmail
            ↓
    Gmail Inbox (zach.fogg@gmail.com)
```

### Key Components

| Component | Purpose | Notes |
|-----------|---------|-------|
| **SES Inbound** | Receives emails for hi@sidechain.live | Requires domain verification and MX records |
| **S3 Bucket** | Stores raw email messages | Provides durability and audit trail |
| **SNS Topic** | Notifies Lambda of new emails | Decouples SES from Lambda processing |
| **Lambda Function** | Processes emails and forwards them | Automatically triggered by SNS messages |
| **IAM Roles** | Controls permissions between services | Least-privilege access model |

## How It Works

### Email Reception Flow

1. **Email Arrives**: External sender sends email to `hi@sidechain.live`
2. **MX Record Routes**: Domain's MX records direct the email to AWS SES
3. **SES Receives**: SES validates and accepts the email
4. **Receipt Rule Triggers**: SES receipt rule is activated for `hi@sidechain.live`
5. **Two Actions Execute**:
   - **S3 Action**: Raw email is stored in the S3 bucket (first action, position 1)
   - **SNS Action**: Notification is published to SNS topic (second action, position 2)

### Email Forwarding Flow

1. **SNS Triggers Lambda**: SNS publishes message containing SES receipt information
2. **Lambda Starts**: Lambda function is invoked with the SNS event
3. **Email Retrieved**: Lambda retrieves the raw email from S3 using the message ID
4. **Retry Logic**: If S3 has consistency delays, Lambda retries with exponential backoff
5. **Email Forwarded**: Lambda uses SES SendEmail to forward to `zach.fogg@gmail.com`
6. **Logging**: All operations are logged to CloudWatch for monitoring

## DNS Configuration

### MX Records

For emails to reach your domain via SES, you must configure MX records in your DNS provider (Cloudflare in this case).

#### Required MX Record

| Type | Name | Value | Priority | TTL |
|------|------|-------|----------|-----|
| MX | @ (root) | inbound-smtp.us-west-2.amazonaws.com | 10 | 3600 |

**How to add in Cloudflare:**

1. Go to your Cloudflare dashboard
2. Select domain: `sidechain.live`
3. Navigate to DNS tab
4. Click "Add Record"
5. Select Type: **MX**
6. Name: **@** (or leave blank for root)
7. Mail Server: **inbound-smtp.us-west-2.amazonaws.com**
8. Priority: **10**
9. TTL: **Auto** (or 3600)
10. Click Save

#### Verification

Verify the MX record is properly configured:

```bash
dig MX sidechain.live @asa.ns.cloudflare.com
```

Should return:
```
sidechain.live.		300	IN	MX	10 inbound-smtp.us-west-2.amazonaws.com.
```

### Other DNS Records (Already Configured)

Your domain likely already has these records for email authentication:

- **SPF Record**: `v=spf1 include:amazonses.com ~all`
- **DKIM Records**: Three CNAME records (provided by SES verification)
- **DMARC Record** (optional): Policy for domain reputation

These enable SES to send emails on behalf of your domain.

## AWS Infrastructure

### SES Configuration

#### Verified Domain

The domain `sidechain.live` must be verified in SES:

```bash
aws ses verify-domain-identity --domain sidechain.live --region us-west-2
```

Check verification status:

```bash
aws ses get-identity-verification-attributes --identities sidechain.live --region us-west-2
```

#### Sending Quota

By default, SES accounts are in **Sandbox Mode** with limited sending:
- Max 1 email/second
- Limited to verified addresses only
- Request production access via AWS Console when ready

### S3 Bucket

**Name**: `sidechain-email-receipts-345594574298`

**Purpose**: Stores raw email messages for processing and audit

**Security**:
- Block public access: ✅ Enabled
- Bucket policy: Only SES service can write objects
- Versioning: Disabled (emails are immutable)
- Server-side encryption: Default (AES-256)

**Retention**: No automatic deletion (emails stored indefinitely)

**Access Pattern**:
```
Email object key: {messageId}
Example: j3f07j7ah2g3jj7klfsnugca92kgtb4udgvar4o1
```

### SNS Topic

**Name**: `sidechain-email-receipts`

**Purpose**: Notifies Lambda of new email receipts from SES

**Subscriptions**:
- Lambda function: `sidechain-email-forwarder`

**Policy**: Only SES service can publish messages

### Lambda Function

**Name**: `sidechain-email-forwarder`

**Runtime**: `provided.al2` (custom Go binary)

**Handler**: `email-forwarder` (bootstrap binary name)

**Memory**: 512 MB

**Timeout**: 60 seconds

**Environment Variables**:
```
S3_BUCKET_NAME=sidechain-email-receipts-345594574298
FORWARD_TO_EMAIL=zach.fogg@gmail.com
```

**Code Location**: `backend/cmd/email-forwarder/main.go`

**Key Features**:
- Parses SNS message containing SES receipt
- Retrieves raw email from S3
- Implements retry logic with exponential backoff (up to 3 attempts, 500ms-1500ms delays)
- Forwards email to target address using SES SendEmail
- Logs all operations to CloudWatch

**Logs**: View in CloudWatch under `/aws/lambda/sidechain-email-forwarder`

### IAM Roles and Policies

**Role**: `sidechain-email-forwarder-role`

**Trust Policy**: Allows `lambda.amazonaws.com` to assume the role

**Attached Policies**:

1. **S3 Access** (`email-forwarder-s3-policy`):
   ```json
   {
     "Version": "2012-10-17",
     "Statement": [
       {
         "Effect": "Allow",
         "Action": ["s3:GetObject"],
         "Resource": "arn:aws:s3:::sidechain-email-receipts-345594574298/*"
       },
       {
         "Effect": "Allow",
         "Action": ["s3:ListBucket"],
         "Resource": "arn:aws:s3:::sidechain-email-receipts-345594574298"
       }
     ]
   }
   ```

2. **SES Access** (`email-forwarder-ses-policy`):
   ```json
   {
     "Version": "2012-10-17",
     "Statement": [
       {
         "Effect": "Allow",
         "Action": ["ses:SendEmail", "ses:SendRawEmail"],
         "Resource": "*"
       }
     ]
   }
   ```

3. **CloudWatch Logs** (`email-forwarder-logs-policy`):
   ```json
   {
     "Version": "2012-10-17",
     "Statement": [
       {
         "Effect": "Allow",
         "Action": [
           "logs:CreateLogGroup",
           "logs:CreateLogStream",
           "logs:PutLogEvents"
         ],
         "Resource": "arn:aws:logs:us-west-2:*:*"
       }
     ]
   }
   ```

**Important**: Note that `s3:ListBucket` applies to the bucket ARN, while `s3:GetObject` applies to object ARNs (with `/*`). This is correct and required.

## Deployment

### Prerequisites

- AWS CLI configured with the `zfogg` profile
- Terraform installed (v1.0+)
- Go 1.20+ (for building Lambda)
- Domain MX records configured (see [DNS Configuration](#dns-configuration))

### Infrastructure Deployment

#### 1. Configure Terraform Variables

Edit `terraform/terraform.tfvars`:

```hcl
email_forwarder_enabled = true
forward_to_email        = "zach.fogg@gmail.com"
```

#### 2. Plan the Deployment

```bash
cd terraform
terraform plan -var="email_forwarder_enabled=true"
```

Review the planned changes to verify all resources.

#### 3. Apply the Configuration

```bash
terraform apply -var="email_forwarder_enabled=true"
```

Terraform will create:
- S3 bucket with bucket policy
- SNS topic with topic policy
- Lambda function with environment variables
- Lambda IAM role and policies
- SES receipt rule set
- SES receipt rule for `hi@sidechain.live`

#### 4. Verify Resources Created

```bash
# Check S3 bucket
aws s3 ls sidechain-email-receipts-345594574298 --region us-west-2 --profile zfogg

# Check SNS topic
aws sns get-topic-attributes \
  --topic-arn arn:aws:sns:us-west-2:345594574298:sidechain-email-receipts \
  --region us-west-2 --profile zfogg

# Check Lambda function
aws lambda get-function \
  --function-name sidechain-email-forwarder \
  --region us-west-2 --profile zfogg

# Check SES receipt rule
aws ses describe-active-receipt-rule-set \
  --region us-west-2 --profile zfogg
```

### Lambda Build and Deployment

#### 1. Build the Lambda Binary

```bash
cd backend/cmd/email-forwarder
GOOS=linux GOARCH=amd64 CGO_ENABLED=0 go build -ldflags="-s -w" -o bootstrap main.go
```

**Important Notes**:
- `GOOS=linux GOARCH=amd64`: Lambda runtime is x86_64 Linux
- `CGO_ENABLED=0`: Disable cgo for maximum portability
- `-ldflags="-s -w"`: Strip symbols and DWARF info to reduce binary size
- Output name must be `bootstrap` for custom runtimes

#### 2. Package for Lambda

```bash
zip lambda.zip bootstrap
```

This creates a ~5MB zip file (compressed from ~15MB binary).

#### 3. Deploy to AWS Lambda

```bash
aws lambda update-function-code \
  --function-name sidechain-email-forwarder \
  --zip-file fileb://lambda.zip \
  --region us-west-2 \
  --profile zfogg
```

Or use the Makefile target:

```bash
cd backend
make deploy-email-forwarder
```

#### 4. Verify Deployment

Check that the Lambda function code was updated:

```bash
aws lambda get-function \
  --function-name sidechain-email-forwarder \
  --region us-west-2 --profile zfogg | jq '.Configuration.LastModified'
```

## Testing

### Test 1: Basic Email Forwarding

1. Send email from an external Gmail account (or any email provider) to `hi@sidechain.live`
2. Subject: "Test Email"
3. Body: Any content

**Expected Result**:
- Email appears in `zach.fogg@gmail.com` inbox within 30 seconds
- Subject is prefixed with "Fwd: " (if not already present)
- Email includes forwarded header with original sender and date

### Test 2: Monitor Logs

While email is being processed, watch the Lambda logs:

```bash
aws logs tail /aws/lambda/sidechain-email-forwarder \
  --follow \
  --region us-west-2 \
  --profile zfogg
```

**Expected Log Messages**:
```
2025/12/17 20:21:50 Email forwarder initialized - forwarding to: zach.fogg@gmail.com
2025/12/17 20:21:50 Processing email from: sender@example.com, MessageID: j3f07j7ah2g3jj7klfsnugca92kgtb4udgvar4o1
2025/12/17 20:21:50 Successfully forwarded email from: sender@example.com
```

### Test 3: Check S3 Storage

Verify the raw email was stored in S3:

```bash
aws s3 ls s3://sidechain-email-receipts-345594574298/ \
  --recursive \
  --region us-west-2 \
  --profile zfogg | tail -5
```

You should see the email file listed with the message ID as the key.

### Test 4: Verify Email Headers

Check the forwarded email in Gmail to verify headers:

1. Open forwarded email in Gmail
2. Click "Show original" or "More" → "Show original"
3. Look for:
   - Original `From:` header
   - Original `Date:` header
   - "Forwarded from hi@sidechain.live" in the body

## Troubleshooting

### Email Not Arriving in Inbox

**Symptom**: Sent email to `hi@sidechain.live`, but didn't receive it in Gmail.

**Troubleshooting Steps**:

1. **Check MX Records**:
   ```bash
   dig MX sidechain.live
   ```
   Should show: `inbound-smtp.us-west-2.amazonaws.com` with priority 10

2. **Check SES Receipt Rule**:
   ```bash
   aws ses describe-active-receipt-rule-set \
     --region us-west-2 --profile zfogg
   ```
   Rule should be enabled and have two actions: S3 and SNS (in that order)

3. **Check CloudWatch Logs**:
   ```bash
   aws logs tail /aws/lambda/sidechain-email-forwarder \
     --region us-west-2 --profile zfogg
   ```
   Look for errors like "NoSuchKey" or "AccessDenied"

4. **Check S3 Bucket**:
   ```bash
   aws s3 ls s3://sidechain-email-receipts-345594574298/ \
     --region us-west-2 --profile zfogg
   ```
   The email should be stored here

5. **Check Gmail Spam Folder**: Sometimes forwarded emails go to spam

### Lambda Returning 403 Error

**Symptom**: Logs show `AccessDenied` for S3 GetObject or ListBucket.

**Solution**: Check IAM policy:

```bash
aws iam get-role-policy \
  --role-name sidechain-email-forwarder-role \
  --policy-name email-forwarder-s3-policy \
  --region us-west-2 --profile zfogg
```

Verify:
- `s3:GetObject` action has resource `arn:aws:s3:::sidechain-email-receipts-345594574298/*`
- `s3:ListBucket` action has resource `arn:aws:s3:::sidechain-email-receipts-345594574298` (no `/*`)

### Lambda Returning 404 (NoSuchKey)

**Symptom**: Logs show `NoSuchKey` error from S3 GetObject.

**Possible Causes**:
1. S3 eventual consistency delay (Lambda queried before S3 write completed)
2. Incorrect message ID format

**Solution**: The code includes retry logic that handles this:
- First attempt: Immediate
- Second attempt: After 500ms
- Third attempt: After 1000ms

If still failing after retries, check that SES receipt rule is storing emails in the correct S3 bucket.

### Email Forwarding Not Working (No Logs)

**Symptom**: No Lambda logs at all - Lambda is not being invoked.

**Troubleshooting**:

1. **Check SNS Subscription**:
   ```bash
   aws sns list-subscriptions-by-topic \
     --topic-arn arn:aws:sns:us-west-2:345594574298:sidechain-email-receipts \
     --region us-west-2 --profile zfogg
   ```
   Should show Lambda function as subscriber

2. **Check Lambda Permission**:
   ```bash
   aws lambda get-policy \
     --function-name sidechain-email-forwarder \
     --region us-west-2 --profile zfogg
   ```
   Should allow SNS service to invoke

3. **Check SNS Metrics**:
   ```bash
   aws cloudwatch get-metric-statistics \
     --namespace AWS/SNS \
     --metric-name NumberOfMessagesPublished \
     --dimensions Name=TopicName,Value=sidechain-email-receipts \
     --start-time 2025-12-17T00:00:00Z --end-time 2025-12-17T23:59:59Z \
     --period 300 --statistics Sum \
     --region us-west-2 --profile zfogg
   ```

### Invalid Entry Point Error

**Symptom**: Lambda logs show `exec format error` or `Invalid Entrypoint`.

**Cause**: Lambda binary was built for wrong architecture.

**Solution**: Rebuild with correct flags:

```bash
cd backend/cmd/email-forwarder
GOOS=linux GOARCH=amd64 CGO_ENABLED=0 go build -o bootstrap main.go
zip lambda.zip bootstrap
aws lambda update-function-code \
  --function-name sidechain-email-forwarder \
  --zip-file fileb://lambda.zip \
  --region us-west-2 --profile zfogg
```

### Email Forwarded But With Wrong Formatting

**Symptom**: Email arrives but formatting is broken.

**Possible Causes**:
1. Original email has complex MIME encoding
2. HTML vs plain text mismatch

**Current Implementation**: The `forwardEmail` function sends both HTML and plain text versions, which should work for most clients. Check the HTML body format in `main.go` if needed.

## Maintenance

### Monitoring

#### CloudWatch Dashboard (Recommended)

Create a CloudWatch dashboard to monitor:

```bash
aws cloudwatch put-dashboard \
  --dashboard-name email-forwarder \
  --dashboard-body '{...}'
```

Or via AWS Console: CloudWatch → Dashboards → Create dashboard

#### Key Metrics to Monitor

1. **Lambda Invocations**:
   - Namespace: `AWS/Lambda`
   - Metric: `Invocations`
   - Filter: `FunctionName=sidechain-email-forwarder`

2. **Lambda Errors**:
   - Namespace: `AWS/Lambda`
   - Metric: `Errors`
   - Filter: `FunctionName=sidechain-email-forwarder`

3. **Lambda Duration**:
   - Namespace: `AWS/Lambda`
   - Metric: `Duration`
   - Filter: `FunctionName=sidechain-email-forwarder`

4. **SNS Messages Published**:
   - Namespace: `AWS/SNS`
   - Metric: `NumberOfMessagesPublished`
   - Filter: `TopicName=sidechain-email-receipts`

#### CloudWatch Alarms

Create alarms for critical issues:

```bash
# Alarm on Lambda errors
aws cloudwatch put-metric-alarm \
  --alarm-name email-forwarder-errors \
  --alarm-description "Email forwarder Lambda errors" \
  --metric-name Errors \
  --namespace AWS/Lambda \
  --statistic Sum \
  --period 300 \
  --threshold 1 \
  --comparison-operator GreaterThanOrEqualToThreshold \
  --evaluation-periods 1 \
  --region us-west-2 --profile zfogg
```

### Log Retention

By default, CloudWatch logs are retained indefinitely. To reduce costs, set retention:

```bash
aws logs put-retention-policy \
  --log-group-name /aws/lambda/sidechain-email-forwarder \
  --retention-in-days 30 \
  --region us-west-2 --profile zfogg
```

### S3 Cleanup

Emails in S3 are stored indefinitely. To manage storage costs:

#### Option 1: Set S3 Lifecycle Policy

```bash
# Via Terraform or AWS CLI
aws s3api put-bucket-lifecycle-configuration \
  --bucket sidechain-email-receipts-345594574298 \
  --lifecycle-configuration Rules=[{
    Status=Enabled,
    Prefix='',
    Expiration=Days=90
  }]
```

#### Option 2: Manual Cleanup

```bash
# Delete emails older than 90 days
aws s3 rm s3://sidechain-email-receipts-345594574298/ \
  --recursive \
  --exclude "*" \
  --include "2025-09-*" \
  --region us-west-2 --profile zfogg
```

### Code Updates

To update the Lambda function code:

1. **Edit Code**:
   ```bash
   vim backend/cmd/email-forwarder/main.go
   ```

2. **Rebuild**:
   ```bash
   cd backend/cmd/email-forwarder
   GOOS=linux GOARCH=amd64 CGO_ENABLED=0 go build -ldflags="-s -w" -o bootstrap main.go
   ```

3. **Package**:
   ```bash
   zip lambda.zip bootstrap
   ```

4. **Deploy**:
   ```bash
   aws lambda update-function-code \
     --function-name sidechain-email-forwarder \
     --zip-file fileb://lambda.zip \
     --region us-west-2 --profile zfogg
   ```

5. **Test**: Send a test email and verify it arrives

### Infrastructure Updates

To update infrastructure via Terraform:

1. **Edit Configuration**:
   ```bash
   vim terraform/email_forwarder.tf
   ```

2. **Plan Changes**:
   ```bash
   cd terraform
   terraform plan -var="email_forwarder_enabled=true"
   ```

3. **Review Output**: Ensure no unexpected resource deletions

4. **Apply Changes**:
   ```bash
   terraform apply -var="email_forwarder_enabled=true"
   ```

5. **Verify**: Check AWS Console or CLI to confirm changes

## Cost Estimates

### Monthly Costs (Example)

Assuming 100 emails/month:

| Service | Usage | Cost |
|---------|-------|------|
| **SES** | 100 inbound + 100 outbound = 200 emails | $0.10 (first 62,000 free tier) |
| **Lambda** | 100 invocations × 335ms = 33,500ms | < $0.01 (1M free requests/month) |
| **S3** | 100 emails × 6KB = 600KB stored | < $0.01 (free tier: 5GB) |
| **SNS** | 100 messages published | < $0.01 (free tier: 1M) |
| **CloudWatch Logs** | ~100 invocations × 500 bytes = 50KB | < $0.01 |
| **Total** | | **~$0.15/month** |

### Scaling to High Volume (1000 emails/day)

At production scale with 30,000 emails/month:

| Service | Cost |
|---------|------|
| SES | $15 |
| Lambda | $1 |
| S3 + CloudWatch | $1 |
| **Total** | **~$17/month** |

**Note**: SES sandbox mode limits to 200 emails/day. Request production access for higher volumes.

## References

- [AWS SES Documentation](https://docs.aws.amazon.com/ses/)
- [AWS Lambda Documentation](https://docs.aws.amazon.com/lambda/)
- [AWS S3 Documentation](https://docs.aws.amazon.com/s3/)
- [Terraform AWS Provider](https://registry.terraform.io/providers/hashicorp/aws/latest/docs)
- [SES Receiving Email](https://docs.aws.amazon.com/ses/latest/dg/receiving-email.html)

## Support

For issues or questions:

1. Check CloudWatch logs: `/aws/lambda/sidechain-email-forwarder`
2. Review troubleshooting section above
3. Check AWS service status: https://status.aws.amazon.com/
4. Review code comments in `backend/cmd/email-forwarder/main.go`
