# Email Forwarding Setup Guide

This guide walks you through setting up email forwarding for `hi@sidechain.live` to `zach.fogg@gmail.com` using AWS SES, Lambda, and S3.

## What's Been Created

### 1. Lambda Function Code
**Location**: `backend/cmd/email-forwarder/main.go`

The Lambda function:
- Receives SNS notifications from SES when emails are received
- Downloads the raw email from S3
- Forwards it to the target email address via SES
- Logs all activity for debugging

### 2. Infrastructure as Code (Terraform)
**Location**: `terraform/email_forwarder.tf`

Terraform configuration includes:
- S3 bucket for storing received emails
- SNS topic for SES notifications
- Lambda function with appropriate IAM roles and permissions
- SES receipt rules to trigger the forwarding pipeline
- All necessary security policies

### 3. Build Scripts
**Location**: `backend/Makefile`

Added make targets:
- `make build-email-forwarder` - Build the Lambda binary
- `make package-email-forwarder` - Package into lambda.zip
- `make deploy-email-forwarder` - Deploy to AWS
- `make clean-email-forwarder` - Clean build artifacts

### 4. Documentation
**Location**: `backend/cmd/email-forwarder/README.md`

Comprehensive guide including:
- Architecture overview
- Manual deployment steps
- Terraform deployment instructions
- Configuration guide
- Testing procedures
- Troubleshooting help

## Quick Start

### Option A: Automated Deployment with Terraform (Recommended)

```bash
# 1. Enable email forwarder in Terraform
cd terraform
echo 'email_forwarder_enabled = true' >> terraform.tfvars
echo 'forward_to_email = "zach.fogg@gmail.com"' >> terraform.tfvars

# 2. Build the Lambda function
cd ../backend
make package-email-forwarder

# 3. Deploy infrastructure and Lambda
cd ../terraform
terraform init
terraform plan
terraform apply
```

### Option B: Manual Deployment

See `backend/cmd/email-forwarder/README.md` for step-by-step manual deployment instructions.

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                    Email Flow                               │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  Email → hi@sidechain.live                                 │
│    ↓                                                         │
│  AWS SES receives and processes email                       │
│    ↓                                                         │
│  Email stored in S3 bucket                                 │
│  SNS notification sent                                      │
│    ↓                                                         │
│  Lambda triggered by SNS                                    │
│    ↓                                                         │
│  Lambda retrieves email from S3                             │
│    ↓                                                         │
│  Lambda forwards via SES                                    │
│    ↓                                                         │
│  Email delivered to zach.fogg@gmail.com                     │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

## Configuration

### Environment Variables

The Lambda function requires:
- `S3_BUCKET_NAME`: Where received emails are stored
- `FORWARD_TO_EMAIL`: Target email address (default: zach.fogg@gmail.com)

### SES Configuration

After deployment, ensure:
1. Domain `sidechain.live` is verified in SES
2. Email address `zach.fogg@gmail.com` is verified in SES (or move out of sandbox)
3. SES receipt rule is active and properly configured

## Testing

Send a test email to `hi@sidechain.live`:

```bash
aws ses send-email \
  --from noreply@sidechain.live \
  --destination ToAddresses=hi@sidechain.live \
  --message Subject={Data="Test Email"},Body={Text={Data="Testing email forwarding"}} \
  --region us-west-2 --profile zfogg
```

Check Lambda logs:

```bash
aws logs tail /aws/lambda/sidechain-email-forwarder --region us-west-2 --follow
```

## Cost Estimation

Monthly costs (estimated at 100 emails/month):
- SES incoming: ~$0.15 (Free tier includes 62,000 emails/month)
- SES outgoing: ~$0.15
- S3 storage: <$0.01 (minimal)
- Lambda: <$0.01 (free tier includes 1M invocations)
- SNS: <$0.01

**Total: ~$0.30/month** (practically free due to AWS free tier)

## Troubleshooting

### Emails not forwarding?

1. **Check SES sending limits**: Might be in sandbox mode
   ```bash
   aws ses get-account-sending-enabled --region us-west-2
   ```

2. **Check Lambda logs**:
   ```bash
   aws logs tail /aws/lambda/sidechain-email-forwarder --region us-west-2
   ```

3. **Verify S3 bucket has emails**:
   ```bash
   aws s3 ls sidechain-email-receipts/ --recursive
   ```

4. **Check SES receipt rules**:
   ```bash
   aws ses describe-receipt-rule-set --rule-set-name sidechain-email-forwarder --region us-west-2
   ```

### Lambda permission errors?

- Ensure Lambda role has S3 GetObject permission
- Ensure Lambda role has SES SendEmail permission
- Check SNS subscription to Lambda

For detailed troubleshooting, see `backend/cmd/email-forwarder/README.md`.

## Files Modified

- ✅ `backend/cmd/email-forwarder/main.go` - New Lambda function
- ✅ `backend/cmd/email-forwarder/README.md` - Comprehensive documentation
- ✅ `backend/Makefile` - Added email-forwarder build targets
- ✅ `terraform/email_forwarder.tf` - Infrastructure as Code

## Next Steps

1. Review the code in `backend/cmd/email-forwarder/main.go`
2. Read the full documentation in `backend/cmd/email-forwarder/README.md`
3. Choose deployment method (Terraform or manual)
4. Build and deploy: `make package-email-forwarder`
5. Test by sending an email to `hi@sidechain.live`
6. Monitor logs in CloudWatch

## Support

For issues or questions:
1. Check `backend/cmd/email-forwarder/README.md` for troubleshooting
2. Review CloudWatch Logs: `/aws/lambda/sidechain-email-forwarder`
3. Verify SES receipt rules are active
4. Ensure all IAM permissions are correctly assigned
